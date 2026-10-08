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
#include <QtConcurrent/QtConcurrentMap>
#include <QThread>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{
// A vertex moves at most this far, in cm, in one sweep, so a bad start can't throw it across the scene.
const double max_move = 2.0;

// What fabrics are given in, in the solver's cm, g and s: g per square metre, N/m and micro newton metres.
const double per_square_metre = 1.0e-4;
const double newton_per_metre = 1.0e3;
const double micro_newton_metre = 10.0;

// Cloth stiffer to stretch than this, in N/m, a step's sweeps can't keep up with, and it never comes to rest; cloth
// this stiff hardly stretches under its own weight anyway, so stiffer cloth is taken to be this stiff.
const qreal stiffest_stretch = 300.0;

// Below this, in cm, a tangential move is taken as resting, so friction can hold the cloth still.
const double friction_rest = 0.01;

// Body triangles this much further away than the cloth's thickness, in cm, are watched for contact. They are only
// looked up again once a vertex has used up half of that margin moving, which saves most lookups.
const double contact_margin = 2.0;

// The self contact search shares out the vertices and the edges among threads in runs of this many.
const int search_run = 256;

// Colours with at least this many vertices are solved on several threads, and self contacts worked out when there are
// at least this many; for fewer, handing out the work costs more than it saves.
const int parallel_colour = 1024;
const int parallel_contacts = 1024;

// Determinants smaller than this leave a vertex where it is, its forces don't say where to go.
const double singular = 1e-12;

// No vertex gets less mass than this, in g, so loose bits of mesh don't make the solve stiff.
const double min_mass = 1e-4;

// Parts of the cloth up to this much further apart than its thickness, in cm, are watched for touching, more if they
// are moving; they are looked for again once some vertex has moved half as far.
const double self_margin = 1.0;

// Moving parts of the cloth are watched for touching at most this much further away, in cm; a vertex moving further
// in one step could pass through cloth unseen, but watching that far around everything would cost too much.
const double self_reach_limit = 2.0;

// The cubes the cloth is sorted into to find its parts near each other are at least this big, in cm, and there are no
// more than this many of them.
const double smallest_cube = 2.0;
const int most_cubes = 1 << 20;

// Lengths below this, in cm, count as none.
const double tiny = 1e-9;

// Parts of a piece with corners closer than this to each other in the flat piece, in cm, are neighbours there: they
// don't push each other apart. At a mesh's usual 2 cm, cloth can't fold over tighter than that, and thin triangles
// along a piece's edges would otherwise push their neighbours.
const double rest_neighbours = 2.5;

// Parts of cloth lie on each other, rather than beside each other, when the gap between them is at least this close
// to square to the triangle or to both edges, as the cosine of the angle.
const double lying_on = 0.5;

// The direction square to a triangle, or to two edges, is only trusted when the angle between its sides, or between
// the edges, is at least this wide, as its sine: about 12 degrees.
const double clear_angle = 0.2;

// A vertex moves at most this share of the way to the nearest other cloth in a step, so two parts moving towards
// each other can't pass through each other; parts already touching count as a thickness apart.
const double self_bound_share = 0.45;

// A fold onto itself, right side in or wrong side in, is held this many radians short of flat: right onto itself, the
// two ways it can fold there come to the same angle, and the cloth wouldn't know which way to go.
const double fold_short_of_flat = 5.0 * M_PI / 180.0;

// A hinge further than this many radians from the angle it holds pulls no harder than this far: cloth starting flat
// on a fold onto itself would otherwise be thrown about by it.
const double hardest_pull = M_PI / 4.0;

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
// Where a vertex heading from the start of the step to somewhere may go: no further than the body was searched around
// it, or it could pass through the body unseen, nor further than the cloth near it allows (the conservative bounds of
// Chen et al.). Says whether the cloth stopped it.
Vec3 withinReach(const Vec3& start, const Vec3& heading_to, double body_reach, double cloth_bound, bool* stopped)
{
    Vec3 moved_to = heading_to;
    double travelled = (moved_to - start).length();
    if (travelled > body_reach)
    {
        moved_to = start + (moved_to - start) * (body_reach / travelled);
        travelled = body_reach;
    }
    *stopped = travelled > cloth_bound;
    if (*stopped)
    {
        moved_to = start + (moved_to - start) * (cloth_bound / travelled);
    }
    return moved_to;
}

//---------------------------------------------------------------------------------------------------------------------
// A part of a whole, or nothing of nothing, as a triangle squashed flat has.
double share(double part, double whole)
{
    return qAbs(whole) > tiny ? part / whole : 0.0;
}

//---------------------------------------------------------------------------------------------------------------------
// Where the point of triangle abc closest to p is, as the weights of a, b and c (Ericson, Real-Time Collision
// Detection, 5.1.5).
void closestOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c, double weights[3])
{
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const double d1 = ab.dot(p - a);
    const double d2 = ac.dot(p - a);
    const double d3 = ab.dot(p - b);
    const double d4 = ac.dot(p - b);
    const double d5 = ab.dot(p - c);
    const double d6 = ac.dot(p - c);
    const double va = d3 * d6 - d5 * d4;
    const double vb = d5 * d2 - d1 * d6;
    const double vc = d1 * d4 - d3 * d2;

    double u = 1;  // of a
    double v = 0;  // of b
    double w = 0;  // of c
    if (d1 <= 0 && d2 <= 0)
    {
        // a is closest
    }
    else if (d3 >= 0 && d4 <= d3)
    {
        u = 0;
        v = 1;
    }
    else if (vc <= 0 && d1 >= 0 && d3 <= 0)
    {
        v = share(d1, d1 - d3);
        u = 1 - v;
    }
    else if (d6 >= 0 && d5 <= d6)
    {
        u = 0;
        w = 1;
    }
    else if (vb <= 0 && d2 >= 0 && d6 <= 0)
    {
        w = share(d2, d2 - d6);
        u = 1 - w;
    }
    else if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
    {
        w = share(d4 - d3, (d4 - d3) + (d5 - d6));
        u = 0;
        v = 1 - w;
    }
    else
    {
        const double whole = va + vb + vc;
        v = share(vb, whole);
        w = share(vc, whole);
        u = 1 - v - w;
    }
    weights[0] = u;
    weights[1] = v;
    weights[2] = w;
}

//---------------------------------------------------------------------------------------------------------------------
// Where the closest points of the segments p1-q1 and p2-q2 are, as how far along each they are, from 0 to 1
// (Ericson, Real-Time Collision Detection, 5.1.9).
void closestOnSegments(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2, double* s, double* t)
{
    const Vec3 d1 = q1 - p1;
    const Vec3 d2 = q2 - p2;
    const Vec3 r = p1 - p2;
    const double a = d1.dot(d1);
    const double e = d2.dot(d2);
    const double f = d2.dot(r);
    *s = 0;
    *t = 0;
    if (a <= tiny && e > tiny)
    {
        *t = qBound(0.0, f / e, 1.0);
    }
    else if (a > tiny)
    {
        const double c = d1.dot(r);
        if (e <= tiny)
        {
            *s = qBound(0.0, -c / a, 1.0);
        }
        else
        {
            const double b = d1.dot(d2);
            const double apart = a * e - b * b;
            *s = apart > tiny ? qBound(0.0, (b * f - c * e) / apart, 1.0) : 0.0;
            *t = (b * *s + f) / e;
            if (*t < 0)
            {
                *t = 0;
                *s = qBound(0.0, -c / a, 1.0);
            }
            else if (*t > 1)
            {
                *t = 1;
                *s = qBound(0.0, (b - c) / a, 1.0);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

//---------------------------------------------------------------------------------------------------------------------
// How far the cloth bends across the edge from a to b, between the triangles abc and bad wound as the mesh winds
// them, in radians: 0 flat, positive with c and d bent towards the side the windings face away from, up to pi; and
// how that changes as each corner moves (Bridson et al., SCA 2003). False where a triangle has no area left.
bool bendAcross(const Vec3 corners[4], double* angle, Vec3 gradient[4])
{
    const Vec3& a = corners[0];
    const Vec3& b = corners[1];
    const Vec3& c = corners[2];
    const Vec3& d = corners[3];
    const Vec3 edge = b - a;
    const double length = edge.length();
    const Vec3 first = cross(edge, c - a);
    const Vec3 second = cross(a - b, d - b);
    const double first_squared = first.dot(first);
    const double second_squared = second.dot(second);
    if (length < tiny || first_squared < tiny || second_squared < tiny)
    {
        return false;
    }

    *angle = qAtan2(cross(first, second).dot(edge) / length, first.dot(second));
    const Vec3 first_part = first * (1.0 / first_squared);
    const Vec3 second_part = second * (1.0 / second_squared);
    gradient[0] = (first_part * ((c - b).dot(edge) / length) + second_part * ((d - b).dot(edge) / length)) * -1.0;
    gradient[1] = first_part * ((c - a).dot(edge) / length) + second_part * ((d - a).dot(edge) / length);
    gradient[2] = first_part * -length;
    gradient[3] = second_part * -length;
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// How far a hinge is bent past the angle it holds, both as bendAcross() measures them. The full turn is cut on the far
// side from where the hinge folds to, and at least a quarter turn from flat: a hinge folding the cloth onto itself is
// nearly as far from flat one way round as the other, and has to pull it its own way from flat, and back from a little
// the other way.
double bentPast(double angle, double rest)
{
    // The turn the angle is taken within, from just past its lower end: the cut, below the rest angle or above it.
    const double lowest = rest >= 0 ? qMin(rest - M_PI, -M_PI / 2.0) : qMax(rest + M_PI, M_PI / 2.0) - 2.0 * M_PI;
    double turned = angle;
    while (turned <= lowest)
    {
        turned += 2.0 * M_PI;
    }
    while (turned > lowest + 2.0 * M_PI)
    {
        turned -= 2.0 * M_PI;
    }
    return turned - rest;
}

//---------------------------------------------------------------------------------------------------------------------
// The gap between the closest points of two parts of cloth, from the second to the first: a vertex and a triangle,
// corners 0 and 1 to 3, or two edges, corners 0 to 1 and 2 to 3. It is the sum of the corners, each weighted as given
// back in weights.
Vec3 contactGap(const Vec3 corners[4], bool edges, double weights[4])
{
    weights[0] = 1;
    if (edges)
    {
        double s = 0;
        double t = 0;
        closestOnSegments(corners[0], corners[1], corners[2], corners[3], &s, &t);
        weights[0] = 1 - s;
        weights[1] = s;
        weights[2] = t - 1;
        weights[3] = -t;
    }
    else
    {
        double on_triangle[3];
        closestOnTriangle(corners[0], corners[1], corners[2], corners[3], on_triangle);
        weights[1] = -on_triangle[0];
        weights[2] = -on_triangle[1];
        weights[3] = -on_triangle[2];
    }

    Vec3 gap;
    for (int k = 0; k < 4; ++k)
    {
        gap += corners[k] * weights[k];
    }
    return gap;
}

//---------------------------------------------------------------------------------------------------------------------
// The direction square to the triangle, or to both edges, of two parts of cloth; not a unit vector.
Vec3 contactNormal(const Vec3 corners[4], bool edges)
{
    return edges ? cross(corners[1] - corners[0], corners[3] - corners[2])
                 : cross(corners[2] - corners[1], corners[3] - corners[1]);
}

//---------------------------------------------------------------------------------------------------------------------
// Whether the direction square to the triangle, or to both edges, is well defined: not for a sliver of a triangle, nor
// for edges close to parallel, whose square direction turns wildly as they move.
bool clearNormal(const Vec3 corners[4], bool edges, const Vec3& normal)
{
    const Vec3 a = edges ? corners[1] - corners[0] : corners[2] - corners[1];
    const Vec3 b = edges ? corners[3] - corners[2] : corners[3] - corners[1];
    return normal.length() >= clear_angle * a.length() * b.length() && normal.length() > tiny;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether the closest points of two parts of cloth lie inside the triangle, or inside both edges, as given by the
// weights of contactGap(). Only there can the parts pass through each other.
bool acrossContact(bool edges, const double weights[4])
{
    return edges ? weights[1] > 0 && weights[1] < 1 && weights[3] < 0 && weights[3] > -1
                 : weights[1] < 0 && weights[2] < 0 && weights[3] < 0;
}

//---------------------------------------------------------------------------------------------------------------------
// How far apart two parts of cloth are, and the unit direction from the second to the first, given the gap between
// them. Where the closest points are inside the triangle, or both edges, the distance is measured square to it on the
// side the first part is meant to be on, negative once it has passed through. Elsewhere it is the gap's length.
double contactDistance(const Vec3 corners[4], bool edges, const double weights[4], const Vec3& gap, double side,
                       Vec3* direction)
{
    const Vec3 normal = contactNormal(corners, edges);
    const double normal_length = normal.length();
    double distance = gap.length();
    if (side != 0 && acrossContact(edges, weights) && clearNormal(corners, edges, normal))
    {
        *direction = normal * (side / normal_length);
        distance = gap.dot(*direction);
    }
    else if (distance > tiny)
    {
        *direction = gap * (1.0 / distance);
    }
    else
    {
        *direction = normal_length > tiny ? normal * ((side < 0 ? -1.0 : 1.0) / normal_length) : Vec3{0.0, 1.0, 0.0};
    }
    return distance;
}

//---------------------------------------------------------------------------------------------------------------------
// Cubes of the same size over a box, each listing the items whose own boxes reach into it.
struct CubeGrid
{
    Vec3         origin;
    double       size = 1;
    int          counts[3] = {1, 1, 1};
    QVector<int> starts;  // where each cube's items start, and where the last one's end
    QVector<int> items;

    // Cubes covering the box from low to high, at least big enough.
    CubeGrid(const Vec3& low, const Vec3& high, double wanted_size)
        : origin(low)
        , size(wanted_size)
    {
        const double extent[3] = {high.x - low.x, high.y - low.y, high.z - low.z};
        auto total = [this, &extent]()
        {
            qint64 cubes = 1;
            for (int k = 0; k < 3; ++k)
            {
                counts[k] = qMax(1, qCeil(extent[k] / size));
                cubes *= counts[k];
            }
            return cubes;
        };
        while (total() > most_cubes)
        {
            size *= 2;
        }
    }

    void indexOf(const Vec3& point, int index[3]) const
    {
        const double offset[3] = {point.x - origin.x, point.y - origin.y, point.z - origin.z};
        for (int k = 0; k < 3; ++k)
        {
            index[k] = qBound(0, qFloor(offset[k] / size), counts[k] - 1);
        }
    }

    int cubeOf(const Vec3& point) const
    {
        int index[3];
        indexOf(point, index);
        return (index[2] * counts[1] + index[1]) * counts[0] + index[0];
    }

    // Visits each cube the box from low to high reaches into.
    template <typename Visit>
    void forEachCube(const Vec3& low, const Vec3& high, Visit visit) const
    {
        int from[3];
        int to[3];
        indexOf(low, from);
        indexOf(high, to);
        for (int z = from[2]; z <= to[2]; ++z)
        {
            for (int y = from[1]; y <= to[1]; ++y)
            {
                for (int x = from[0]; x <= to[0]; ++x)
                {
                    visit((z * counts[1] + y) * counts[0] + x);
                }
            }
        }
    }

    // Sorts the items into the cubes, each into all its box reaches into; bounds(item, low, high) gives the box.
    template <typename Bounds>
    void fill(int item_count, Bounds bounds)
    {
        starts = QVector<int>(counts[0] * counts[1] * counts[2] + 1, 0);
        for (int pass = 0; pass < 2; ++pass)
        {
            QVector<int> next = starts;
            for (int item = 0; item < item_count; ++item)
            {
                Vec3 low;
                Vec3 high;
                bounds(item, &low, &high);
                forEachCube(low, high, [this, pass, item, &next](int cube)
                {
                    if (pass == 0)
                    {
                        ++starts[cube + 1];
                    }
                    else
                    {
                        items[next[cube]++] = item;
                    }
                });
            }
            if (pass == 0)
            {
                for (int cube = 1; cube < starts.size(); ++cube)
                {
                    starts[cube] += starts.at(cube - 1);
                }
                items = QVector<int>(starts.last());
            }
        }
    }
};

//---------------------------------------------------------------------------------------------------------------------
quint64 pairKey(int a, int b)
{
    return (static_cast<quint64>(qMin(a, b)) << 32) | static_cast<quint32>(qMax(a, b));
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
    , m_compute()
    , m_on_device(false)
    , m_device_step()
{}

//---------------------------------------------------------------------------------------------------------------------
ClothSolver::~ClothSolver() = default;

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
/// @brief Changes how much of its speed the cloth loses per second from the next step on; at 1 / time step it keeps
/// none, and only the forces on it in each step move it.
void ClothSolver::setAirDamping(qreal air_damping)
{
    m_settings.air_damping = air_damping;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Switches the cloth keeping from passing through itself on or off from the next step on; off, pieces can be
/// sewn together through each other.
void ClothSolver::setSelfContact(bool self_contact)
{
    m_settings.self_contact = self_contact;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Lets pieces pass through each other from the next step on, or not; each still keeps from passing through
/// itself, so a hem turned up stays inside while the pieces are sewn together through each other.
void ClothSolver::setPiecesPassThrough(bool pass_through)
{
    if (pass_through != m_settings.pieces_pass_through)
    {
        m_settings.pieces_pass_through = pass_through;
        m_self_found_at.clear();  // the parts that may touch are other ones now
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds a piece of cloth: its flat mesh, which gives its rest shape, where its vertices start out in cm, its
/// fabric and the direction of its grain in the flat piece, in degrees anticlockwise from the piece's x axis, as
/// the piece scene shows it, and the lines it is folded along. Returns the number of its first vertex; stitches and
/// pins count vertices over all pieces.
quint32 ClothSolver::addMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions, const Fabric& fabric,
                             qreal grain_angle, const QVector<ClothFold>& folds)
{
    const int offset = vertexCount();
    const int count = mesh.vertexCount();
    const int piece = m_pieces.isEmpty() ? 0 : m_pieces.last() + 1;
    if (m_piece_start.isEmpty())
    {
        m_piece_start.append(0);
    }
    m_piece_start.append(offset + count);
    for (int i = 0; i < count; ++i)
    {
        const QVector3D start = i < positions.size() ? positions.at(i) : QVector3D();
        m_position << start.x() << start.y() << start.z();
        m_velocity << 0.0 << 0.0 << 0.0;
        m_mass << 0.0;
        m_pinned << false;
        m_pieces << piece;
        m_rest << mesh.rest_positions.at(i);
    }

    // The fabric's own directions in the flat piece, whose y axis points down: the grain, or warp, and across it the
    // weft. Each triangle's corners in them make its deformation gradient.
    const double grain = qDegreesToRadians(grain_angle);
    const QPointF warp_direction(qCos(grain), -qSin(grain));
    const QPointF weft_direction(-warp_direction.y(), warp_direction.x());
    auto in_fabric = [&mesh, &warp_direction, &weft_direction](quint32 vertex)
    {
        const QPointF& point = mesh.rest_positions.at(static_cast<int>(vertex));
        return QPointF(QPointF::dotProduct(point, weft_direction), QPointF::dotProduct(point, warp_direction));
    };
    const double density = fabric.weight * per_square_metre;
    Fabric stretch = fabric;
    stretch.warp_stiffness = qMin(fabric.warp_stiffness, stiffest_stretch);
    stretch.weft_stiffness = qMin(fabric.weft_stiffness, stiffest_stretch);
    stretch.bias_stiffness = qMin(fabric.bias_stiffness, stiffest_stretch);

    // A third of each triangle's weight goes to each corner.
    QHash<quint64, QVector<int>> edge_opposites;
    for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
    {
        const quint32 corners[3] = {mesh.indices.at(t), mesh.indices.at(t + 1), mesh.indices.at(t + 2)};
        const QPointF a = in_fabric(corners[0]);
        const QPointF b = in_fabric(corners[1]);
        const QPointF c = in_fabric(corners[2]);
        const double determinant = (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
        const double area = qAbs(determinant) / 2.0;
        for (int k = 0; k < 3; ++k)
        {
            m_mass[offset + static_cast<int>(corners[k])] += density * area / 3.0;
            edge_opposites[edgeKey(corners[k], corners[(k + 1) % 3])].append(static_cast<int>(corners[(k + 2) % 3]));
        }

        if (area > tiny)
        {
            // The deformation gradient is the sum over the corners of position times shape: the rows of the inverse
            // of the drafted sides' matrix for the second and third corner, less both for the first.
            Membrane membrane;
            for (int k = 0; k < 3; ++k)
            {
                membrane.vertices[k] = offset + static_cast<int>(corners[k]);
            }
            membrane.area = area;
            membrane.shape[1][0] = (c.y() - a.y()) / determinant;
            membrane.shape[1][1] = -(c.x() - a.x()) / determinant;
            membrane.shape[2][0] = -(b.y() - a.y()) / determinant;
            membrane.shape[2][1] = (b.x() - a.x()) / determinant;
            membrane.shape[0][0] = -membrane.shape[1][0] - membrane.shape[2][0];
            membrane.shape[0][1] = -membrane.shape[1][1] - membrane.shape[2][1];
            membrane.weft = stretch.weft_stiffness * newton_per_metre;
            membrane.warp = stretch.warp_stiffness * newton_per_metre;
            membrane.shear = stretch.shearStiffness() * newton_per_metre;
            m_membranes.append(membrane);
        }
    }
    for (int i = offset; i < m_mass.size(); ++i)
    {
        m_mass[i] = qMax(m_mass.at(i), min_mass);
    }
    for (const quint32 index : mesh.indices)
    {
        m_faces.append(offset + static_cast<int>(index));
    }

    // Across each inner edge the cloth resists bending. The weights are the cotangent ones of Bergou et al.; spread
    // over the two triangles' area, they make bending as stiff as the fabric's rigidity, to within a few percent.
    auto cotangent = [&mesh](int corner, int first, int second)
    {
        const QPointF u = mesh.rest_positions.at(first) - mesh.rest_positions.at(corner);
        const QPointF v = mesh.rest_positions.at(second) - mesh.rest_positions.at(corner);
        const double cross = qAbs(u.x() * v.y() - u.y() * v.x());
        return cross > tiny ? QPointF::dotProduct(u, v) / cross : 0.0;
    };
    auto triangle_area = [&mesh](int a, int b, int c)
    {
        const QPointF u = mesh.rest_positions.at(b) - mesh.rest_positions.at(a);
        const QPointF v = mesh.rest_positions.at(c) - mesh.rest_positions.at(a);
        return qAbs(u.x() * v.y() - u.y() * v.x()) / 2.0;
    };
    const double rigidity = fabric.bending * micro_newton_metre;

    // The edges along folds, and which fold each is of.
    QHash<quint64, int> fold_of_edge;
    for (int f = 0; f < folds.size(); ++f)
    {
        const QVector<quint32>& line = folds.at(f).vertices;
        for (int i = 0; i + 1 < line.size(); ++i)
        {
            if (line.at(i) != line.at(i + 1))
            {
                fold_of_edge.insert(edgeKey(line.at(i), line.at(i + 1)), f);
            }
        }
    }

    // In the order of the edges, not of the hash, so the same cloth always drapes the same way.
    QList<quint64> edges = edge_opposites.keys();
    std::sort(edges.begin(), edges.end());
    for (const quint64 edge : edges)
    {
        const int a = static_cast<int>(edge >> 32);
        const int b = static_cast<int>(edge & 0xffffffffu);
        m_edges << offset + a << offset + b;

        const QVector<int> opposite = edge_opposites.value(edge);
        const double areas = opposite.size() == 2 ? triangle_area(a, b, opposite.at(0))
                                                    + triangle_area(a, b, opposite.at(1)) : 0.0;
        const int fold = fold_of_edge.value(edge, -1);
        if (areas > tiny && fold >= 0)
        {
            // Along a fold, the hinge holds the fold's angle, with discrete shells' stiffness for the fabric's
            // rigidity. It goes from a to b where abc is wound as the mesh is, so it knows the right side.
            const int c = opposite.at(0);
            const int d = opposite.at(1);
            const QPointF& at_a = mesh.rest_positions.at(a);
            const QPointF& at_b = mesh.rest_positions.at(b);
            const QPointF& at_c = mesh.rest_positions.at(c);
            const bool wound = (at_b.x() - at_a.x()) * (at_c.y() - at_a.y())
                               - (at_c.x() - at_a.x()) * (at_b.y() - at_a.y()) > 0;
            const int corners[4] = {wound ? a : b, wound ? b : a, c, d};
            Hinge hinge;
            for (int k = 0; k < 4; ++k)
            {
                hinge.vertices[k] = offset + corners[k];
            }
            const double edge_squared = QPointF::dotProduct(at_b - at_a, at_b - at_a);
            hinge.stiffness = folds.at(fold).strength * 6.0 * rigidity * edge_squared / areas;
            hinge.fold = true;
            const double fullest = M_PI - fold_short_of_flat;
            hinge.rest_angle = qBound(-fullest, M_PI - qDegreesToRadians(folds.at(fold).angle), fullest);
            m_hinges.append(hinge);
        }
        else if (areas > tiny)
        {
            const int c = opposite.at(0);
            const int d = opposite.at(1);
            const double at_a_c = cotangent(a, b, c);
            const double at_a_d = cotangent(a, b, d);
            const double at_b_c = cotangent(b, a, c);
            const double at_b_d = cotangent(b, a, d);
            Hinge hinge;
            const int corners[4] = {a, b, c, d};
            const double weights[4] = {at_b_c + at_b_d, at_a_c + at_a_d, -at_a_c - at_b_c, -at_a_d - at_b_d};
            for (int k = 0; k < 4; ++k)
            {
                hinge.vertices[k] = offset + corners[k];
                hinge.weights[k] = weights[k];
            }
            hinge.stiffness = rigidity / areas;
            m_hinges.append(hinge);
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
    m_on_device = false;
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
/// @brief Puts a vertex somewhere, at rest, as a hand holding the cloth there does. Pinned, it stays there through
/// the steps, and the cloth around it follows.
void ClothSolver::moveVertex(quint32 vertex, const QVector3D& position)
{
    if (static_cast<int>(vertex) < vertexCount())
    {
        store(m_position, static_cast<int>(vertex), Vec3{position.x(), position.y(), position.z()});
        store(m_velocity, static_cast<int>(vertex), Vec3());
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Runs the sweeps on the graphics card from the next step on, or with none on the processor again. Must be
/// called on the thread that steps, which the device was opened on, and with none before the device closes. False if
/// the card can't compute.
bool ClothSolver::useDevice(QRhi* device)
{
    m_compute.reset();
    m_on_device = false;
    if (device != nullptr)
    {
        m_compute.reset(new ClothCompute(device));
        if (!m_compute->isReady())
        {
            m_compute.reset();
        }
    }
    return m_compute != nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the sweeps run on the graphics card. Should the card fail, they go back to the processor.
bool ClothSolver::isOnDevice() const
{
    return m_compute != nullptr;
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
/// @brief Where a vertex is, in cm.
QVector3D ClothSolver::position(quint32 vertex) const
{
    QVector3D at;
    if (static_cast<int>(vertex) < vertexCount())
    {
        const Vec3 position = load(m_position, static_cast<int>(vertex));
        at = QVector3D(static_cast<float>(position.x), static_cast<float>(position.y), static_cast<float>(position.z));
    }
    return at;
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
        m_on_device = false;
    }

    const double h = time_step;
    const Vec3 gravity{m_settings.gravity.x(), m_settings.gravity.y(), m_settings.gravity.z()};

    // Where each vertex would go if nothing but gravity acted on it. The first guess of where it ends up goes on as
    // it was going, without gravity: cloth that hangs or rests then starts where it is, instead of below, where the
    // few sweeps per step couldn't quite lift it back. Chen et al.'s adaptive initialization, which adds as much of
    // gravity as a vertex was last pulled by, keeps stiff cloth swinging for ever.
    m_previous = m_position;
    m_inertial = m_position;
    for (int i = 0; i < vertexCount(); ++i)
    {
        if (!m_pinned.at(i))
        {
            const Vec3 position = load(m_position, i);
            const Vec3 velocity = load(m_velocity, i);
            store(m_inertial, i, position + velocity * h + gravity * (h * h));
            store(m_position, i, position + velocity * h);
        }
    }

    findContacts();
    findSelfContacts();
    for (int i = 0; i < m_self_bound.size(); ++i)
    {
        const Vec3 start = load(m_previous, i);
        const Vec3 guess = load(m_position, i);
        const double travelled = (guess - start).length();
        if (travelled > m_self_bound.at(i))
        {
            store(m_position, i, start + (guess - start) * (m_self_bound.at(i) / travelled));
        }
    }

    // On the graphics card, if there is one; should it fail, on the processor from then on.
    if (m_compute == nullptr || !sweepOnDevice(h))
    {
        m_compute.reset();
        sweep(h);
    }

    const double kept = qMax(0.0, 1.0 - m_settings.air_damping * h);
    for (int i = 0; i < vertexCount(); ++i)
    {
        const bool still = m_pinned.at(i) || m_stopped.value(i, 0) != 0;
        store(m_velocity, i, still ? Vec3() : (load(m_position, i) - load(m_previous, i)) * (kept / h));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Sweeps over the vertices, colour by colour, each moving to where its forces balance.
void ClothSolver::sweep(double time_step)
{
    const double h = time_step;

    // Vertices of one colour share no membrane, hinge or stitch, so they can move at the same time. Each writes only
    // its own position, which mustn't be shared with another list then. Cloth touching itself can bring vertices of
    // one colour together, so the cloth a vertex touches is seen where it was when the sweep started; the sweeps on the
    // graphics card see it the same way.
    QVector<double> two_sweeps_ago = m_position;
    QVector<double> one_sweep_ago = m_position;
    m_position.detach();
    double weight = 1.0;
    for (int iteration = 0; iteration < m_settings.iterations; ++iteration)
    {
        pushSelfContacts(h);
        for (QVector<int>& color : m_colors)
        {
            if (color.size() >= parallel_colour)
            {
                const int run_count = qMax(1, QThread::idealThreadCount());
                const int run_length = (static_cast<int>(color.size()) + run_count - 1) / run_count;
                QVector<int> runs(run_count);
                std::iota(runs.begin(), runs.end(), 0);
                QtConcurrent::blockingMap(runs, [this, h, &color, run_length](const int& run)
                {
                    const int end = qMin(static_cast<int>(color.size()), (run + 1) * run_length);
                    for (int i = run * run_length; i < end; ++i)
                    {
                        const int vertex = color.at(i);
                        if (!m_pinned.at(vertex))
                        {
                            solveVertex(vertex, h);
                        }
                    }
                });
            }
            else
            {
                for (const int vertex : color)
                {
                    if (!m_pinned.at(vertex))
                    {
                        solveVertex(vertex, h);
                    }
                }
            }
        }

        // Each sweep from the second on is carried on from where the vertices were two sweeps before, by a weight that
        // grows towards a limit set by the spectral radius. It mustn't take a vertex further than a sweep may.
        if (m_settings.acceleration > 0)
        {
            const double infinity = std::numeric_limits<double>::infinity();
            const double radius = m_settings.acceleration * m_settings.acceleration;
            weight = iteration == 0 ? 1.0 : 4.0 / (4.0 - radius * (iteration == 1 ? 2.0 : weight));
            if (iteration > 0)
            {
                for (int i = 0; i < vertexCount(); ++i)
                {
                    if (!m_pinned.at(i))
                    {
                        const Vec3 earlier = load(two_sweeps_ago, i);
                        bool stopped = false;
                        store(m_position, i, withinReach(load(m_previous, i),
                                                         earlier + (load(m_position, i) - earlier) * weight,
                                                         bodyReach(i), m_self_bound.value(i, infinity), &stopped));
                        if (stopped && i < m_stopped.size())
                        {
                            m_stopped[i] = 1;
                        }
                    }
                }
            }
            two_sweeps_ago = one_sweep_ago;
            one_sweep_ago = m_position;
            one_sweep_ago.detach();
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The sweeps on the graphics card: the cloth goes onto it once, then each step's start, and the vertices come back
// where the sweeps left them. False if the card failed.
bool ClothSolver::sweepOnDevice(double time_step)
{
    if (!m_on_device)
    {
        m_on_device = m_compute->setCloth(packedCloth());
        if (!m_on_device)
        {
            return false;
        }
    }

    const int count = vertexCount();
    ClothCompute::Step& step = m_device_step;
    step.time_step = static_cast<float>(time_step);
    step.damping = static_cast<float>(m_settings.damping / time_step);
    step.stitch_stiffness = static_cast<float>(m_settings.stitch_stiffness);
    step.contact_stiffness = static_cast<float>(m_settings.contact_stiffness);
    step.self_contact_stiffness = static_cast<float>(m_settings.self_contact_stiffness);
    step.friction = static_cast<float>(m_settings.friction);
    step.thickness = static_cast<float>(m_settings.thickness);
    step.floor_height = static_cast<float>(m_settings.floor_height);
    step.has_floor = m_settings.floor ? 1 : 0;
    step.has_body = m_collider.isEmpty() ? 0 : 1;
    step.contact_margin = static_cast<float>(contact_margin);
    step.max_move = static_cast<float>(max_move);
    step.friction_rest = static_cast<float>(friction_rest);
    step.vertex_count = count;

    // Where each vertex starts the sweeps and whether it is pinned; where it started the step and how far the cloth
    // near it lets it move; where it would go by itself and its mass.
    ClothCompute::Start start;
    start.positions.resize(4 * count);
    start.motion.resize(8 * count);
    const float unbounded = 3.0e38f;
    for (int i = 0; i < count; ++i)
    {
        for (int k = 0; k < 3; ++k)
        {
            start.positions[4 * i + k] = static_cast<float>(m_position.at(3 * i + k));
            start.motion[8 * i + k] = static_cast<float>(m_previous.at(3 * i + k));
            start.motion[8 * i + 4 + k] = static_cast<float>(m_inertial.at(3 * i + k));
        }
        start.positions[4 * i + 3] = m_pinned.at(i) ? 1.0f : 0.0f;
        start.motion[8 * i + 3] = static_cast<float>(qMin(m_self_bound.value(i, unbounded),
                                                          static_cast<double>(unbounded)));
        start.motion[8 * i + 7] = static_cast<float>(m_mass.at(i));
    }

    // The body triangle each vertex may touch, then the self contacts.
    start.contacts.reserve(2 * count + 1 + m_self_roles.size() + 8 * m_self_contacts.size());
    for (int i = 0; i < count; ++i)
    {
        start.contacts.append(m_contact_triangle.value(i, -1));
    }
    step.self_starts = static_cast<qint32>(start.contacts.size());
    for (int i = 0; i <= count; ++i)
    {
        start.contacts.append(m_self_role_start.value(i, 0));
    }
    step.self_roles = static_cast<qint32>(start.contacts.size());
    for (const SelfContactRole& role : m_self_roles)
    {
        start.contacts.append(role.contact << 2 | role.role);
    }
    step.self_contacts = static_cast<qint32>(start.contacts.size());
    step.self_contact_count = static_cast<qint32>(m_self_contacts.size());
    for (const SelfContact& contact : m_self_contacts)
    {
        start.contacts << contact.vertices[0] << contact.vertices[1] << contact.vertices[2] << contact.vertices[3]
                       << (contact.edges ? 1 : 0) << static_cast<qint32>(contact.side) << (contact.live ? 1 : 0)
                       << 0;
    }

    // Each sweep's Chebyshev weight, as sweep() has them.
    double weight = 1.0;
    for (int iteration = 0; iteration < m_settings.iterations; ++iteration)
    {
        const double radius = m_settings.acceleration * m_settings.acceleration;
        weight = iteration == 0 ? 1.0 : 4.0 / (4.0 - radius * (iteration == 1 ? 2.0 : weight));
        start.weights.append(iteration > 0 && m_settings.acceleration > 0 ? static_cast<float>(weight) : 0.0f);
    }

    QVector<float> swept;
    QVector<qint32> stopped;
    if (!m_compute->sweep(step, start, &swept, &stopped))
    {
        return false;
    }
    for (int i = 0; i < count; ++i)
    {
        for (int k = 0; k < 3; ++k)
        {
            m_position[3 * i + k] = swept.at(4 * i + k);
        }
    }
    m_stopped = QVector<char>(count, 0);
    for (int i = 0; i < count; ++i)
    {
        m_stopped[i] = stopped.at(i) != 0 ? 1 : 0;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth packed as the compute shaders read it, and where its parts start; see shaders/cloth_sweep.comp.
ClothCompute::Cloth ClothSolver::packedCloth()
{
    const int count = vertexCount();
    ClothCompute::Cloth cloth;
    cloth.vertex_count = count;
    QVector<qint32>& topology = cloth.topology;
    QVector<float>& terms = cloth.terms;
    ClothCompute::Step& step = m_device_step;

    for (const QVector<int>& color : m_colors)
    {
        cloth.colour_starts.append(static_cast<qint32>(topology.size()));
        topology += color;
    }
    cloth.colour_starts.append(static_cast<qint32>(topology.size()));

    // Each vertex's roles, membranes first, then hinges and stitches, and where they start.
    step.vertex_terms = static_cast<qint32>(topology.size());
    topology.resize(topology.size() + 4 * count);
    for (int vertex = 0; vertex < count; ++vertex)
    {
        topology[step.vertex_terms + 4 * vertex] = static_cast<qint32>(topology.size());
        for (const Role& role : m_vertex_membranes.at(vertex))
        {
            topology.append(role.term << 2 | role.corner);
        }
        topology[step.vertex_terms + 4 * vertex + 1] = static_cast<qint32>(topology.size());
        for (const Role& role : m_vertex_hinges.at(vertex))
        {
            topology.append(role.term << 2 | role.corner);
        }
        topology[step.vertex_terms + 4 * vertex + 2] = static_cast<qint32>(topology.size());
        for (const StitchRole& role : m_vertex_stitches.at(vertex))
        {
            // The stitched vertex, the start or the end of the edge it is stitched to, or both.
            const StitchTerm& stitch = m_stitches.at(role.stitch);
            const int kind = vertex == stitch.vertex && role.weight > 0 ? 0
                             : stitch.edge_start == stitch.edge_end   ? 3
                             : vertex == stitch.edge_start            ? 1
                                                                      : 2;
            topology.append(role.stitch << 2 | kind);
        }
        topology[step.vertex_terms + 4 * vertex + 3] = static_cast<qint32>(topology.size());
    }

    step.membrane_corners = static_cast<qint32>(topology.size());
    for (const Membrane& membrane : m_membranes)
    {
        topology << membrane.vertices[0] << membrane.vertices[1] << membrane.vertices[2] << 0;
        terms << static_cast<float>(membrane.shape[0][0]) << static_cast<float>(membrane.shape[0][1])
              << static_cast<float>(membrane.shape[1][0]) << static_cast<float>(membrane.shape[1][1])
              << static_cast<float>(membrane.shape[2][0]) << static_cast<float>(membrane.shape[2][1])
              << static_cast<float>(membrane.area) << 0.0f << static_cast<float>(membrane.weft)
              << static_cast<float>(membrane.warp) << static_cast<float>(membrane.shear) << 0.0f;
    }

    step.hinge_corners = static_cast<qint32>(topology.size());
    step.hinges = static_cast<qint32>(terms.size() / 4);
    for (const Hinge& hinge : m_hinges)
    {
        topology << hinge.vertices[0] << hinge.vertices[1] << hinge.vertices[2] << hinge.vertices[3];
        terms << static_cast<float>(hinge.weights[0]) << static_cast<float>(hinge.weights[1])
              << static_cast<float>(hinge.weights[2]) << static_cast<float>(hinge.weights[3])
              << static_cast<float>(hinge.stiffness) << static_cast<float>(hinge.rest_angle)
              << (hinge.fold ? 1.0f : 0.0f) << 0.0f;
    }

    step.stitch_corners = static_cast<qint32>(topology.size());
    step.stitches = static_cast<qint32>(terms.size() / 4);
    for (const StitchTerm& stitch : m_stitches)
    {
        topology << stitch.vertex << stitch.edge_start << stitch.edge_end << 0;
        terms << static_cast<float>(stitch.along) << 0.0f << 0.0f << 0.0f;
    }

    // The body's triangles: their corners and the way out of the body.
    step.body = static_cast<qint32>(terms.size() / 4);
    const QVector<QVector3D>& body = m_collider.positions();
    const QVector<quint32>& triangles = m_collider.triangles();
    for (int t = 0; t + 2 < triangles.size() && !m_collider.isEmpty(); t += 3)
    {
        const QVector3D& a = body.at(static_cast<int>(triangles.at(t)));
        const QVector3D& b = body.at(static_cast<int>(triangles.at(t + 1)));
        const QVector3D& c = body.at(static_cast<int>(triangles.at(t + 2)));
        const QVector3D normal = QVector3D::crossProduct(b - a, c - a).normalized();
        for (const QVector3D& corner : {a, b, c, normal})
        {
            terms << corner.x() << corner.y() << corner.z() << 0.0f;
        }
    }
    return cloth;
}

//---------------------------------------------------------------------------------------------------------------------
// Lists which membranes, hinges and stitches each vertex takes part in, and colours the vertices so no two that share
// one have the same colour: those of one colour can then all move at once.
void ClothSolver::prepare()
{
    const int count = vertexCount();
    m_vertex_membranes = QVector<QVector<Role>>(count);
    m_vertex_hinges = QVector<QVector<Role>>(count);
    m_vertex_stitches = QVector<QVector<StitchRole>>(count);
    QVector<QVector<int>> neighbours(count);

    for (int m = 0; m < m_membranes.size(); ++m)
    {
        const Membrane& membrane = m_membranes.at(m);
        for (int k = 0; k < 3; ++k)
        {
            m_vertex_membranes[membrane.vertices[k]].append({m, k});
            neighbours[membrane.vertices[k]].append(membrane.vertices[(k + 1) % 3]);
            neighbours[membrane.vertices[k]].append(membrane.vertices[(k + 2) % 3]);
        }
    }
    for (int h = 0; h < m_hinges.size(); ++h)
    {
        const Hinge& hinge = m_hinges.at(h);
        for (int k = 0; k < 4; ++k)
        {
            m_vertex_hinges[hinge.vertices[k]].append({h, k});
        }
        neighbours[hinge.vertices[2]].append(hinge.vertices[3]);
        neighbours[hinge.vertices[3]].append(hinge.vertices[2]);
    }

    m_stitched = QVector<QVector<int>>(count);
    for (int s = 0; s < m_stitches.size(); ++s)
    {
        const StitchTerm& stitch = m_stitches.at(s);
        for (const int end : {stitch.edge_start, stitch.edge_end})
        {
            m_stitched[stitch.vertex].append(end);
            m_stitched[end].append(stitch.vertex);
        }
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

    for (QVector<int>& stitched : m_stitched)
    {
        std::sort(stitched.begin(), stitched.end());
        stitched.erase(std::unique(stitched.begin(), stitched.end()), stitched.end());
    }

    // Across a seam, the cloth near the stitched vertices on one side is next to the cloth near their partners on the
    // other, as it is next to itself within a piece; also where a piece is sewn to itself, as a sleeve is.
    m_seam_neighbours.clear();
    auto near_at_rest = [this](int vertex)
    {
        QVector<int> near_vertex;
        const int piece = m_pieces.at(vertex);
        for (int other = m_piece_start.at(piece); other < m_piece_start.at(piece + 1); ++other)
        {
            const QPointF apart = m_rest.at(other) - m_rest.at(vertex);
            if (QPointF::dotProduct(apart, apart) < rest_neighbours * rest_neighbours)
            {
                near_vertex.append(other);
            }
        }
        return near_vertex;
    };
    for (int vertex = 0; vertex < count; ++vertex)
    {
        for (const int partner : m_stitched.at(vertex))
        {
            if (partner > vertex)
            {
                const QVector<int> here = near_at_rest(vertex);
                const QVector<int> there = near_at_rest(partner);
                for (const int a : here)
                {
                    for (const int b : there)
                    {
                        m_seam_neighbours.insert(pairKey(a, b));
                    }
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
    m_self_found_at.clear();
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
// How far a vertex may move from where it started the step: the body was only searched so far around it.
double ClothSolver::bodyReach(int vertex) const
{
    return m_collider.isEmpty() ? std::numeric_limits<double>::infinity()
                                : contact_margin + (load(m_inertial, vertex) - load(m_previous, vertex)).length();
}

//---------------------------------------------------------------------------------------------------------------------
// Before each sweep: how each self contact pushes each of its four vertices, nine numbers each, the push and then how
// stiffly, as a symmetric 3 x 3 matrix. Cloth closer to itself than its thickness is pushed apart, along the line
// between the closest points; parts that have passed through each other during the step are pushed back to the sides
// they started the step on. A vertex sees the cloth it touches where it was when the sweep started, and is itself still
// there when its turn comes, as nothing else moves it; so each contact pushes the same all through the sweep, and is
// worked out once rather than for each of its vertices.
void ClothSolver::pushSelfContacts(double time_step)
{
    const int count = static_cast<int>(m_self_contacts.size());
    m_self_push.fill(0.0, 36 * count);
    double* pushes = m_self_push.data();
    const double damping = m_settings.damping / time_step;
    auto push = [this, pushes, damping](int c)
    {
        const SelfContact& contact = m_self_contacts.at(c);
        if (!contact.live)
        {
            return;
        }
        Vec3 corners[4];
        for (int k = 0; k < 4; ++k)
        {
            corners[k] = load(m_position, contact.vertices[k]);
        }
        double weights[4];
        const Vec3 gap = contactGap(corners, contact.edges, weights);
        Vec3 normal;
        const double distance = contactDistance(corners, contact.edges, weights, gap, contact.side, &normal);
        if (distance >= m_settings.thickness)
        {
            return;
        }

        // Damping slows the parts coming together or apart, so cloth landing on cloth doesn't bounce.
        Vec3 closing;
        for (int k = 0; k < 4; ++k)
        {
            closing += (corners[k] - load(m_previous, contact.vertices[k])) * weights[k];
        }
        const double k = m_settings.self_contact_stiffness;
        const double depth = m_settings.thickness - distance - damping * normal.dot(closing);
        for (int role = 0; role < 4; ++role)
        {
            const double weight = weights[role];
            const Vec3 force = normal * (k * weight * depth);
            Mat3 stiffness;
            stiffness.addOuter(normal, k * weight * weight * (1.0 + damping));
            double* to = pushes + 9 * (4 * c + role);
            to[0] = force.x;
            to[1] = force.y;
            to[2] = force.z;
            to[3] = stiffness.xx;
            to[4] = stiffness.xy;
            to[5] = stiffness.xz;
            to[6] = stiffness.yy;
            to[7] = stiffness.yz;
            to[8] = stiffness.zz;
        }
    };

    if (count >= parallel_contacts)
    {
        const int run_count = qMax(1, QThread::idealThreadCount());
        const int run_length = (count + run_count - 1) / run_count;
        QVector<int> runs(run_count);
        std::iota(runs.begin(), runs.end(), 0);
        QtConcurrent::blockingMap(runs, [&push, count, run_length](const int& run)
        {
            for (int c = run * run_length; c < qMin(count, (run + 1) * run_length); ++c)
            {
                push(c);
            }
        });
    }
    else
    {
        for (int c = 0; c < count; ++c)
        {
            push(c);
        }
    }
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
    for (const Role& role : m_vertex_membranes.at(vertex))
    {
        const Membrane& membrane = m_membranes.at(role.term);

        // The deformation gradient's columns, where the weft and the warp went, and how fast they are changing.
        Vec3 weft;
        Vec3 warp;
        Vec3 weft_rate;
        Vec3 warp_rate;
        for (int k = 0; k < 3; ++k)
        {
            const int corner = membrane.vertices[k];
            const Vec3 at = k == role.corner ? position : load(m_position, corner);
            const Vec3 change = k == role.corner ? moved : at - load(m_previous, corner);
            weft += at * membrane.shape[k][0];
            warp += at * membrane.shape[k][1];
            weft_rate += change * membrane.shape[k][0];
            warp_rate += change * membrane.shape[k][1];
        }

        // Green strain and its rate, and the stress they make, damping slowing the cloth's straining.
        const double strain_weft = (weft.dot(weft) - 1.0) / 2.0;
        const double strain_warp = (warp.dot(warp) - 1.0) / 2.0;
        const double strain_shear = weft.dot(warp) / 2.0;
        const double rate_weft = weft.dot(weft_rate);
        const double rate_warp = warp.dot(warp_rate);
        const double rate_shear = (weft.dot(warp_rate) + warp.dot(weft_rate)) / 2.0;
        const double stress_weft = membrane.weft * (strain_weft + damping * rate_weft);
        const double stress_warp = membrane.warp * (strain_warp + damping * rate_warp);
        const double stress_shear = 2.0 * membrane.shear * (strain_shear + damping * rate_shear);

        const double along_weft = membrane.shape[role.corner][0];
        const double along_warp = membrane.shape[role.corner][1];
        const double area = membrane.area;
        force -= (weft * (stress_weft * along_weft + stress_shear * along_warp)
                  + warp * (stress_shear * along_weft + stress_warp * along_warp)) * area;

        // The material part of the Hessian, with damping's share, and the geometric part where the cloth is
        // stretched; where it is pushed together that part is left out, which keeps the Hessian positive.
        const double material = area * (1.0 + damping);
        hessian.addOuter(weft * along_weft, material * membrane.weft);
        hessian.addOuter(warp * along_warp, material * membrane.warp);
        hessian.addOuter((warp * along_weft + weft * along_warp) * 0.5, 4.0 * material * membrane.shear);
        const double geometric = along_weft * along_weft * stress_weft + 2.0 * along_weft * along_warp * stress_shear
                                 + along_warp * along_warp * stress_warp;
        hessian.addIdentity(area * qMax(0.0, geometric));
    }

    // Bending: Bergou et al.'s quadratic energy, which only the cloth leaving its flat shape gives rise to; along a
    // fold, the hinge's angle away from the fold's, with the Gauss-Newton part of its Hessian.
    for (const Role& role : m_vertex_hinges.at(vertex))
    {
        const Hinge& hinge = m_hinges.at(role.term);
        if (hinge.fold)
        {
            Vec3 corners[4];
            Vec3 gradient[4];
            double angle = 0;
            for (int k = 0; k < 4; ++k)
            {
                corners[k] = k == role.corner ? position : load(m_position, hinge.vertices[k]);
            }
            if (bendAcross(corners, &angle, gradient))
            {
                double rate = 0;
                for (int k = 0; k < 4; ++k)
                {
                    rate += gradient[k].dot(k == role.corner ? moved
                                                             : corners[k] - load(m_previous, hinge.vertices[k]));
                }
                const double off = bentPast(angle, hinge.rest_angle);
                const double pulled = qBound(-hardest_pull, off, hardest_pull);
                const Vec3& slope = gradient[role.corner];
                force -= slope * (hinge.stiffness * (pulled + rate * damping));
                hessian.addOuter(slope, hinge.stiffness * (hardest_pull / qMax(qAbs(off), hardest_pull) + damping));
            }
            continue;
        }

        Vec3 bend;
        Vec3 bend_rate;
        for (int k = 0; k < 4; ++k)
        {
            const int corner = hinge.vertices[k];
            const Vec3 at = k == role.corner ? position : load(m_position, corner);
            bend += at * hinge.weights[k];
            bend_rate += (k == role.corner ? moved : at - load(m_previous, corner)) * hinge.weights[k];
        }
        const double weight = hinge.weights[role.corner];
        force -= (bend + bend_rate * damping) * (hinge.stiffness * weight);
        hessian.addIdentity(hinge.stiffness * weight * weight * (1.0 + damping));
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

    // Cloth closer to itself than its thickness pushes it away, as pushSelfContacts() worked out.
    for (int r = m_self_role_start.value(vertex); r < m_self_role_start.value(vertex + 1); ++r)
    {
        const SelfContactRole& role = m_self_roles.at(r);
        const double* push = m_self_push.constData() + 9 * (4 * role.contact + role.role);
        force += Vec3{push[0], push[1], push[2]};
        hessian.xx += push[3];
        hessian.xy += push[4];
        hessian.xz += push[5];
        hessian.yy += push[6];
        hessian.yz += push[7];
        hessian.zz += push[8];
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

        bool stopped = false;
        store(m_position, vertex, withinReach(load(m_previous, vertex), position + step, bodyReach(vertex),
                                              m_self_bound.value(vertex, std::numeric_limits<double>::infinity()),
                                              &stopped));
        if (vertex < m_stopped.size())
        {
            m_stopped[vertex] = stopped ? 1 : 0;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Finds the parts of the cloth that may touch each other during the step: each vertex and the triangles, and each edge
// and the edges, that were near when the step started, or further apart by as much as they are heading this step.
// Parts sewn to each other are left out.
void ClothSolver::findSelfContacts()
{
    const int count = vertexCount();
    m_self_bound.clear();
    m_stopped = QVector<char>(count, 0);
    if (!m_settings.self_contact || m_faces.isEmpty())
    {
        m_self_contacts.clear();
        m_self_roles.clear();
        m_self_role_start = QVector<int>(count + 1, 0);
        m_self_found_at.clear();
        return;
    }

    QVector<double> heading(count, 0.0);
    for (int i = 0; i < count; ++i)
    {
        heading[i] = qMin((load(m_inertial, i) - load(m_previous, i)).length(), self_reach_limit);
    }

    // The contacts found in an earlier step still do while no vertex has moved, and is heading, further than half the
    // margin from where they were found: parts that weren't near then can't touch yet.
    bool still = m_self_found_at.size() == m_previous.size();
    for (int i = 0; i < count && still; ++i)
    {
        still = (load(m_previous, i) - load(m_self_found_at, i)).length() + heading.at(i) <= self_margin / 2.0;
    }
    if (!still)
    {
        m_self_contacts.clear();
        findSelfContactsAround(heading);
        m_self_found_at = m_previous;
    }

    // Which sides the parts start the step on, and how far their vertices may move in it.
    m_self_bound = QVector<double>(count, std::numeric_limits<double>::infinity());
    for (SelfContact& contact : m_self_contacts)
    {
        startSelfContact(&contact, heading);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Searches the cloth for the parts near each other, as findSelfContacts() says, from where the vertices are now and
// how far they are heading, and lists which contacts each vertex takes part in.
void ClothSolver::findSelfContactsAround(const QVector<double>& heading)
{
    const int count = vertexCount();

    // Two parts can come together by the gap the cloth keeps and what both are heading. Each part reaches out by half
    // the gap and as far as it is heading; the parts whose reaches overlap may touch. Each part reaching only as far
    // as itself keeps the search small when a few vertices move fast.
    const double close_by = m_settings.thickness + self_margin;
    const double half_gap = close_by / 2.0;
    auto reach_of = [this, &heading, half_gap](const int* vertices, int count_of, Vec3* box_low, Vec3* box_high)
    {
        const double infinite = std::numeric_limits<double>::infinity();
        *box_low = {infinite, infinite, infinite};
        *box_high = {-infinite, -infinite, -infinite};
        double grow = half_gap;
        for (int k = 0; k < count_of; ++k)
        {
            const Vec3 point = load(m_previous, vertices[k]);
            grow = qMax(grow, half_gap + heading.at(vertices[k]));
            *box_low = {qMin(box_low->x, point.x), qMin(box_low->y, point.y), qMin(box_low->z, point.z)};
            *box_high = {qMax(box_high->x, point.x), qMax(box_high->y, point.y), qMax(box_high->z, point.z)};
        }
        *box_low = *box_low - Vec3{grow, grow, grow};
        *box_high = *box_high + Vec3{grow, grow, grow};
    };
    auto overlap = [](const Vec3& a_low, const Vec3& a_high, const Vec3& b_low, const Vec3& b_high)
    {
        return a_low.x <= b_high.x && b_low.x <= a_high.x && a_low.y <= b_high.y && b_low.y <= a_high.y
               && a_low.z <= b_high.z && b_low.z <= a_high.z;
    };
    auto heading_of = [&heading](const int* vertices, int count_of)
    {
        double most = 0;
        for (int k = 0; k < count_of; ++k)
        {
            most = qMax(most, heading.at(vertices[k]));
        }
        return most;
    };

    // The reaches of the triangles and the edges, and a grid of cubes around all of them.
    const int face_count = static_cast<int>(m_faces.size() / 3);
    const int edge_count = static_cast<int>(m_edges.size() / 2);
    QVector<Vec3> face_low(face_count);
    QVector<Vec3> face_high(face_count);
    QVector<Vec3> edge_low(edge_count);
    QVector<Vec3> edge_high(edge_count);
    const double infinite = std::numeric_limits<double>::infinity();
    Vec3 low{infinite, infinite, infinite};
    Vec3 high{-infinite, -infinite, -infinite};
    for (int face = 0; face < face_count; ++face)
    {
        reach_of(m_faces.constData() + 3 * face, 3, &face_low[face], &face_high[face]);
        low = {qMin(low.x, face_low.at(face).x), qMin(low.y, face_low.at(face).y), qMin(low.z, face_low.at(face).z)};
        high = {qMax(high.x, face_high.at(face).x), qMax(high.y, face_high.at(face).y),
                qMax(high.z, face_high.at(face).z)};
    }
    for (int edge = 0; edge < edge_count; ++edge)
    {
        reach_of(m_edges.constData() + 2 * edge, 2, &edge_low[edge], &edge_high[edge]);
    }

    // Each vertex and the triangles near it.
    CubeGrid faces(low, high, smallest_cube);
    faces.fill(face_count, [&face_low, &face_high](int face, Vec3* box_low, Vec3* box_high)
    {
        *box_low = face_low.at(face);
        *box_high = face_high.at(face);
    });

    // The vertices and the edges are shared out among threads in runs as long whatever the threads. Each run keeps the
    // contacts it finds in the order of its vertices or edges, so the contacts come out the same.
    struct SearchRun
    {
        int                  begin = 0;
        int                  end = 0;
        QVector<SelfContact> found;
    };
    auto runs_of = [](int item_count)
    {
        QVector<SearchRun> runs;
        for (int begin = 0; begin < item_count; begin += search_run)
        {
            SearchRun run;
            run.begin = begin;
            run.end = qMin(begin + search_run, item_count);
            runs.append(run);
        }
        return runs;
    };
    QVector<SearchRun> vertex_runs = runs_of(count);
    QtConcurrent::blockingMap(vertex_runs, [&](SearchRun& run)
    {
        QVector<int> seen_face(face_count, -1);
        QVector<int> near_face;
        for (int vertex = run.begin; vertex < run.end; ++vertex)
        {
            Vec3 vertex_low;
            Vec3 vertex_high;
            reach_of(&vertex, 1, &vertex_low, &vertex_high);
            near_face.clear();
            faces.forEachCube(vertex_low, vertex_high, [&](int cube)
            {
                for (int i = faces.starts.at(cube); i < faces.starts.at(cube + 1); ++i)
                {
                    const int face = faces.items.at(i);
                    if (seen_face.at(face) != vertex)
                    {
                        seen_face[face] = vertex;
                        if (overlap(vertex_low, vertex_high, face_low.at(face), face_high.at(face)))
                        {
                            near_face.append(face);
                        }
                    }
                }
            });
            for (const int face : near_face)
            {
                const int* corners = m_faces.constData() + 3 * face;
                if (m_settings.pieces_pass_through && m_pieces.at(vertex) != m_pieces.at(corners[0]))
                {
                    continue;
                }
                bool related = false;
                for (int k = 0; k < 3; ++k)
                {
                    related = related || corners[k] == vertex || sewnTogether(vertex, corners[k]);
                }
                related = related || closeAtRest(&vertex, 1, corners, 3);
                if (!related)
                {
                    SelfContact contact;
                    contact.vertices[0] = vertex;
                    std::copy(corners, corners + 3, contact.vertices + 1);
                    if (mayTouch(contact, close_by + heading.at(vertex) + heading_of(corners, 3)))
                    {
                        run.found.append(contact);
                    }
                }
            }
        }
    });
    for (const SearchRun& run : vertex_runs)
    {
        m_self_contacts += run.found;
    }

    // Each edge and the edges near it, each pair once.
    CubeGrid edges(low, high, smallest_cube);
    edges.fill(edge_count, [&edge_low, &edge_high](int edge, Vec3* box_low, Vec3* box_high)
    {
        *box_low = edge_low.at(edge);
        *box_high = edge_high.at(edge);
    });
    QVector<SearchRun> edge_runs = runs_of(edge_count);
    QtConcurrent::blockingMap(edge_runs, [&](SearchRun& run)
    {
        QVector<int> seen(edge_count, -1);
        QVector<int> near_edge;
        for (int edge = run.begin; edge < run.end; ++edge)
        {
            const int* first = m_edges.constData() + 2 * edge;
            near_edge.clear();
            edges.forEachCube(edge_low.at(edge), edge_high.at(edge), [&](int cube)
            {
                for (int i = edges.starts.at(cube); i < edges.starts.at(cube + 1); ++i)
                {
                    const int other = edges.items.at(i);
                    if (other > edge && seen.at(other) != edge)
                    {
                        seen[other] = edge;
                        if (overlap(edge_low.at(edge), edge_high.at(edge), edge_low.at(other), edge_high.at(other)))
                        {
                            near_edge.append(other);
                        }
                    }
                }
            });
            for (const int other : near_edge)
            {
                const int* second = m_edges.constData() + 2 * other;
                if (m_settings.pieces_pass_through && m_pieces.at(first[0]) != m_pieces.at(second[0]))
                {
                    continue;
                }
                bool related = false;
                for (int j = 0; j < 2 && !related; ++j)
                {
                    for (int k = 0; k < 2 && !related; ++k)
                    {
                        related = first[j] == second[k] || sewnTogether(first[j], second[k]);
                    }
                }
                related = related || closeAtRest(first, 2, second, 2);
                if (!related)
                {
                    SelfContact contact;
                    contact.edges = true;
                    contact.vertices[0] = first[0];
                    contact.vertices[1] = first[1];
                    contact.vertices[2] = second[0];
                    contact.vertices[3] = second[1];
                    if (mayTouch(contact, close_by + heading_of(first, 2) + heading_of(second, 2)))
                    {
                        run.found.append(contact);
                    }
                }
            }
        }
    });
    for (const SearchRun& run : edge_runs)
    {
        m_self_contacts += run.found;
    }

    // Which contacts each vertex takes part in.
    m_self_role_start = QVector<int>(count + 1, 0);
    for (const SelfContact& contact : m_self_contacts)
    {
        for (const int vertex : contact.vertices)
        {
            ++m_self_role_start[vertex + 1];
        }
    }
    for (int vertex = 0; vertex < count; ++vertex)
    {
        m_self_role_start[vertex + 1] += m_self_role_start.at(vertex);
    }
    m_self_roles = QVector<SelfContactRole>(m_self_role_start.last());
    QVector<int> next = m_self_role_start;
    for (int c = 0; c < m_self_contacts.size(); ++c)
    {
        for (int k = 0; k < 4; ++k)
        {
            m_self_roles[next[m_self_contacts.at(c).vertices[k]]++] = {c, k};
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Keeps a contact if its parts were no further apart than that when the step started, with the side they were on.
bool ClothSolver::mayTouch(const SelfContact& contact, double furthest) const
{
    Vec3 corners[4];
    for (int k = 0; k < 4; ++k)
    {
        corners[k] = load(m_previous, contact.vertices[k]);
    }
    double weights[4];
    return contactGap(corners, contact.edges, weights).length() <= furthest;
}

//---------------------------------------------------------------------------------------------------------------------
// Notes which side of each other the parts of a contact start the step on and whether they can touch during it, and
// limits how far their vertices may move in it.
void ClothSolver::startSelfContact(SelfContact* contact, const QVector<double>& heading)
{
    Vec3 corners[4];
    for (int k = 0; k < 4; ++k)
    {
        corners[k] = load(m_previous, contact->vertices[k]);
    }
    double weights[4];
    const Vec3 gap = contactGap(corners, contact->edges, weights);
    const Vec3 normal = contactNormal(corners, contact->edges);
    const double along = gap.dot(normal);
    contact->side = 0;
    if (clearNormal(corners, contact->edges, normal) && qAbs(along) >= lying_on * gap.length() * normal.length()
        && qAbs(along) > tiny)
    {
        contact->side = along < 0 ? -1.0 : 1.0;
    }

    // The first part is a vertex or the first edge, the second the rest.
    const int first_count = contact->edges ? 2 : 1;
    double first_heading = 0;
    double second_heading = 0;
    for (int k = 0; k < 4; ++k)
    {
        double& part = k < first_count ? first_heading : second_heading;
        part = qMax(part, heading.at(contact->vertices[k]));
    }
    contact->live = gap.length() <= m_settings.thickness + self_margin / 4.0 + first_heading + second_heading;

    // Only parts lying on each other can pass through each other.
    if (contact->side != 0)
    {
        const double bound = self_bound_share * qMax(gap.length(), m_settings.thickness);
        for (const int vertex : contact->vertices)
        {
            m_self_bound[vertex] = qMin(m_self_bound.at(vertex), bound);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Whether a vertex and a triangle, or two edges, are neighbours: in the same flat piece, or across a seam.
bool ClothSolver::closeAtRest(const int* first, int first_count, const int* second, int second_count) const
{
    const bool same_piece = m_pieces.at(first[0]) == m_pieces.at(second[0]);
    bool close = false;
    for (int j = 0; j < first_count && !close; ++j)
    {
        for (int k = 0; k < second_count && !close; ++k)
        {
            if (same_piece)
            {
                const QPointF apart = m_rest.at(first[j]) - m_rest.at(second[k]);
                close = QPointF::dotProduct(apart, apart) < rest_neighbours * rest_neighbours;
            }
            close = close || m_seam_neighbours.contains(pairKey(first[j], second[k]));
        }
    }
    return close;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether a stitch holds the two vertices together.
bool ClothSolver::sewnTogether(int a, int b) const
{
    const QVector<int>& stitched = m_stitched.at(a);
    return std::binary_search(stitched.cbegin(), stitched.cend(), b);
}
