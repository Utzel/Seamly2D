//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_topstitch.cpp
//  @author Julius
//  @date   6 Oct, 2026
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

#include "tst_topstitch.h"

#include <QLineF>
#include <QtTest>

#include <algorithm>

#include "../vgarment/piece_mesher.h"
#include "../vgarment/topstitch.h"

namespace
{
// Close enough, in cm.
const qreal tolerance = 1e-6;

//---------------------------------------------------------------------------------------------------------------------
// A piece 20 by 10 cm with a path point at each corner, ids 1 to 4, going round the given way.
PieceOutline rectangle(bool anticlockwise = true)
{
    QVector<QPointF> points = {QPointF(0, 0), QPointF(20, 0), QPointF(20, 10), QPointF(0, 10)};
    if (!anticlockwise)
    {
        points = {QPointF(0, 0), QPointF(0, 10), QPointF(20, 10), QPointF(20, 0)};
    }
    QVector<OutlineNode> nodes;
    for (int i = 0; i < points.size(); ++i)
    {
        OutlineNode node;
        node.id = static_cast<quint32>(i + 1);
        node.index = i;
        nodes.append(node);
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
bool samePoint(const QPointF& a, const QPointF& b)
{
    return QLineF(a, b).length() < tolerance;
}

//---------------------------------------------------------------------------------------------------------------------
qreal distanceToRectangle(const QPointF& point)
{
    return qMin(qMin(point.x(), 20.0 - point.x()), qMin(point.y(), 10.0 - point.y()));
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_Topstitch::TST_Topstitch(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// A row along one edge runs the distance inside it, from the edge on one side to the edge on the other, whichever
// way the outline goes round.
void TST_Topstitch::rowRunsInsideOneEdge() const
{
    const QVector<QVector<QPointF>> rows = Topstitching::rows(rectangle(), {true, false, false, false}, 0.6);
    QCOMPARE(rows.size(), 1);
    QVERIFY(samePoint(rows.first().first(), QPointF(0, 0.6)));
    QVERIFY(samePoint(rows.first().last(), QPointF(20, 0.6)));
    for (const QPointF& point : rows.first())
    {
        QVERIFY(qAbs(point.y() - 0.6) < tolerance);
    }

    const QVector<QVector<QPointF>> reversed = Topstitching::rows(rectangle(false), {false, false, false, true}, 0.6);
    QCOMPARE(reversed.size(), 1);
    QVERIFY(samePoint(reversed.first().first(), QPointF(20, 0.6)));
    QVERIFY(samePoint(reversed.first().last(), QPointF(0, 0.6)));
}

//---------------------------------------------------------------------------------------------------------------------
// Two edges stitched one after the other share a row, which turns the corner between them at the distance from both.
void TST_Topstitch::rowTurnsWhereEdgesMeet() const
{
    const QVector<QVector<QPointF>> rows = Topstitching::rows(rectangle(), {true, true, false, false}, 0.6);
    QCOMPARE(rows.size(), 1);
    const QVector<QPointF>& row = rows.first();
    QVERIFY(samePoint(row.first(), QPointF(0, 0.6)));
    QVERIFY(std::any_of(row.cbegin(), row.cend(), [](const QPointF& point)
    {
        return samePoint(point, QPointF(19.4, 0.6));
    }));
    QVERIFY(samePoint(row.last(), QPointF(19.4, 10)));

    const QVector<QVector<QPointF>> apart = Topstitching::rows(rectangle(), {true, false, true, false}, 0.6);
    QCOMPARE(apart.size(), 2);
}

//---------------------------------------------------------------------------------------------------------------------
// With every edge stitched, the row goes all the way round, the distance inside, and closes.
void TST_Topstitch::everyEdgeMakesALoop() const
{
    const QVector<QVector<QPointF>> rows = Topstitching::rows(rectangle(), {true, true, true, true}, 0.6);
    QCOMPARE(rows.size(), 1);
    const QVector<QPointF>& row = rows.first();
    QVERIFY(samePoint(row.first(), row.last()));
    QCOMPARE(row.size(), 5);
    for (const QPointF& point : row)
    {
        QVERIFY(qAbs(distanceToRectangle(point) - 0.6) < tolerance);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A strip narrower than twice the distance has no room for a row all round it, but one along an edge still fits
// as long as it stays inside the strip.
void TST_Topstitch::rowsLeaveOutWhatIsTooNarrow() const
{
    QVector<OutlineNode> nodes(4);
    for (int i = 0; i < nodes.size(); ++i)
    {
        nodes[i].id = static_cast<quint32>(i + 1);
        nodes[i].index = i;
    }
    const PieceOutline strip({QPointF(0, 0), QPointF(20, 0), QPointF(20, 1), QPointF(0, 1)}, nodes);
    QVERIFY(Topstitching::rows(strip, {true, true, true, true}, 0.6).isEmpty());
    QCOMPARE(Topstitching::rows(strip, {true, true, true, true}, 0.3).size(), 1);
    QCOMPARE(Topstitching::rows(strip, {true, false, false, false}, 0.6).size(), 1);
    QVERIFY(Topstitching::rows(strip, {true, false, false, false}, 1.5).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// Stitches of the given length sit in the middle of the row, on the mesh's surface, and move with it.
void TST_Topstitch::stitchesLieOnTheMesh() const
{
    const GarmentMesh mesh = PieceMesher().meshPolygon(rectangle().points());
    const QVector<QVector<QPointF>> rows = {{QPointF(0, 0.6), QPointF(20, 0.6)}};
    const QVector<ThreadStitch> stitches = Topstitching::stitches(mesh, rows, 0.3, 0.12);
    QCOMPARE(stitches.size(), 66);
    QCOMPARE(stitches.first().width, 0.12f);

    const QPointF first = stitches.first().start.restPosition(mesh);
    const QPointF last = stitches.last().end.restPosition(mesh);
    QVERIFY(qAbs(first.x() - (20.0 - stitches.last().end.restPosition(mesh).x())) < 1e-4);
    QVERIFY(first.x() > 0 && last.x() < 20);
    for (const ThreadStitch& stitch : stitches)
    {
        const QPointF start = stitch.start.restPosition(mesh);
        const QPointF middle = stitch.middle.restPosition(mesh);
        const QPointF end = stitch.end.restPosition(mesh);
        QVERIFY(qAbs(start.y() - 0.6) < 1e-4 && qAbs(end.y() - 0.6) < 1e-4);
        QVERIFY(qAbs(QLineF(start, end).length() - 0.24) < 1e-4);
        QVERIFY(samePoint(middle, (start + end) / 2) || QLineF(middle, (start + end) / 2).length() < 1e-4);
    }

    // The mesh stood upright, 5 cm in front: the stitches go with it.
    QVector<QVector3D> positions;
    for (const QPointF& rest : mesh.rest_positions)
    {
        positions.append(QVector3D(static_cast<float>(rest.x()), static_cast<float>(-rest.y()), 5.0f));
    }
    const QVector3D moved = stitches.first().start.position(positions);
    QVERIFY(qAbs(moved.x() - first.x()) < 1e-4 && qAbs(moved.y() + first.y()) < 1e-4 && qAbs(moved.z() - 5) < 1e-4);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Topstitch::pointsOutsideGoToTheEdge() const
{
    const GarmentMesh mesh = PieceMesher().meshPolygon(rectangle().points());
    const QVector<SurfacePoint> located = Topstitching::locate(mesh, {QPointF(-1, 5), QPointF(7.3, 4.1),
                                                                      QPointF(25, 12)});
    QCOMPARE(located.size(), 3);
    QVERIFY(QLineF(located.at(0).restPosition(mesh), QPointF(0, 5)).length() < 1e-4);
    QVERIFY(QLineF(located.at(1).restPosition(mesh), QPointF(7.3, 4.1)).length() < 1e-4);
    QVERIFY(QLineF(located.at(2).restPosition(mesh), QPointF(20, 10)).length() < 1e-4);
}

//---------------------------------------------------------------------------------------------------------------------
// The default style comes first and has one row; every style's rows run inside the edge, one further in than the
// last. A style a pattern names but the 3D View doesn't know is the default.
void TST_Topstitch::stylesRunInsideTheEdge() const
{
    const QVector<TopstitchStyle> styles = TopstitchStyle::presets();
    QVERIFY(styles.size() > 1);
    QCOMPARE(styles.first().name, TopstitchStyle::defaultName());
    QCOMPARE(styles.first().distances, QVector<qreal>({Topstitching::defaultDistance()}));
    for (const TopstitchStyle& style : styles)
    {
        QVERIFY(!style.distances.isEmpty() && style.distances.first() > 0);
        QVERIFY(std::is_sorted(style.distances.cbegin(), style.distances.cend()));
        QVERIFY(style.stitch_length > 0 && style.thread_width > 0);
        QCOMPARE(TopstitchStyle::preset(style.name).name, style.name);
    }
    QCOMPARE(TopstitchStyle::preset(QStringLiteral("unknown")).name, TopstitchStyle::defaultName());
}

//---------------------------------------------------------------------------------------------------------------------
// Each stitch is a spindle on each face of the cloth, five corners and four triangles each, facing away from the
// cloth, its ridge raised in proportion to the thread's width.
void TST_Topstitch::threadLiesOnBothFaces() const
{
    const GarmentMesh mesh = PieceMesher().meshPolygon(rectangle().points());
    const QVector<ThreadStitch> stitches = Topstitching::stitches(mesh, {{QPointF(5, 0.6), QPointF(5.7, 0.6)}}, 0.3,
                                                                  0.1);
    QCOMPARE(stitches.size(), 2);

    // Flat, facing +z.
    QVector<QVector3D> positions;
    for (const QPointF& rest : mesh.rest_positions)
    {
        positions.append(QVector3D(static_cast<float>(rest.x()), static_cast<float>(rest.y()), 0.0f));
    }
    const QVector<QVector3D> normals(positions.size(), QVector3D(0, 0, 1));
    const ThreadMesh thread = Topstitching::threadMesh(stitches, positions, normals);
    QCOMPARE(thread.positions.size(), 2 * 2 * 5);
    QCOMPARE(thread.normals.size(), thread.positions.size());
    QCOMPARE(thread.indices.size(), 2 * 2 * 4 * 3);

    for (int i = 0; i < thread.indices.size(); i += 3)
    {
        const QVector3D& a = thread.positions.at(static_cast<int>(thread.indices.at(i)));
        const QVector3D& b = thread.positions.at(static_cast<int>(thread.indices.at(i + 1)));
        const QVector3D& c = thread.positions.at(static_cast<int>(thread.indices.at(i + 2)));
        const QVector3D face = QVector3D::crossProduct(b - a, c - a);
        const bool front = (i / 12) % 2 == 0;
        QVERIFY(front ? face.z() > 0 : face.z() < 0);
    }
    float highest = 0;
    float lowest = 0;
    for (const QVector3D& position : thread.positions)
    {
        highest = qMax(highest, position.z());
        lowest = qMin(lowest, position.z());
    }
    QVERIFY(highest > 0.05f && highest < 0.1f && qAbs(lowest + highest) < 1e-5f);

    const ThreadMesh thicker = Topstitching::threadMesh(stitches, positions, normals, 2.0);
    float thicker_highest = 0;
    for (const QVector3D& position : thicker.positions)
    {
        thicker_highest = qMax(thicker_highest, position.z());
    }
    QVERIFY(qAbs(thicker_highest - 2 * highest) < 1e-5f);

    // On cloth drawn 0.1 cm thick, the thread lies on its faces, 0.05 cm either side of its middle.
    const ThreadMesh on_thick_cloth = Topstitching::threadMesh(stitches, positions, normals, 1.0, 0.05);
    float thick_highest = 0;
    float thick_lowest = 0;
    for (const QVector3D& position : on_thick_cloth.positions)
    {
        thick_highest = qMax(thick_highest, position.z());
        thick_lowest = qMin(thick_lowest, position.z());
    }
    QVERIFY2(qAbs(thick_highest - (highest + 0.05f)) < 1e-5f && qAbs(thick_lowest + thick_highest) < 1e-5f,
             qUtf8Printable(QStringLiteral("the thread reaches from %1 to %2 cm").arg(thick_lowest)
                                .arg(thick_highest)));
}
