//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_clothsolver.cpp
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

#include "tst_clothsolver.h"

#include <QLineF>
#include <QtMath>
#include <QtTest>

#include <limits>

#include "../vgarment/body_collider.h"
#include "../vgarment/cloth_solver.h"
#include "../vgarment/garment_mesh.h"
#include "../vgarment/piece_mesher.h"
#include "../vgarment/piece_outline.h"

namespace
{
const qreal frame = 1.0 / 60.0;

//---------------------------------------------------------------------------------------------------------------------
// A rectangle with path points first_id to first_id + 3 at its corners, clockwise from the top left.
PieceOutline rectangle(qreal left, qreal top, qreal width, qreal height, quint32 first_id)
{
    const QVector<QPointF> points = {QPointF(left, top), QPointF(left + width, top),
                                     QPointF(left + width, top + height), QPointF(left, top + height)};
    QVector<OutlineNode> nodes;
    for (int i = 0; i < points.size(); ++i)
    {
        OutlineNode node;
        node.id = first_id + static_cast<quint32>(i);
        node.index = i;
        nodes.append(node);
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
// The piece lying flat at a height, its y running along z.
QVector<QVector3D> lyingFlat(const GarmentMesh& mesh, qreal height)
{
    QVector<QVector3D> positions;
    for (const QPointF& point : mesh.rest_positions)
    {
        positions.append(QVector3D(static_cast<float>(point.x()), static_cast<float>(height),
                                   static_cast<float>(point.y())));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// The piece standing upright facing the viewer, as on the board of pieces.
QVector<QVector3D> standing(const GarmentMesh& mesh)
{
    QVector<QVector3D> positions;
    for (const QPointF& point : mesh.rest_positions)
    {
        positions.append(QVector3D(static_cast<float>(point.x()), static_cast<float>(-point.y()), 0.0f));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// How much the most stretched mesh edge is longer than at rest, as a share of its length. Squeezed edges don't count:
// cloth pushed together buckles into folds.
qreal worstStrain(const GarmentMesh& mesh, const QVector<QVector3D>& positions, int offset = 0)
{
    qreal worst = 0;
    for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const int a = static_cast<int>(mesh.indices.at(t + k));
            const int b = static_cast<int>(mesh.indices.at(t + (k + 1) % 3));
            const qreal rest = QLineF(mesh.rest_positions.at(a), mesh.rest_positions.at(b)).length();
            const qreal now = (positions.at(offset + a) - positions.at(offset + b)).length();
            worst = qMax(worst, now / rest - 1.0);
        }
    }
    return worst;
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D centroid(const QVector<QVector3D>& positions)
{
    QVector3D sum;
    for (const QVector3D& position : positions)
    {
        sum += position;
    }
    return positions.isEmpty() ? sum : sum / static_cast<float>(positions.size());
}

//---------------------------------------------------------------------------------------------------------------------
// A sphere around the origin, its triangles counter-clockwise seen from outside.
BodyCollider sphere(float radius)
{
    const int rings = 24;
    const int segments = 48;
    QVector<QVector3D> positions{QVector3D(0, radius, 0)};
    for (int ring = 1; ring < rings; ++ring)
    {
        const qreal polar = M_PI * ring / rings;
        for (int segment = 0; segment < segments; ++segment)
        {
            const qreal azimuth = 2.0 * M_PI * segment / segments;
            positions.append(QVector3D(static_cast<float>(radius * qSin(polar) * qCos(azimuth)),
                                       static_cast<float>(radius * qCos(polar)),
                                       static_cast<float>(radius * qSin(polar) * qSin(azimuth))));
        }
    }
    positions.append(QVector3D(0, -radius, 0));

    auto ring_vertex = [](int ring, int segment)
    {
        return static_cast<quint32>(1 + (ring - 1) * segments + (segment % segments));
    };
    const quint32 bottom = static_cast<quint32>(positions.size() - 1);

    QVector<quint32> triangles;
    for (int segment = 0; segment < segments; ++segment)
    {
        triangles << 0 << ring_vertex(1, segment + 1) << ring_vertex(1, segment);
        triangles << bottom << ring_vertex(rings - 1, segment) << ring_vertex(rings - 1, segment + 1);
        for (int ring = 1; ring + 1 < rings; ++ring)
        {
            const quint32 a = ring_vertex(ring, segment);
            const quint32 b = ring_vertex(ring, segment + 1);
            const quint32 c = ring_vertex(ring + 1, segment);
            const quint32 d = ring_vertex(ring + 1, segment + 1);
            triangles << a << b << c << b << d << c;
        }
    }
    return BodyCollider(positions, triangles);
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_ClothSolver::TST_ClothSolver(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// Without anything holding it, cloth falls like anything else and keeps its shape.
void TST_ClothSolver::freeFallFollowsGravity() const
{
    ClothSettings settings;
    settings.air_damping = 0;
    settings.floor = false;
    ClothSolver solver(settings);
    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(0, 0, 10, 10, 1));
    solver.addMesh(mesh, lyingFlat(mesh, 100));

    const int steps = 30;
    for (int i = 0; i < steps; ++i)
    {
        solver.step(frame);
    }

    // Implicit Euler falls g h² n (n + 1) / 2 in n steps.
    const qreal expected = 981.0 * frame * frame * steps * (steps + 1) / 2.0;
    const qreal fallen = 100.0 - centroid(solver.positions()).y();
    QVERIFY2(qAbs(fallen - expected) < 0.01 * expected,
             qUtf8Printable(QStringLiteral("fell %1 cm instead of %2").arg(fallen).arg(expected)));
    QVERIFY(worstStrain(mesh, solver.positions()) < 0.001);
}

//---------------------------------------------------------------------------------------------------------------------
// Held along one edge, a cloth swings down, hangs straight and comes to rest without stretching much.
void TST_ClothSolver::pinnedClothHangs() const
{
    ClothSolver solver;
    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(0, 0, 20, 20, 1));
    solver.addMesh(mesh, lyingFlat(mesh, 100));
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        solver.setPinned(static_cast<quint32>(i), qAbs(mesh.rest_positions.at(i).y()) < 1e-9);
    }

    for (int i = 0; i < 240; ++i)
    {
        solver.step(frame);
    }

    const QVector<QVector3D> positions = solver.positions();
    float lowest = std::numeric_limits<float>::max();
    for (const QVector3D& position : positions)
    {
        lowest = qMin(lowest, position.y());
    }
    float fastest = 0;
    for (const QVector3D& velocity : solver.velocities())
    {
        fastest = qMax(fastest, velocity.length());
    }

    QVERIFY2(lowest < 82.0f, qUtf8Printable(QStringLiteral("the lowest point is at %1 cm").arg(lowest)));
    const qreal strain = worstStrain(mesh, positions);
    QVERIFY2(strain < 0.05, qUtf8Printable(QStringLiteral("an edge is stretched by %1 %").arg(strain * 100)));
    QVERIFY2(fastest < 5.0f, qUtf8Printable(QStringLiteral("still moving at %1 cm/s").arg(fastest)));
}

//---------------------------------------------------------------------------------------------------------------------
// Two pieces 5 cm apart, the right side of one sewn to the left side of the other, tops together.
void TST_ClothSolver::stitchesCloseTheGap() const
{
    ClothSettings settings;
    settings.gravity = QVector3D();
    settings.floor = false;
    ClothSolver solver(settings);

    const PieceMesher mesher;
    const GarmentMesh left = mesher.meshOutline(rectangle(0, 0, 10, 10, 1));
    const GarmentMesh right = mesher.meshOutline(rectangle(15, 0, 10, 10, 11));
    const quint32 left_offset = solver.addMesh(left, standing(left));
    const quint32 right_offset = solver.addMesh(right, standing(right));

    // The left piece's right side runs top to bottom from point 2 to 3; the right piece's left side runs bottom to
    // top from point 14 to 11, so it is turned round to start at the top as well.
    const SeamStretch first = left.stretch(2, 3, left_offset);
    const SeamStretch second = right.stretch(14, 11, right_offset).reversed();
    const QVector<Stitch> stitches = SeamStretch::stitches(first, second);
    QVERIFY(!stitches.isEmpty());
    solver.addStitches(stitches);

    for (int i = 0; i < 120; ++i)
    {
        solver.step(frame);
    }

    const QVector<QVector3D> positions = solver.positions();
    float widest = 0;
    for (const Stitch& stitch : stitches)
    {
        const QVector3D target = positions.at(static_cast<int>(stitch.edge_start)) * (1.0f - stitch.along)
                                 + positions.at(static_cast<int>(stitch.edge_end)) * stitch.along;
        widest = qMax(widest, (positions.at(static_cast<int>(stitch.vertex)) - target).length());
    }
    QVERIFY2(widest < 0.3f, qUtf8Printable(QStringLiteral("a stitch is still %1 cm open").arg(widest)));
    QVERIFY(worstStrain(left, positions, static_cast<int>(left_offset)) < 0.1);
    QVERIFY(worstStrain(right, positions, static_cast<int>(right_offset)) < 0.1);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::colliderMeasuresDistance() const
{
    const BodyCollider ball = sphere(10);

    BodyContact contact;
    const QVector3D above(0, 15, 0);
    QVERIFY(ball.trianglesWithin(above, 1).isEmpty());
    QVERIFY(ball.closest(above, ball.trianglesWithin(above, 6), &contact));
    QVERIFY2(qAbs(contact.distance - 5.0f) < 0.1f, qUtf8Printable(QString::number(contact.distance)));
    QVERIFY(contact.normal.y() > 0.99f);

    const QVector3D inside(3, 0, 0);
    QVERIFY(ball.closest(inside, ball.trianglesWithin(inside, 8), &contact));
    QVERIFY2(qAbs(contact.distance + 7.0f) < 0.1f, qUtf8Printable(QString::number(contact.distance)));
    QVERIFY(contact.normal.x() > 0.99f);
}

//---------------------------------------------------------------------------------------------------------------------
// A cloth dropped onto a ball comes to rest on top of it, hanging down around it, without going in.
void TST_ClothSolver::clothRestsOnSphere() const
{
    const float radius = 10;
    ClothSettings settings;
    settings.floor = false;
    ClothSolver solver(settings);
    solver.setCollider(sphere(radius));

    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(-15, -15, 30, 30, 1));
    solver.addMesh(mesh, lyingFlat(mesh, 12));

    for (int i = 0; i < 180; ++i)
    {
        solver.step(frame);
    }

    const QVector<QVector3D> positions = solver.positions();
    float nearest = std::numeric_limits<float>::max();
    float lowest = std::numeric_limits<float>::max();
    float highest = -std::numeric_limits<float>::max();
    for (const QVector3D& position : positions)
    {
        nearest = qMin(nearest, position.length());
        lowest = qMin(lowest, position.y());
        highest = qMax(highest, position.y());
    }

    QVERIFY2(nearest > radius - 0.1f, qUtf8Printable(QStringLiteral("a vertex is %1 cm from the middle").arg(nearest)));
    QVERIFY2(highest > radius && highest < radius + 1.0f, qUtf8Printable(QStringLiteral("top at %1").arg(highest)));
    QVERIFY2(lowest < 2.0f, qUtf8Printable(QStringLiteral("the cloth only hangs down to %1 cm").arg(lowest)));
    const qreal strain = worstStrain(mesh, positions);
    QVERIFY2(strain < 0.1, qUtf8Printable(QStringLiteral("an edge is stretched by %1 %").arg(strain * 100)));
}
