//---------------------------------------------------------------------------------------------------------------------
//  @file   cloth_solver.cpp
//  @author Julius
//  @date   5 Oct, 2026
//
//  @copyright
//  Copyright (C)  2026 Seamly, LLC
//  https://github.com/fashionfreedom/seamly2d
//
//  @brief
//  Seamly2D is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Seamly2D is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------------------------------------------------

#include "cloth_solver.h"

#include <QHash>
#include <QLineF>
#include <QtConcurrent/QtConcurrentMap>
#include <QtMath>

#include <algorithm>
#include <limits>
#include <numeric>

namespace
{
// A vertex moves at most this far, in cm, in one sweep, so a bad start can't throw it across the scene.
const double max_move = 2.0;

// Below this, in cm, a tangential move is taken as resting, so friction can hold the cloth still.
const double friction_rest = 0.01;

// Body triangles this much further away than the cloth's thickness, in cm, are watched for contact. They are only
// looked up again once a vertex has used up half of that margin moving, which saves most lookups.
const double contact_margin = 2.0;

// Colours with at least this many vertices are solved on several threads; for fewer, handing out the work costs
// more than it saves.
const int parallel_colour = 1024;

// Determinants smaller than this leave a vertex where it is, its forces don't say where to go.
const double singular = 1e-12;

// No vertex gets less mass than this, in g, so loose bits of mesh don't make the solve stiff.
const double min_mass = 1e-4;

//---------------------------------------------------------------------------------------------------------------------
struct Vec3
{
    double x = 0;
    double y = 0;
    double z = 0;

    Vec3 operator+(const Vec3& other) const
    {
        return {x + other.x, y + other.y, z + other.z};
    }

    Vec3 operator-(const Vec3& other) const
    {
        return {x - other.x, y - other.y, z - other.z};
    }

    Vec3 operator*(double factor) const
    {
        return {x * factor, y * factor, z * factor};
    }

    Vec3& operator+=(const Vec3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    double dot(const Vec3& other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }

    double length() const
    {
        return qSqrt(dot(*this));
    }
};

//---------------------------------------------------------------------------------------------------------------------
// A symmetric 3 x 3 matrix, enough for one vertex's Hessian.
struct Mat3
{
    double xx = 0;
    double xy = 0;
    double xz = 0;
    double yy = 0;
    double yz = 0;
    double zz = 0;

    void addIdentity(double factor)
    {
        xx += factor;
        yy += factor;
        zz += factor;
    }

    void addOuter(const Vec3& v, double factor)
    {
        xx += factor * v.x * v.x;
        xy += factor * v.x * v.y;
        xz += factor * v.x * v.z;
        yy += factor * v.y * v.y;
        yz += factor * v.y * v.z;
        zz += factor * v.z * v.z;
    }

    // Solves M x = b; false if M is singular.
    bool solve(const Vec3& b, Vec3* x) const
    {
        const double c_xx = yy * zz - yz * yz;
        const double c_xy = xz * yz - xy * zz;
        const double c_xz = xy * yz - xz * yy;
        const double determinant = xx * c_xx + xy * c_xy + xz * c_xz;
        if (qAbs(determinant) < singular)
        {
            return false;
        }
        const double c_yy = xx * zz - xz * xz;
        const double c_yz = xy * xz - xx * yz;
        const double c_zz = xx * yy - xy * xy;
        x->x = (c_xx * b.x + c_xy * b.y + c_xz * b.z) / determinant;
        x->y = (c_xy * b.x + c_yy * b.y + c_yz * b.z) / determinant;
        x->z = (c_xz * b.x + c_yz * b.y + c_zz * b.z) / determinant;
        return true;
    }
};

//---------------------------------------------------------------------------------------------------------------------
Vec3 load(const QVector<double>& values, int vertex)
{
    return {values.at(3 * vertex), values.at(3 * vertex + 1), values.at(3 * vertex + 2)};
}

//---------------------------------------------------------------------------------------------------------------------
void store(QVector<double>& values, int vertex, const Vec3& value)
{
    values[3 * vertex] = value.x;
    values[3 * vertex + 1] = value.y;
    values[3 * vertex + 2] = value.z;
}

//---------------------------------------------------------------------------------------------------------------------
quint64 edgeKey(quint32 a, quint32 b)
{
    return (static_cast<quint64>(qMin(a, b)) << 32) | qMax(a, b);
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
ClothSolver::ClothSolver(const ClothSettings& settings)
    : m_settings(settings)
    , m_prepared(false)
    , m_stepped(false)
{}

//---------------------------------------------------------------------------------------------------------------------
const ClothSettings& ClothSolver::settings() const
{
    return m_settings;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Changes gravity from the next step on, in cm/s²; without it pieces can be sewn together before they fall.
void ClothSolver::setGravity(const QVector3D& gravity)
{
    m_settings.gravity = gravity;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Changes the friction against the body from the next step on; without it pieces slide into place freely.
void ClothSolver::setFriction(qreal friction)
{
    m_settings.friction = friction;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds a piece of cloth: its flat mesh, which gives its rest shape, and where its vertices start out in cm.
/// Returns the number of its first vertex; stitches and pins count vertices over all pieces.
quint32 ClothSolver::addMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    const int offset = vertexCount();
    const int count = mesh.vertexCount();
    for (int i = 0; i < count; ++i)
    {
        const QVector3D start = i < positions.size() ? positions.at(i) : QVector3D();
        m_position << start.x() << start.y() << start.z();
        m_velocity << 0.0 << 0.0 << 0.0;
        m_last_velocity << 0.0 << 0.0 << 0.0;
        m_mass << 0.0;
        m_pinned << false;
    }

    // A third of each triangle's weight goes to each corner. Each edge resists stretching; across each inner
    // edge the two opposite corners resist bending.
    QHash<quint64, QVector<int>> edge_opposites;
    for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
    {
        const quint32 corners[3] = {mesh.indices.at(t), mesh.indices.at(t + 1), mesh.indices.at(t + 2)};
        const QPointF& a = mesh.rest_positions.at(static_cast<int>(corners[0]));
        const QPointF& b = mesh.rest_positions.at(static_cast<int>(corners[1]));
        const QPointF& c = mesh.rest_positions.at(static_cast<int>(corners[2]));
        const double area = qAbs((b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y())) / 2.0;
        for (int k = 0; k < 3; ++k)
        {
            m_mass[offset + static_cast<int>(corners[k])] += m_settings.density * area / 3.0;
            edge_opposites[edgeKey(corners[k], corners[(k + 1) % 3])].append(static_cast<int>(corners[(k + 2) % 3]));
        }
    }
    for (int i = offset; i < m_mass.size(); ++i)
    {
        m_mass[i] = qMax(m_mass.at(i), min_mass);
    }

    auto rest_distance = [&mesh](int a, int b)
    {
        return QLineF(mesh.rest_positions.at(a), mesh.rest_positions.at(b)).length();
    };
    for (auto edge = edge_opposites.cbegin(); edge != edge_opposites.cend(); ++edge)
    {
        const int a = static_cast<int>(edge.key() >> 32);
        const int b = static_cast<int>(edge.key() & 0xffffffffu);
        m_springs.append({offset + a, offset + b, rest_distance(a, b), m_settings.stretch_stiffness});

        const QVector<int>& opposite = edge.value();
        if (opposite.size() == 2)
        {
            m_springs.append({offset + opposite.at(0), offset + opposite.at(1), rest_distance(opposite.at(0),
                                                                                              opposite.at(1)),
                              m_settings.bend_stiffness});
        }
    }

    m_prepared = false;
    return static_cast<quint32>(offset);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds stitches between vertices of the meshes added, counting vertices over all pieces.
void ClothSolver::addStitches(const QVector<Stitch>& stitches)
{
    for (const Stitch& stitch : stitches)
    {
        const int count = vertexCount();
        const bool known = static_cast<int>(stitch.vertex) < count && static_cast<int>(stitch.edge_start) < count
                           && static_cast<int>(stitch.edge_end) < count;
        if (known)
        {
            m_stitches.append({static_cast<int>(stitch.vertex), static_cast<int>(stitch.edge_start),
                               static_cast<int>(stitch.edge_end), stitch.along});
        }
    }
    m_prepared = false;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The body the cloth drapes over.
void ClothSolver::setCollider(const BodyCollider& collider)
{
    m_collider = collider;
    m_contacts.clear();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A pinned vertex stays where it is.
void ClothSolver::setPinned(quint32 vertex, bool pinned)
{
    if (static_cast<int>(vertex) < m_pinned.size())
    {
        m_pinned[static_cast<int>(vertex)] = pinned;
    }
}

//---------------------------------------------------------------------------------------------------------------------
int ClothSolver::vertexCount() const
{
    return static_cast<int>(m_mass.size());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the vertices are, in cm, over all pieces in the order they were added.
QVector<QVector3D> ClothSolver::positions() const
{
    QVector<QVector3D> positions;
    positions.reserve(vertexCount());
    for (int i = 0; i < vertexCount(); ++i)
    {
        const Vec3 position = load(m_position, i);
        positions.append(QVector3D(static_cast<float>(position.x), static_cast<float>(position.y),
                                   static_cast<float>(position.z)));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How fast the vertices move, in cm/s.
QVector<QVector3D> ClothSolver::velocities() const
{
    QVector<QVector3D> velocities;
    velocities.reserve(vertexCount());
    for (int i = 0; i < vertexCount(); ++i)
    {
        const Vec3 velocity = load(m_velocity, i);
        velocities.append(QVector3D(static_cast<float>(velocity.x), static_cast<float>(velocity.y),
                                    static_cast<float>(velocity.z)));
    }
    return velocities;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far apart, in cm, the sides of the stitch that is furthest from closed still are.
qreal ClothSolver::widestStitch() const
{
    double widest = 0;
    for (const StitchTerm& stitch : m_stitches)
    {
        const Vec3 target = load(m_position, stitch.edge_start) * (1.0 - stitch.along)
                            + load(m_position, stitch.edge_end) * stitch.along;
        widest = qMax(widest, (load(m_position, stitch.vertex) - target).length());
    }
    return widest;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Moves the cloth on by a step of the given length in seconds; a 60th of a second works well.
void ClothSolver::step(qreal time_step)
{
    if (time_step <= 0 || vertexCount() == 0)
    {
        return;
    }
    if (!m_prepared)
    {
        prepare();
    }

    const double h = time_step;
    const Vec3 gravity{m_settings.gravity.x(), m_settings.gravity.y(), m_settings.gravity.z()};

    // Where each vertex would go if nothing but gravity acted on it. The first guess of where it ends up adds only as
    // much of gravity as the vertex was last pulled by (Chen et al.'s adaptive initialization): cloth that hangs or
    // rests then starts where it is, instead of below, where the few sweeps per step couldn't quite lift it back.
    const double gravity_length = gravity.length();
    const Vec3 down = gravity_length > 0 ? gravity * (1.0 / gravity_length) : Vec3();
    m_previous = m_position;
    m_inertial = m_position;
    for (int i = 0; i < vertexCount(); ++i)
    {
        if (!m_pinned.at(i))
        {
            const Vec3 position = load(m_position, i);
            const Vec3 velocity = load(m_velocity, i);
            store(m_inertial, i, position + velocity * h + gravity * (h * h));

            // Before the first step nothing is known yet, so everything is taken to be falling.
            const Vec3 acceleration = (velocity - load(m_last_velocity, i)) * (1.0 / h);
            const double pulled = m_stepped ? qBound(0.0, acceleration.dot(down), gravity_length) : gravity_length;
            store(m_position, i, position + velocity * h + down * (pulled * h * h));
        }
    }
    m_last_velocity = m_velocity;
    m_stepped = true;

    findContacts();

    // Vertices of one colour share no spring or stitch, so they can move at the same time. Each writes only its own
    // position, which mustn't be shared with another list then.
    m_position.detach();
    auto solve = [this, h](const int& vertex)
    {
        if (!m_pinned.at(vertex))
        {
            solveVertex(vertex, h);
        }
    };
    for (int iteration = 0; iteration < m_settings.iterations; ++iteration)
    {
        for (QVector<int>& color : m_colors)
        {
            if (color.size() >= parallel_colour)
            {
                QtConcurrent::blockingMap(color, solve);
            }
            else
            {
                std::for_each(color.cbegin(), color.cend(), solve);
            }
        }
    }

    const double kept = qMax(0.0, 1.0 - m_settings.air_damping * h);
    for (int i = 0; i < vertexCount(); ++i)
    {
        store(m_velocity, i, m_pinned.at(i) ? Vec3() : (load(m_position, i) - load(m_previous, i)) * (kept / h));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Lists which springs and stitches each vertex takes part in, and colours the vertices so no two that share a spring
// or a stitch have the same colour: those of one colour can then all move at once.
void ClothSolver::prepare()
{
    const int count = vertexCount();
    m_vertex_springs = QVector<QVector<int>>(count);
    m_vertex_stitches = QVector<QVector<StitchRole>>(count);
    QVector<QVector<int>> neighbours(count);

    for (int s = 0; s < m_springs.size(); ++s)
    {
        const Spring& spring = m_springs.at(s);
        m_vertex_springs[spring.a].append(s);
        m_vertex_springs[spring.b].append(s);
        neighbours[spring.a].append(spring.b);
        neighbours[spring.b].append(spring.a);
    }

    for (int s = 0; s < m_stitches.size(); ++s)
    {
        const StitchTerm& stitch = m_stitches.at(s);
        QVector<int> involved{stitch.vertex, stitch.edge_start};
        m_vertex_stitches[stitch.vertex].append({s, 1.0});
        if (stitch.edge_start == stitch.edge_end)
        {
            m_vertex_stitches[stitch.edge_start].append({s, -1.0});
        }
        else
        {
            m_vertex_stitches[stitch.edge_start].append({s, -(1.0 - stitch.along)});
            m_vertex_stitches[stitch.edge_end].append({s, -stitch.along});
            involved.append(stitch.edge_end);
        }
        for (const int a : involved)
        {
            for (const int b : involved)
            {
                if (a != b)
                {
                    neighbours[a].append(b);
                }
            }
        }
    }

    QVector<int> color_of(count, -1);
    m_colors.clear();
    for (int i = 0; i < count; ++i)
    {
        QVector<bool> taken(m_colors.size() + 1, false);
        for (const int neighbour : neighbours.at(i))
        {
            if (color_of.at(neighbour) >= 0)
            {
                taken[color_of.at(neighbour)] = true;
            }
        }
        const int color = static_cast<int>(std::find(taken.cbegin(), taken.cend(), false) - taken.cbegin());
        if (color == m_colors.size())
        {
            m_colors.append(QVector<int>());
        }
        color_of[i] = color;
        m_colors[color].append(i);
    }

    m_contacts.clear();
    m_prepared = true;
}

//---------------------------------------------------------------------------------------------------------------------
// Finds the body triangles each vertex may touch during the step: those near the way it is heading.
void ClothSolver::findContacts()
{
    if (m_contacts.size() != vertexCount())
    {
        m_contacts = QVector<QVector<int>>(vertexCount());
        m_contact_origin = QVector<double>(3 * vertexCount(), std::numeric_limits<double>::max());
    }
    m_contact_triangle = QVector<int>(vertexCount(), -1);
    if (m_collider.isEmpty())
    {
        return;
    }

    QVector<int> vertices(vertexCount());
    std::iota(vertices.begin(), vertices.end(), 0);
    m_contacts.detach();
    m_contact_origin.detach();
    m_contact_triangle.detach();
    QtConcurrent::blockingMap(vertices, [this](const int& vertex)
    {
        if (!m_pinned.at(vertex))
        {
            // The triangles found last time still do while the vertex stays within half the margin of where they
            // were looked up, its way through this step included.
            const Vec3 start = load(m_previous, vertex);
            const double heading = (load(m_inertial, vertex) - start).length();
            const double drift = (start - load(m_contact_origin, vertex)).length();
            if (drift + heading > contact_margin / 2.0)
            {
                const float radius = static_cast<float>(m_settings.thickness + contact_margin + heading);
                const QVector3D from(static_cast<float>(start.x), static_cast<float>(start.y),
                                     static_cast<float>(start.z));
                m_contacts[vertex] = m_collider.trianglesWithin(from, radius);
                store(m_contact_origin, vertex, start);
            }

            // The triangle nearest to where the vertex is first guessed to go is the one it may touch this step.
            const Vec3 guess = load(m_position, vertex);
            const QVector3D at(static_cast<float>(guess.x), static_cast<float>(guess.y), static_cast<float>(guess.z));
            BodyContact contact;
            if (m_collider.closest(at, m_contacts.at(vertex), &contact))
            {
                m_contact_triangle[vertex] = contact.triangle;
            }
        }
    });
}

//---------------------------------------------------------------------------------------------------------------------
// Moves one vertex to where its forces balance, holding all others still: one Newton step on its own energy.
void ClothSolver::solveVertex(int vertex, double time_step)
{
    const double h = time_step;
    const Vec3 position = load(m_position, vertex);
    const Vec3 moved = position - load(m_previous, vertex);

    // Inertia pulls the vertex towards where it would go by itself.
    const double inertia = m_mass.at(vertex) / (h * h);
    Vec3 force = (load(m_inertial, vertex) - position) * inertia;
    Mat3 hessian;
    hessian.addIdentity(inertia);

    const double damping = m_settings.damping / h;
    for (const int s : m_vertex_springs.at(vertex))
    {
        const Spring& spring = m_springs.at(s);
        const int other = spring.a == vertex ? spring.b : spring.a;
        const Vec3 offset = position - load(m_position, other);
        const double length = offset.length();
        if (length > 1e-9)
        {
            const Vec3 direction = offset * (1.0 / length);
            const double k = spring.stiffness;
            force -= direction * (k * (length - spring.rest_length));

            // Shortened springs give no sideways stiffness, which keeps the Hessian positive.
            hessian.addOuter(direction, k);
            const double sideways = k * qMax(0.0, 1.0 - spring.rest_length / length);
            hessian.addIdentity(sideways);
            hessian.addOuter(direction, -sideways);

            // Damping slows the spring's stretching, not the cloth moving or turning as a whole.
            const Vec3 relative = moved - (load(m_position, other) - load(m_previous, other));
            force -= direction * (damping * k * direction.dot(relative));
            hessian.addOuter(direction, damping * k);
        }
    }

    for (const StitchRole& role : m_vertex_stitches.at(vertex))
    {
        const StitchTerm& stitch = m_stitches.at(role.stitch);
        const Vec3 target = load(m_position, stitch.edge_start) * (1.0 - stitch.along)
                            + load(m_position, stitch.edge_end) * stitch.along;
        const Vec3 gap = load(m_position, stitch.vertex) - target;
        force -= gap * (m_settings.stitch_stiffness * role.weight);
        hessian.addIdentity(m_settings.stitch_stiffness * role.weight * role.weight);
    }

    const int triangle = m_contact_triangle.value(vertex, -1);
    const QVector3D point(static_cast<float>(position.x), static_cast<float>(position.y),
                          static_cast<float>(position.z));
    const BodyContact contact = triangle >= 0 ? m_collider.contactWith(point, triangle) : BodyContact();
    // The body and the floor push the cloth out to its thickness. Friction holds back sliding along them, up to what
    // the push allows.
    auto push_out = [this, &force, &hessian, &moved](const Vec3& normal, double depth)
    {
        const double k = m_settings.contact_stiffness;
        force += normal * (k * depth);
        hessian.addOuter(normal, k);

        const Vec3 sliding = moved - normal * normal.dot(moved);
        const double friction = qMin(m_settings.friction * k * depth / qMax(sliding.length(), friction_rest), k);
        force -= sliding * friction;
        hessian.addIdentity(friction);
        hessian.addOuter(normal, -friction);
    };
    if (triangle >= 0 && contact.distance < m_settings.thickness)
    {
        push_out(Vec3{contact.normal.x(), contact.normal.y(), contact.normal.z()},
                 m_settings.thickness - contact.distance);
    }
    const double above_floor = position.y - m_settings.floor_height;
    if (m_settings.floor && above_floor < m_settings.thickness)
    {
        push_out(Vec3{0.0, 1.0, 0.0}, m_settings.thickness - above_floor);
    }

    Vec3 step;
    if (hessian.solve(force, &step))
    {
        const double length = step.length();
        if (length > max_move)
        {
            step = step * (max_move / length);
        }

        // The body was only searched so far around where the vertex started the step; it mustn't go beyond, or it
        // could pass through the body unseen (the conservative bound of Chen et al.).
        Vec3 moved_to = position + step;
        if (!m_collider.isEmpty())
        {
            const Vec3 start = load(m_previous, vertex);
            const double reach = contact_margin + (load(m_inertial, vertex) - start).length();
            const double travelled = (moved_to - start).length();
            if (travelled > reach)
            {
                moved_to = start + (moved_to - start) * (reach / travelled);
            }
        }
        store(m_position, vertex, moved_to);
    }
}
