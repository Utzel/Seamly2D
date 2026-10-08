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
#include "../vgarment/compute_device.h"
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

//---------------------------------------------------------------------------------------------------------------------
// A drape test runs on the processor, and again with the sweeps on the graphics card.
void onProcessorAndDevice()
{
    QTest::addColumn<bool>("on_device");
    QTest::newRow("processor") << false;
    QTest::newRow("graphics card") << true;
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
// Losing all its speed in every step, a cloth moves only as far as each step's forces take it: under gravity g h² a
// step, however long it has been falling.
void TST_ClothSolver::fullAirDampingKeepsNoSpeed() const
{
    ClothSettings settings;
    settings.floor = false;
    ClothSolver solver(settings);
    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(0, 0, 10, 10, 1));
    solver.addMesh(mesh, lyingFlat(mesh, 100));
    solver.setAirDamping(1.0 / frame);

    const int steps = 30;
    for (int i = 0; i < steps; ++i)
    {
        solver.step(frame);
    }

    const qreal expected = 981.0 * frame * frame * steps;
    const qreal fallen = 100.0 - centroid(solver.positions()).y();
    QVERIFY2(qAbs(fallen - expected) < 0.01 * expected,
             qUtf8Printable(QStringLiteral("fell %1 cm instead of %2").arg(fallen).arg(expected)));
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
void TST_ClothSolver::heldClothFollowsTheHand_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// A cloth hanging from its top edge, taken by a corner of its bottom edge and pulled 10 cm forward, follows the hand:
// the corner is where it is held, the cloth beside it comes along, the top edge stays. Let go, it swings back.
void TST_ClothSolver::heldClothFollowsTheHand() const
{
    QFETCH(bool, on_device);
    ComputeDevice device;
    ClothSolver solver;
    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(0, 0, 20, 20, 1));
    const QVector<QVector3D> flat = lyingFlat(mesh, 100);
    solver.addMesh(mesh, flat);
    auto nearest = [&mesh](const QPointF& rest)
    {
        int found = 0;
        for (int i = 1; i < mesh.vertexCount(); ++i)
        {
            if (QLineF(mesh.rest_positions.at(i), rest).length() < QLineF(mesh.rest_positions.at(found), rest).length())
            {
                found = i;
            }
        }
        return static_cast<quint32>(found);
    };
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        solver.setPinned(static_cast<quint32>(i), qAbs(mesh.rest_positions.at(i).y()) < 1e-9);
    }
    const quint32 corner = nearest(QPointF(20, 20));
    const quint32 beside = nearest(QPointF(17, 20));
    const quint32 top = nearest(QPointF(10, 0));
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }

    for (int i = 0; i < 180; ++i)
    {
        solver.step(frame);
    }
    const QVector3D hanging = solver.position(corner);
    const QVector3D beside_hanging = solver.position(beside);

    // The hanging cloth lies across z; the hand pulls the corner out of it over half a second, then holds it there.
    const QVector3D hand = hanging + QVector3D(0, 0, 10);
    solver.setPinned(corner, true);
    for (int i = 1; i <= 30; ++i)
    {
        solver.moveVertex(corner, hanging + (hand - hanging) * (static_cast<float>(i) / 30.0f));
        solver.step(frame);
    }
    for (int i = 0; i < 60; ++i)
    {
        solver.step(frame);
    }
    const QVector3D held = solver.position(corner);
    const float beside_moved = qAbs(solver.position(beside).z() - beside_hanging.z());
    const QString report = QStringLiteral("corner held at %1 cm from the hand, beside it moved %2 cm, top edge moved %3 cm")
                               .arg((held - hand).length()).arg(beside_moved)
                               .arg((solver.position(top) - flat.at(static_cast<int>(top))).length());
    QVERIFY2((held - hand).length() < 1e-3f, qUtf8Printable(report));
    QVERIFY2(beside_moved > 5.0f, qUtf8Printable(report));
    QVERIFY2((solver.position(top) - flat.at(static_cast<int>(top))).length() < 1e-3f, qUtf8Printable(report));

    solver.setPinned(corner, false);
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);
    const float back = qAbs(solver.position(corner).z() - hanging.z());
    QVERIFY2(back < 2.0f, qUtf8Printable(QStringLiteral("let go, the corner is %1 cm off where it hung").arg(back)));
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
void TST_ClothSolver::clothRestsOnSphere_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// A cloth dropped onto a ball comes to rest on top of it, hanging down around it, without going in.
void TST_ClothSolver::clothRestsOnSphere() const
{
    QFETCH(bool, on_device);
    const float radius = 10;
    ClothSettings settings;
    settings.floor = false;
    ComputeDevice device;
    ClothSolver solver(settings);
    solver.setCollider(sphere(radius));

    const GarmentMesh mesh = PieceMesher().meshOutline(rectangle(-15, -15, 30, 30, 1));
    solver.addMesh(mesh, lyingFlat(mesh, 12));
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }

    for (int i = 0; i < 180; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

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

//---------------------------------------------------------------------------------------------------------------------
// A sheet dropped onto another lying on the floor comes to rest on top of it, a thickness above; without self contact
// it falls through onto the floor.
void TST_ClothSolver::clothLandsOnCloth() const
{
    for (const bool self_contact : {true, false})
    {
        ClothSettings settings;
        settings.self_contact = self_contact;
        ClothSolver solver(settings);

        const PieceMesher mesher;
        const GarmentMesh lower = mesher.meshOutline(rectangle(0, 0, 30, 30, 1));
        const GarmentMesh upper = mesher.meshOutline(rectangle(5, 5, 20, 20, 11));
        solver.addMesh(lower, lyingFlat(lower, settings.thickness));
        const int upper_offset = static_cast<int>(solver.addMesh(upper, lyingFlat(upper, 5)));

        for (int i = 0; i < 120; ++i)
        {
            solver.step(frame);
        }

        const QVector<QVector3D> positions = solver.positions();
        float upper_lowest = std::numeric_limits<float>::max();
        for (int i = upper_offset; i < positions.size(); ++i)
        {
            upper_lowest = qMin(upper_lowest, positions.at(i).y());
        }
        float lower_highest = -std::numeric_limits<float>::max();
        for (int i = 0; i < upper_offset; ++i)
        {
            lower_highest = qMax(lower_highest, positions.at(i).y());
        }

        const QString report = QStringLiteral("with self contact %1: the upper sheet's lowest at %2 cm, the lower's "
                                              "highest at %3 cm").arg(self_contact).arg(upper_lowest)
                                   .arg(lower_highest);
        if (self_contact)
        {
            QVERIFY2(upper_lowest > lower_highest + 0.5f * static_cast<float>(settings.thickness)
                         && upper_lowest < 1.5f, qUtf8Printable(report));
        }
        else
        {
            QVERIFY2(upper_lowest < lower_highest, qUtf8Printable(report));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::foldedClothKeepsItsLayers_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// A strip folded over onto itself keeps its two layers apart as the upper one settles onto the lower one.
void TST_ClothSolver::foldedClothKeepsItsLayers() const
{
    QFETCH(bool, on_device);
    ClothSettings settings;
    ComputeDevice device;
    ClothSolver solver(settings);

    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 40, 10, 1));
    QVector<QVector3D> folded;
    for (const QPointF& point : strip.rest_positions)
    {
        const bool over = point.x() > 20;
        folded.append(QVector3D(static_cast<float>(over ? 40 - point.x() : point.x()),
                                static_cast<float>(over ? 3.0 : settings.thickness), static_cast<float>(point.y())));
    }
    solver.addMesh(strip, folded);
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }

    for (int i = 0; i < 120; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    // Away from the fold, which cloth that resists bending turns in a loop a few cm wide, the folded over half lies on
    // the other, about a thickness above it, and nowhere sinks into it. Where exactly the upper half sags most, and
    // where the lower one bulges, hangs on the slightest difference as the strip settles, so the two halves are
    // measured as a whole.
    const QVector<QVector3D> positions = solver.positions();
    float upper_lowest = std::numeric_limits<float>::max();
    float upper_height = 0;
    float lower_height = 0;
    int upper_count = 0;
    int lower_count = 0;
    for (int i = 0; i < strip.vertexCount(); ++i)
    {
        const qreal x = strip.rest_positions.at(i).x();
        if (x > 30)
        {
            upper_lowest = qMin(upper_lowest, positions.at(i).y());
            upper_height += positions.at(i).y();
            ++upper_count;
        }
        else if (x < 10)
        {
            lower_height += positions.at(i).y();
            ++lower_count;
        }
    }
    upper_height /= static_cast<float>(qMax(1, upper_count));
    lower_height /= static_cast<float>(qMax(1, lower_count));
    const float thickness = static_cast<float>(settings.thickness);
    const float gap = upper_height - lower_height;
    QVERIFY2(gap > 0.5f * thickness && gap < 2.0f * thickness && upper_lowest > lower_height,
             qUtf8Printable(QStringLiteral("the upper half lies %1 cm above the lower one, its lowest at %2 cm, the "
                                           "lower one at %3 cm").arg(gap).arg(upper_lowest).arg(lower_height)));
}

//---------------------------------------------------------------------------------------------------------------------
// While pieces pass through each other, as while they are sewn together, a strip folded over onto itself still keeps
// its layers apart, and a sheet between them sinks through the lower one to the floor.
void TST_ClothSolver::piecesPassThroughEachOtherButNotThemselves() const
{
    ClothSettings settings;
    settings.pieces_pass_through = true;
    ClothSolver solver(settings);

    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 40, 10, 1));
    QVector<QVector3D> folded;
    for (const QPointF& point : strip.rest_positions)
    {
        const bool over = point.x() > 20;
        folded.append(QVector3D(static_cast<float>(over ? 40 - point.x() : point.x()),
                                static_cast<float>(over ? 3.0 : settings.thickness), static_cast<float>(point.y())));
    }
    solver.addMesh(strip, folded);
    const GarmentMesh sheet = PieceMesher().meshOutline(rectangle(4, 2, 10, 6, 11));
    const quint32 sheet_offset = solver.addMesh(sheet, lyingFlat(sheet, 1.5));
    for (int i = 0; i < 120; ++i)
    {
        solver.step(frame);
    }

    const QVector<QVector3D> positions = solver.positions();
    float upper = 0;
    float lower = 0;
    int upper_count = 0;
    int lower_count = 0;
    for (int i = 0; i < strip.vertexCount(); ++i)
    {
        const qreal x = strip.rest_positions.at(i).x();
        if (x > 30)
        {
            upper += positions.at(i).y();
            ++upper_count;
        }
        else if (x < 10)
        {
            lower += positions.at(i).y();
            ++lower_count;
        }
    }
    upper /= static_cast<float>(qMax(1, upper_count));
    lower /= static_cast<float>(qMax(1, lower_count));
    float sheet_height = 0;
    for (int i = 0; i < sheet.vertexCount(); ++i)
    {
        sheet_height += positions.at(static_cast<int>(sheet_offset) + i).y() / static_cast<float>(sheet.vertexCount());
    }
    QVERIFY2(upper > lower + 0.5f * static_cast<float>(settings.thickness)
                 && sheet_height < lower + 0.5f * static_cast<float>(settings.thickness),
             qUtf8Printable(QStringLiteral("the strip's halves are at %1 and %2 cm, the sheet at %3 cm")
                                .arg(upper).arg(lower).arg(sheet_height)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::layersKeepTheirOrder_data() const
{
    QTest::addColumn<int>("sheet_layer");
    QTest::addColumn<bool>("sheet_turned");
    QTest::addColumn<int>("patch_layer");
    QTest::addColumn<bool>("patch_turned");
    QTest::addColumn<qreal>("start");
    QTest::addColumn<bool>("sewing");
    QTest::addColumn<bool>("over");
    QTest::addColumn<bool>("on_device");
    for (const bool on_device : {false, true})
    {
        const QString device = on_device ? QStringLiteral(", graphics card") : QString();
        QTest::newRow(qPrintable(QStringLiteral("outer layer starting under") + device))
            << 0 << false << 1 << false << -0.1 << false << true << on_device;
        QTest::newRow(qPrintable(QStringLiteral("inner layer starting over") + device))
            << 1 << false << 0 << false << 0.1 << false << false << on_device;
        QTest::newRow(qPrintable(QStringLiteral("outer layer turned over, starting over") + device))
            << 0 << true << 1 << true << 0.1 << false << false << on_device;
        QTest::newRow(qPrintable(QStringLiteral("layers facing different ways") + device))
            << 0 << false << 1 << true << -0.1 << false << false << on_device;
        QTest::newRow(qPrintable(QStringLiteral("one layer") + device))
            << 0 << false << 0 << false << -0.1 << false << false << on_device;
        QTest::newRow(qPrintable(QStringLiteral("outer layer starting under, while sewn") + device))
            << 0 << false << 1 << false << -0.1 << true << true << on_device;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A patch starting just over or under a sheet held flat, its right side up, ends up on the side their layers say: the
// higher layer outside, on the side the other piece turns out, whichever side it starts on, and even while pieces
// pass through each other otherwise, as they are sewn together. Pieces of one layer, or facing different ways, keep
// the sides they start on, here the patch falling away under the sheet.
void TST_ClothSolver::layersKeepTheirOrder() const
{
    QFETCH(int, sheet_layer);
    QFETCH(bool, sheet_turned);
    QFETCH(int, patch_layer);
    QFETCH(bool, patch_turned);
    QFETCH(qreal, start);
    QFETCH(bool, sewing);
    QFETCH(bool, over);
    QFETCH(bool, on_device);

    ClothSettings settings;
    settings.floor = false;
    settings.pieces_pass_through = sewing;
    ClothSolver solver(settings);
    const PieceMesher mesher;
    const GarmentMesh sheet = mesher.meshOutline(rectangle(0, 0, 30, 30, 1));
    const GarmentMesh patch = mesher.meshOutline(rectangle(5, 5, 20, 20, 11));
    ClothLayer sheet_wears;
    sheet_wears.number = sheet_layer;
    sheet_wears.turned_over = sheet_turned;
    ClothLayer patch_wears;
    patch_wears.number = patch_layer;
    patch_wears.turned_over = patch_turned;
    solver.addMesh(sheet, lyingFlat(sheet, 0), Fabric(), 90.0, {}, sheet_wears);
    const int patch_offset = static_cast<int>(solver.addMesh(patch, lyingFlat(patch, start), Fabric(), 90.0, {},
                                                             patch_wears));
    for (int i = 0; i < patch_offset; ++i)
    {
        solver.setPinned(static_cast<quint32>(i), true);
    }
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 120; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    float lowest = std::numeric_limits<float>::max();
    float highest = -std::numeric_limits<float>::max();
    for (int i = patch_offset; i < positions.size(); ++i)
    {
        lowest = qMin(lowest, positions.at(i).y());
        highest = qMax(highest, positions.at(i).y());
    }
    const float thickness = static_cast<float>(settings.thickness);
    const QString report = QStringLiteral("the patch is from %1 to %2 cm high, the sheet at 0").arg(lowest)
                               .arg(highest);
    if (over)
    {
        QVERIFY2(lowest > 0.5f * thickness && highest < 3.0f * thickness, qUtf8Printable(report));
    }
    else
    {
        QVERIFY2(highest < -0.5f * thickness, qUtf8Printable(report));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::piecesGatherIntoOneSeam_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// Two panels 20 cm wide sewn along their tops, one after the other, to the 20 cm lower edge of a band held up, as a
// gathered skirt's panels are to a waistband: the seam closes, the panels' 40 cm gathered into the band's 20, the
// first panel's outer corner at the band's start and the second's at its end.
void TST_ClothSolver::piecesGatherIntoOneSeam() const
{
    QFETCH(bool, on_device);
    ClothSettings settings;
    settings.floor = false;
    ClothSolver solver(settings);
    const PieceMesher mesher;
    const GarmentMesh band = mesher.meshOutline(rectangle(0, -4, 20, 4, 1));
    const GarmentMesh left = mesher.meshOutline(rectangle(-10, 0, 20, 15, 11));
    const GarmentMesh right = mesher.meshOutline(rectangle(10, 0, 20, 15, 21));
    const quint32 band_offset = solver.addMesh(band, standing(band));
    const quint32 left_offset = solver.addMesh(left, standing(left));
    const quint32 right_offset = solver.addMesh(right, standing(right));

    // The band's lower edge runs from its right end to its left, so it is turned to run as the panels' tops do.
    const SeamStretch tops = SeamStretch::joined({left.stretch(11, 12, left_offset),
                                                  right.stretch(21, 22, right_offset)});
    solver.addStitches(SeamStretch::stitches(tops, band.stretch(3, 4, band_offset).reversed()));
    for (int i = 0; i < band.vertexCount(); ++i)
    {
        if (band.rest_positions.at(i).y() < -3.99)
        {
            solver.setPinned(band_offset + static_cast<quint32>(i), true);
        }
    }
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    const qreal widest = solver.widestStitch();
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    auto at = [&positions](const GarmentMesh& mesh, quint32 offset, const QPointF& rest)
    {
        int nearest = 0;
        for (int i = 0; i < mesh.vertexCount(); ++i)
        {
            nearest = QLineF(mesh.rest_positions.at(i), rest).length()
                              < QLineF(mesh.rest_positions.at(nearest), rest).length() ? i : nearest;
        }
        return positions.at(static_cast<int>(offset) + nearest);
    };
    const float start_apart = (at(left, left_offset, QPointF(-10, 0)) - at(band, band_offset, QPointF(0, 0))).length();
    const float end_apart = (at(right, right_offset, QPointF(30, 0)) - at(band, band_offset, QPointF(20, 0))).length();
    const float gathered = (at(right, right_offset, QPointF(30, 0)) - at(left, left_offset, QPointF(-10, 0))).length();
    QVERIFY2(widest < 0.5 && start_apart < 0.5f && end_apart < 0.5f && gathered < 21.0f,
             qUtf8Printable(QStringLiteral("the widest stitch is %1 cm, the panels' outer corners %2 and %3 cm from "
                                           "the band's ends, %4 cm apart").arg(widest).arg(start_apart).arg(end_apart)
                                .arg(gathered)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::elasticGathersTheCloth_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// An elastic half as long as the top edge of a 40 cm strip, sewn along it, gathers it to about 20 cm; the strip's bottom
// edge, without, stays about as wide as it is drafted.
void TST_ClothSolver::elasticGathersTheCloth() const
{
    QFETCH(bool, on_device);
    ClothSettings settings;
    settings.floor = false;
    settings.gravity = QVector3D();
    ClothSolver solver(settings);
    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 40, 10, 1));
    solver.addMesh(strip, standing(strip));
    ClothElastic elastic;
    elastic.vertices = strip.stretch(1, 2).vertices();
    elastic.ratio = 0.5;
    solver.addElastic(elastic);
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    auto at = [&strip, &positions](const QPointF& rest)
    {
        int nearest = 0;
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            nearest = QLineF(strip.rest_positions.at(i), rest).length()
                              < QLineF(strip.rest_positions.at(nearest), rest).length() ? i : nearest;
        }
        return positions.at(nearest);
    };
    const float top = (at(QPointF(40, 0)) - at(QPointF(0, 0))).length();
    const float bottom = (at(QPointF(40, 10)) - at(QPointF(0, 10))).length();
    QVERIFY2(top > 16.0f && top < 25.0f && bottom > 30.0f,
             qUtf8Printable(QStringLiteral("the top edge is %1 cm wide, the bottom %2").arg(top).arg(bottom)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::shrinkageResizesTheCloth_data() const
{
    QTest::addColumn<qreal>("weft");
    QTest::addColumn<qreal>("warp");
    QTest::addColumn<bool>("on_device");
    for (const bool on_device : {false, true})
    {
        const QString device = on_device ? QStringLiteral(", graphics card") : QString();
        QTest::newRow(qPrintable(QStringLiteral("shrunk across the grain") + device)) << 0.8 << 1.0 << on_device;
        QTest::newRow(qPrintable(QStringLiteral("stretched out along the grain") + device)) << 1.0 << 1.2 << on_device;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A 20 cm square of a fabric that shrinks comes to that share of its size across the grain, the weft, and along it,
// the warp: its grain runs up the piece.
void TST_ClothSolver::shrinkageResizesTheCloth() const
{
    QFETCH(qreal, weft);
    QFETCH(qreal, warp);
    QFETCH(bool, on_device);
    ClothSettings settings;
    settings.floor = false;
    settings.gravity = QVector3D();
    ClothSolver solver(settings);
    const GarmentMesh square = PieceMesher().meshOutline(rectangle(0, 0, 20, 20, 1));
    Fabric fabric;
    fabric.shrinkage_weft = weft;
    fabric.shrinkage_warp = warp;
    solver.addMesh(square, standing(square), fabric, 90.0);
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    auto at = [&square, &positions](const QPointF& rest)
    {
        int nearest = 0;
        for (int i = 0; i < square.vertexCount(); ++i)
        {
            nearest = QLineF(square.rest_positions.at(i), rest).length()
                              < QLineF(square.rest_positions.at(nearest), rest).length() ? i : nearest;
        }
        return positions.at(nearest);
    };
    const float across = (at(QPointF(20, 10)) - at(QPointF(0, 10))).length();
    const float along = (at(QPointF(10, 20)) - at(QPointF(10, 0))).length();
    QVERIFY2(qAbs(across - 20.0f * static_cast<float>(weft)) < 0.5f
                 && qAbs(along - 20.0f * static_cast<float>(warp)) < 0.5f,
             qUtf8Printable(QStringLiteral("the square is %1 cm across the grain and %2 cm along it").arg(across)
                                .arg(along)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::seamsCloseDespiteSelfContact_data() const
{
    onProcessorAndDevice();
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth keeping its thickness from itself doesn't hold seams open: sewn sides close as tightly as without it.
void TST_ClothSolver::seamsCloseDespiteSelfContact() const
{
    QFETCH(bool, on_device);
    ComputeDevice device;
    if (on_device && device.open() == nullptr)
    {
        QSKIP("No graphics card here can compute");
    }
    float widest[2] = {0, 0};
    for (const bool self_contact : {true, false})
    {
        ClothSettings settings;
        settings.gravity = QVector3D();
        settings.floor = false;
        settings.self_contact = self_contact;
        ClothSolver solver(settings);

        const PieceMesher mesher;
        const GarmentMesh left = mesher.meshOutline(rectangle(0, 0, 10, 10, 1));
        const GarmentMesh right = mesher.meshOutline(rectangle(15, 0, 10, 10, 11));
        const quint32 left_offset = solver.addMesh(left, standing(left));
        const quint32 right_offset = solver.addMesh(right, standing(right));
        const QVector<Stitch> stitches = SeamStretch::stitches(left.stretch(2, 3, left_offset),
                                                               right.stretch(14, 11, right_offset).reversed());
        solver.addStitches(stitches);
        QCOMPARE(solver.useDevice(device.rhi()), on_device);

        for (int i = 0; i < 120; ++i)
        {
            solver.step(frame);
        }
        QCOMPARE(solver.isOnDevice(), on_device);
        solver.useDevice(nullptr);

        const QVector<QVector3D> positions = solver.positions();
        for (const Stitch& stitch : stitches)
        {
            const QVector3D target = positions.at(static_cast<int>(stitch.edge_start)) * (1.0f - stitch.along)
                                     + positions.at(static_cast<int>(stitch.edge_end)) * stitch.along;
            widest[self_contact ? 0 : 1] = qMax(widest[self_contact ? 0 : 1],
                                                (positions.at(static_cast<int>(stitch.vertex)) - target).length());
        }
    }
    QVERIFY2(widest[0] < 0.1f && widest[0] < widest[1] + 0.01f,
             qUtf8Printable(QStringLiteral("a stitch is %1 cm open with self contact, %2 cm without")
                 .arg(widest[0]).arg(widest[1])));
}

//---------------------------------------------------------------------------------------------------------------------
// Two strips of the same fabric hang from their top edge, one cut along the grain, the other on the bias. A fabric
// that hardly gives on the bias stretches far more there under its own weight.
void TST_ClothSolver::biasGivesMoreThanGrain() const
{
    Fabric fabric;
    fabric.weight = 400;
    fabric.warp_stiffness = 100;
    fabric.weft_stiffness = 100;
    fabric.bias_stiffness = 5;

    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 6, 40, 1));
    auto hang = [&strip, &fabric](qreal grain_angle)
    {
        ClothSettings settings;
        settings.floor = false;
        ClothSolver solver(settings);
        solver.addMesh(strip, standing(strip), fabric, grain_angle);
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            solver.setPinned(static_cast<quint32>(i), qAbs(strip.rest_positions.at(i).y()) < 1e-9);
        }
        for (int i = 0; i < 300; ++i)
        {
            solver.step(frame);
        }
        float lowest = std::numeric_limits<float>::max();
        for (const QVector3D& position : solver.positions())
        {
            lowest = qMin(lowest, position.y());
        }
        return -lowest;
    };

    const float on_grain = hang(90);
    const float on_bias = hang(45);
    QVERIFY2(on_grain > 40.0f && on_grain < 41.0f,
             qUtf8Printable(QStringLiteral("the strip cut on the grain hangs %1 cm long").arg(on_grain)));
    QVERIFY2(on_bias > on_grain + 3.0f,
             qUtf8Printable(QStringLiteral("the strip cut on the bias hangs %1 cm long, on the grain %2 cm")
                                .arg(on_bias).arg(on_grain)));
}

//---------------------------------------------------------------------------------------------------------------------
// Strips held level along one end droop under their own weight: chiffon hangs nearly straight down, denim still
// reaches out.
void TST_ClothSolver::stifferFabricBendsLess() const
{
    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 7, 8, 1));
    auto tip = [&strip](const QString& fabric_name)
    {
        ClothSettings settings;
        settings.floor = false;
        ClothSolver solver(settings);
        solver.addMesh(strip, lyingFlat(strip, 0), Fabric::preset(fabric_name));
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            solver.setPinned(static_cast<quint32>(i), strip.rest_positions.at(i).x() < 2.5);
        }
        for (int i = 0; i < 300; ++i)
        {
            solver.step(frame);
        }
        // The middle of the free end.
        const QVector<QVector3D> positions = solver.positions();
        int end = 0;
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            end = QLineF(strip.rest_positions.at(i), QPointF(7, 4)).length()
                          < QLineF(strip.rest_positions.at(end), QPointF(7, 4)).length() ? i : end;
        }
        return positions.at(end);
    };

    const QVector3D denim = tip(QStringLiteral("denim"));
    const QVector3D chiffon = tip(QStringLiteral("chiffon"));
    QVERIFY2(chiffon.y() < -3.5f && denim.x() > chiffon.x() + 1.5f,
             qUtf8Printable(QStringLiteral("the denim's tip is at (%1, %2) cm, the chiffon's at (%3, %4) cm")
                                .arg(denim.x()).arg(denim.y()).arg(chiffon.x()).arg(chiffon.y())));
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric stiff to bend along its grain but limp across it, as some twills and ribbed knits are: held level along one
// end, a strip cut along the grain reaches out about as far as one of a fabric that stiff every way, and one cut
// across the grain droops about as far as one of a fabric that limp.
void TST_ClothSolver::bendingFollowsTheGrain() const
{
    const GarmentMesh strip = PieceMesher().meshOutline(rectangle(0, 0, 7, 8, 1));
    auto tip = [&strip](qreal bending_warp, qreal bending_weft, qreal grain_angle)
    {
        ClothSettings settings;
        settings.floor = false;
        ClothSolver solver(settings);
        Fabric fabric = Fabric::preset(QStringLiteral("chiffon"));
        fabric.bending_warp = bending_warp;
        fabric.bending_weft = bending_weft;
        solver.addMesh(strip, lyingFlat(strip, 0), fabric, grain_angle);
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            solver.setPinned(static_cast<quint32>(i), strip.rest_positions.at(i).x() < 2.5);
        }
        for (int i = 0; i < 300; ++i)
        {
            solver.step(frame);
        }
        const QVector<QVector3D> positions = solver.positions();
        int end = 0;
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            end = QLineF(strip.rest_positions.at(i), QPointF(7, 4)).length()
                          < QLineF(strip.rest_positions.at(end), QPointF(7, 4)).length() ? i : end;
        }
        return positions.at(end);
    };

    const QVector3D stiff = tip(30, 30, 0);
    const QVector3D limp = tip(0.5, 0.5, 0);
    const QVector3D along = tip(30, 0.5, 0);
    const QVector3D across = tip(30, 0.5, 90);
    QVERIFY2(stiff.y() > limp.y() + 1.5f && along.y() > limp.y() + 0.8f * (stiff.y() - limp.y())
                 && across.y() < limp.y() + 0.2f * (stiff.y() - limp.y()),
             qUtf8Printable(QStringLiteral("the tips droop to %1 cm stiff, %2 cm limp, %3 cm cut along the grain, "
                                           "%4 cm cut across it").arg(stiff.y()).arg(limp.y()).arg(along.y())
                                .arg(across.y())));
}

//---------------------------------------------------------------------------------------------------------------------
// Pulled on the bias, a woven's threads turn rather than stretch: its shear stiffness follows from its bias
// stiffness, and never comes out harder than stretching across the grain.
void TST_ClothSolver::shearFollowsFromBias() const
{
    Fabric fabric;
    fabric.warp_stiffness = 2000;
    fabric.weft_stiffness = 1200;
    fabric.bias_stiffness = 150;
    QVERIFY2(qAbs(fabric.shearStiffness() - 1.0 / (4.0 / 150 - 1.0 / 2000 - 1.0 / 1200)) < 1e-9,
             qUtf8Printable(QStringLiteral("shear stiffness %1 N/m").arg(fabric.shearStiffness())));

    fabric.bias_stiffness = 2000;
    QCOMPARE(fabric.shearStiffness(), 1200.0);

    QVERIFY(Fabric::presets().size() >= 5);
    QCOMPARE(Fabric::presets().first().name, Fabric::defaultName());
    QCOMPARE(Fabric::preset(QStringLiteral("denim")).name, QStringLiteral("denim"));
    QCOMPARE(Fabric::preset(QStringLiteral("no such fabric")).name, Fabric::defaultName());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::foldsHoldTheirAngle_data() const
{
    QTest::addColumn<qreal>("angle");
    QTest::addColumn<bool>("on_device");
    const QVector<QPair<QString, qreal>> folds = {{QStringLiteral("right side in at a right angle"), 90.0},
                                                  {QStringLiteral("wrong side in at a right angle"), 270.0},
                                                  {QStringLiteral("right side almost onto itself"), 20.0},
                                                  {QStringLiteral("wrong side almost onto itself"), 340.0},
                                                  {QStringLiteral("right side onto itself"), 0.0},
                                                  {QStringLiteral("wrong side onto itself"), 360.0}};
    for (const bool on_device : {false, true})
    {
        for (const auto& fold : folds)
        {
            const QString row = fold.first + (on_device ? QStringLiteral(", graphics card") : QString());
            QTest::newRow(qPrintable(row)) << fold.second << on_device;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A strip lying flat, folded across its middle, comes to the fold's angle on the side it says: the halves meet at the
// angle on the right side, the side the piece is drafted from, or at its rest to 360 on the wrong side. Folded onto
// itself, it folds the way it is told, a little short of flat.
void TST_ClothSolver::foldsHoldTheirAngle() const
{
    QFETCH(qreal, angle);
    QFETCH(bool, on_device);

    PieceOutline outline = rectangle(0, 0, 20, 10, 1);
    OutlineLine line;
    line.id = 9;
    line.points = {QPointF(10, 0), QPointF(10, 10)};
    outline.setLines({line});
    const GarmentMesh strip = PieceMesher().meshOutline(outline);
    QCOMPARE(strip.lines.size(), 1);

    ClothSettings settings;
    settings.floor = false;
    settings.self_contact = false;
    settings.gravity = QVector3D();
    ClothSolver solver(settings);
    ClothFold fold;
    fold.vertices = strip.lines.first().vertices;
    fold.angle = angle;
    solver.addMesh(strip, standing(strip), Fabric(), 90.0, {fold});
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    auto at = [&strip, &positions](const QPointF& rest)
    {
        int nearest = 0;
        for (int i = 0; i < strip.vertexCount(); ++i)
        {
            nearest = QLineF(strip.rest_positions.at(i), rest).length()
                              < QLineF(strip.rest_positions.at(nearest), rest).length() ? i : nearest;
        }
        return positions.at(nearest);
    };

    // The halves' far ends seen from the fold, across it; the right side of the left half, which faces the viewer
    // as the strip starts out.
    const QVector3D top = at(QPointF(10, 0));
    const QVector3D along = (at(QPointF(10, 10)) - top).normalized();
    const QVector3D middle = (top + at(QPointF(10, 10))) / 2.0f;
    auto across = [&along, &middle](const QVector3D& point)
    {
        const QVector3D offset = point - middle;
        return (offset - along * QVector3D::dotProduct(offset, along)).normalized();
    };
    const QVector3D left = across(at(QPointF(0, 5)));
    const QVector3D right = across(at(QPointF(20, 5)));
    const QVector3D right_side = QVector3D::crossProduct(at(QPointF(0, 5)) - at(QPointF(0, 0)),
                                                         at(QPointF(5, 0)) - at(QPointF(0, 0))).normalized();
    const qreal between = qRadiansToDegrees(qAcos(qBound(-1.0f, QVector3D::dotProduct(left, right), 1.0f)));
    const qreal wanted = qMax(5.0, angle <= 180.0 ? angle : 360.0 - angle);
    const float towards = QVector3D::dotProduct(right, right_side);
    QVERIFY2(qAbs(between - wanted) < 10.0 && (angle < 180.0 ? towards > 0 : towards < 0),
             qUtf8Printable(QStringLiteral("the halves meet at %1 degrees, the right half %2 the right side")
                                .arg(between).arg(towards > 0 ? QStringLiteral("towards") : QStringLiteral("away from"))));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_ClothSolver::seamsHoldTheirAngle_data() const
{
    QTest::addColumn<qreal>("angle");
    QTest::addColumn<bool>("seam_folds");
    QTest::addColumn<bool>("on_device");
    for (const bool on_device : {false, true})
    {
        const QString where = on_device ? QStringLiteral(", graphics card") : QString();
        QTest::newRow(qPrintable(QStringLiteral("right side in at a right angle") + where)) << 90.0 << true << on_device;
        QTest::newRow(qPrintable(QStringLiteral("wrong side in at a right angle") + where)) << 270.0 << true
                                                                                          << on_device;
        QTest::newRow(qPrintable(QStringLiteral("turned, wrong sides together") + where)) << 360.0 << true
                                                                                         << on_device;
        QTest::newRow(qPrintable(QStringLiteral("not while sewing") + where)) << 90.0 << false << on_device;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Two squares sewn edge to edge, lying flat, come to the seam's angle on the first one's right side, as a fold does;
// with seams not holding their angles, as while the pieces are sewn together, they stay flat.
void TST_ClothSolver::seamsHoldTheirAngle() const
{
    QFETCH(qreal, angle);
    QFETCH(bool, seam_folds);
    QFETCH(bool, on_device);

    const GarmentMesh left = PieceMesher().meshOutline(rectangle(0, 0, 10, 10, 1));
    const GarmentMesh right = PieceMesher().meshOutline(rectangle(10, 0, 10, 10, 11));
    ClothSettings settings;
    settings.floor = false;
    settings.self_contact = false;
    settings.gravity = QVector3D();
    settings.seam_folds = seam_folds;
    ClothSolver solver(settings);
    const quint32 left_offset = solver.addMesh(left, standing(left));
    const quint32 right_offset = solver.addMesh(right, standing(right));
    const SeamStretch first = left.stretch(2, 3, left_offset);
    const SeamStretch second = right.stretch(14, 11, right_offset).reversed();
    solver.addStitches(SeamStretch::stitches(first, second));
    solver.addSeamFold(first, second, angle);
    ComputeDevice device;
    if (on_device && !solver.useDevice(device.open()))
    {
        QSKIP("No graphics card here can compute");
    }
    for (int i = 0; i < 300; ++i)
    {
        solver.step(frame);
    }
    QCOMPARE(solver.isOnDevice(), on_device);
    solver.useDevice(nullptr);

    const QVector<QVector3D> positions = solver.positions();
    auto at = [&positions](const GarmentMesh& mesh, quint32 offset, const QPointF& rest)
    {
        int nearest = 0;
        for (int i = 0; i < mesh.vertexCount(); ++i)
        {
            nearest = QLineF(mesh.rest_positions.at(i), rest).length()
                              < QLineF(mesh.rest_positions.at(nearest), rest).length() ? i : nearest;
        }
        return positions.at(static_cast<int>(offset) + nearest);
    };
    const QVector3D top = at(left, left_offset, QPointF(10, 0));
    const QVector3D along = (at(left, left_offset, QPointF(10, 10)) - top).normalized();
    const QVector3D middle = (top + at(left, left_offset, QPointF(10, 10))) / 2.0f;
    auto across = [&along, &middle](const QVector3D& point)
    {
        const QVector3D offset = point - middle;
        return (offset - along * QVector3D::dotProduct(offset, along)).normalized();
    };
    const QVector3D first_way = across(at(left, left_offset, QPointF(0, 5)));
    const QVector3D second_way = across(at(right, right_offset, QPointF(20, 5)));
    const QVector3D right_side = QVector3D::crossProduct(at(left, left_offset, QPointF(0, 5))
                                                             - at(left, left_offset, QPointF(0, 0)),
                                                         at(left, left_offset, QPointF(5, 0))
                                                             - at(left, left_offset, QPointF(0, 0))).normalized();
    const qreal between = qRadiansToDegrees(qAcos(qBound(-1.0f, QVector3D::dotProduct(first_way, second_way), 1.0f)));
    const qreal wanted = !seam_folds ? 180.0 : qMax(5.0, angle <= 180.0 ? angle : 360.0 - angle);
    const float towards = QVector3D::dotProduct(second_way, right_side);
    const bool side_right = !seam_folds || (angle < 180.0 ? towards > 0 : towards < 0);
    QVERIFY2(qAbs(between - wanted) < 10.0 && side_right,
             qUtf8Printable(QStringLiteral("the squares meet at %1 degrees, the second %2 the first's right side")
                                .arg(between).arg(towards > 0 ? QStringLiteral("towards") : QStringLiteral("away from"))));
}

//---------------------------------------------------------------------------------------------------------------------
// The sweeps on the graphics card move the cloth as those on the processor do, through every force there is: a strip
// folded onto itself along a fold lies on the floor, two sheets sewn together lie on a ball, one corner pinned.
// Without the floor and self contact, they agree within what single precision tells apart. Where cloth rests on cloth
// or the floor, a thickness away, the sweeps stop short of where the forces exactly balance, and whether a contact
// counts right at that distance tips the other way now and then; over a few steps, before the cloth crumples and which
// way it folds hangs on the slightest difference, it stays close.
void TST_ClothSolver::deviceSweepsAsProcessor() const
{
    ComputeDevice device;
    if (device.open() == nullptr)
    {
        QSKIP("No graphics card here can compute");
    }

    const PieceMesher mesher;
    PieceOutline strip_outline = rectangle(-30, 15, 40, 10, 1);
    OutlineLine fold_line;
    fold_line.id = 5;
    fold_line.points = {QPointF(-10, 15), QPointF(-10, 25)};
    strip_outline.setLines({fold_line});
    const GarmentMesh strip = mesher.meshOutline(strip_outline);
    ClothFold fold;
    fold.vertices = strip.lines.value(0).vertices;
    fold.angle = 0;
    fold.strength = 10;
    const GarmentMesh left = mesher.meshOutline(rectangle(-10, -10, 10, 20, 11));
    const GarmentMesh right = mesher.meshOutline(rectangle(0, -10, 10, 20, 21));
    auto sweep = [&](ClothSettings settings, bool on_device, int steps, QVector<QVector3D>* positions)
    {
        settings.floor_height = -12;
        ClothSolver solver(settings);
        solver.setCollider(sphere(10));

        // The strip folded in half, right side in, the folded over half a thickness above the other.
        const float thickness = static_cast<float>(settings.thickness);
        const float lowest = static_cast<float>(settings.floor_height) + thickness;
        QVector<QVector3D> folded;
        for (const QPointF& point : strip.rest_positions)
        {
            const bool over = point.x() > -10;
            folded.append(QVector3D(static_cast<float>(over ? -20 - point.x() : point.x()),
                                    over ? lowest + thickness : lowest, static_cast<float>(point.y())));
        }
        solver.addMesh(strip, folded, Fabric(), 90.0, {fold});
        const quint32 left_offset = solver.addMesh(left, lyingFlat(left, 10.0 + settings.thickness));
        const quint32 right_offset = solver.addMesh(right, lyingFlat(right, 10.0 + settings.thickness));
        const SeamStretch sewn_left = left.stretch(12, 13, left_offset);
        const SeamStretch sewn_right = right.stretch(24, 21, right_offset).reversed();
        solver.addStitches(SeamStretch::stitches(sewn_left, sewn_right));
        solver.addSeamFold(sewn_left, sewn_right, 180.0);
        solver.setPinned(left_offset, true);
        if (on_device && !solver.useDevice(device.rhi()))
        {
            return false;
        }
        for (int i = 0; i < steps; ++i)
        {
            solver.step(frame);
        }
        *positions = solver.positions();
        const bool stayed = solver.isOnDevice() == on_device;
        solver.useDevice(nullptr);
        return stayed;
    };
    auto apart = [](const QVector<QVector3D>& here, const QVector<QVector3D>& there, float* mean, float* most)
    {
        *mean = 0;
        *most = 0;
        for (int i = 0; i < here.size() && i < there.size(); ++i)
        {
            const float distance = (here.at(i) - there.at(i)).length();
            *mean += distance / static_cast<float>(here.size());
            *most = qMax(*most, distance);
        }
    };

    ClothSettings without_rests;
    without_rests.floor = false;
    without_rests.self_contact = false;
    ClothSettings everything;
    QVector<QVector3D> on_processor;
    QVector<QVector3D> on_device;
    float mean = 0;
    float most = 0;

    QVERIFY(sweep(without_rests, false, 1, &on_processor));
    QVERIFY2(sweep(without_rests, true, 1, &on_device), qUtf8Printable(device.name()));
    QCOMPARE(on_device.size(), on_processor.size());
    apart(on_device, on_processor, &mean, &most);
    QVERIFY2(most < 0.005f, qUtf8Printable(QStringLiteral("without the floor and self contact, on %1 a vertex ends up "
                                                          "%2 cm from where it does on the processor")
                                               .arg(device.name()).arg(most)));

    QVERIFY(sweep(everything, false, 3, &on_processor));
    QVERIFY2(sweep(everything, true, 3, &on_device), qUtf8Printable(device.name()));
    apart(on_device, on_processor, &mean, &most);
    QVERIFY2(mean < 0.03f && most < 0.3f,
             qUtf8Printable(QStringLiteral("on %1 the vertices end up %2 cm on average and up to %3 cm from where "
                                           "they do on the processor").arg(device.name()).arg(mean).arg(most)));
}
