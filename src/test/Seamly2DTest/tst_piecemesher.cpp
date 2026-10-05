//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_piecemesher.cpp
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

#include "tst_piecemesher.h"

#include <QLineF>
#include <QPolygonF>
#include <QtMath>
#include <QtTest>

#include <algorithm>

#include "../vgarment/piece_mesher.h"
#include "../vgarment/piece_outline.h"
#include "../vgarment/seam_stretch.h"
#include "../vgeometry/vpointf.h"
#include "../vmisc/def.h"
#include "../vmisc/vabstractapplication.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/vpiecepath.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
QVector<QPointF> rectangle(qreal width, qreal height)
{
    return {QPointF(0, 0), QPointF(width, 0), QPointF(width, height), QPointF(0, height)};
}

//---------------------------------------------------------------------------------------------------------------------
// A 30 x 40 cm block with a 15 x 20 cm notch cut out of one corner, 900 square cm in total.
QVector<QPointF> lShape()
{
    return {QPointF(0, 0), QPointF(30, 0), QPointF(30, 20), QPointF(15, 20), QPointF(15, 40), QPointF(0, 40)};
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QPointF> circle(qreal radius, int segments)
{
    QVector<QPointF> points;
    for (int i = 0; i < segments; ++i)
    {
        const qreal angle = 2.0 * M_PI * i / segments;
        points.append(QPointF(radius * qCos(angle), radius * qSin(angle)));
    }
    return points;
}

//---------------------------------------------------------------------------------------------------------------------
// A 30 x 20 cm rectangle with path points 1 to 4 at its corners and a notch, path point 5, 11 cm along its top.
PieceOutline notchedRectangle()
{
    const QVector<QPointF> points = {QPointF(0, 0), QPointF(11, 0), QPointF(30, 0), QPointF(30, 20), QPointF(0, 20)};
    QVector<OutlineNode> nodes;
    const quint32 ids[] = {1, 5, 2, 3, 4};
    for (int i = 0; i < points.size(); ++i)
    {
        OutlineNode node;
        node.id = ids[i];
        node.index = i;
        node.notch = node.id == 5;
        nodes.append(node);
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
qreal polygonArea(const QVector<QPointF>& polygon)
{
    qreal doubled_area = 0;
    for (int i = 0; i < polygon.size(); ++i)
    {
        const QPointF& current = polygon.at(i);
        const QPointF& next = polygon.at((i + 1) % polygon.size());
        doubled_area += current.x() * next.y() - next.x() * current.y();
    }
    return doubled_area / 2.0;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QPointF> boundaryPolygon(const GarmentMesh& mesh)
{
    QVector<QPointF> polygon;
    for (const quint32 index : mesh.boundary)
    {
        polygon.append(mesh.rest_positions.at(static_cast<int>(index)));
    }
    return polygon;
}

//---------------------------------------------------------------------------------------------------------------------
qreal angleAt(const QPointF& corner, const QPointF& a, const QPointF& b)
{
    return QLineF(corner, a).angleTo(QLineF(corner, b));
}

//---------------------------------------------------------------------------------------------------------------------
// Describes the first triangle that is flipped, too big or too thin, or returns an empty string if all are fine.
// Thin means an angle under 10 degrees; that keeps the cloth solver well conditioned.
QString triangleProblem(const GarmentMesh& mesh, qreal edge_length)
{
    QString problem;
    for (int i = 0; i + 2 < mesh.indices.size() && problem.isEmpty(); i += 3)
    {
        const QPointF& a = mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i)));
        const QPointF& b = mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i + 1)));
        const QPointF& c = mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i + 2)));
        const qreal doubled_area = (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
        const qreal longest_edge = qMax(QLineF(a, b).length(), qMax(QLineF(b, c).length(), QLineF(c, a).length()));

        qreal smallest_angle = 180.0;
        for (const qreal angle : {angleAt(a, b, c), angleAt(b, c, a), angleAt(c, a, b)})
        {
            smallest_angle = qMin(smallest_angle, qMin(angle, 360.0 - angle));
        }

        if (doubled_area <= 0)
        {
            problem = QStringLiteral("triangle %1 is flipped").arg(i / 3);
        }
        else if (longest_edge > 2.0 * edge_length)
        {
            problem = QStringLiteral("triangle %1 has a %2 cm edge").arg(i / 3).arg(longest_edge);
        }
        else if (smallest_angle < 10.0)
        {
            problem = QStringLiteral("triangle %1 has a %2 degree angle").arg(i / 3).arg(smallest_angle);
        }
    }
    return problem;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_PieceMesher::TST_PieceMesher(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceMesher::rectangleIsFilled() const
{
    const GarmentMesh mesh = PieceMesher().meshPolygon(rectangle(30, 20));

    QVERIFY(!mesh.isEmpty());
    QCOMPARE(mesh.area(), 600.0);

    // 100 cm of seam line at the default 2 cm spacing
    QCOMPARE(static_cast<int>(mesh.boundary.size()), 50);

    const QString problem = triangleProblem(mesh, PieceMesher::defaultEdgeLength());
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
// The triangulation covers the convex hull; the triangles in the notch must all be gone.
void TST_PieceMesher::concaveOutlineStaysInside() const
{
    const QPolygonF outline(lShape());
    const GarmentMesh mesh = PieceMesher().meshPolygon(lShape());

    QCOMPARE(mesh.area(), 900.0);
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const QPointF centroid = (mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i)))
                                  + mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i + 1)))
                                  + mesh.rest_positions.at(static_cast<int>(mesh.indices.at(i + 2)))) / 3.0;
        QVERIFY2(outline.containsPoint(centroid, Qt::OddEvenFill),
                 qUtf8Printable(QStringLiteral("triangle %1 lies outside the seam line").arg(i / 3)));
    }

    const QString problem = triangleProblem(mesh, PieceMesher::defaultEdgeLength());
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceMesher::cornersAreKept() const
{
    const GarmentMesh mesh = PieceMesher().meshPolygon(lShape());

    for (const QPointF& corner : lShape())
    {
        const bool kept = std::any_of(mesh.rest_positions.cbegin(), mesh.rest_positions.cend(),
                                      [&corner](const QPointF& position)
        {
            return QLineF(position, corner).length() < 1e-9;
        });
        QVERIFY2(kept, qUtf8Printable(QStringLiteral("corner (%1, %2) was moved").arg(corner.x()).arg(corner.y())));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A curve comes in as many short segments; the seam line has to come out evenly spaced at the edge length.
void TST_PieceMesher::curveIsEvenlySampled() const
{
    const qreal edge_length = 2.0;
    const GarmentMesh mesh = PieceMesher(edge_length).meshPolygon(circle(10, 360));
    const QVector<QPointF> seam_line = boundaryPolygon(mesh);

    // circumference of 62.8 cm
    QCOMPARE(static_cast<int>(seam_line.size()), 31);
    for (int i = 0; i < seam_line.size(); ++i)
    {
        const qreal length = QLineF(seam_line.at(i), seam_line.at((i + 1) % seam_line.size())).length();
        QVERIFY2(length > 0.5 * edge_length && length < 1.5 * edge_length,
                 qUtf8Printable(QStringLiteral("seam line segment %1 is %2 cm long").arg(i).arg(length)));
    }

    // The triangles cover exactly what the resampled seam line encloses.
    QCOMPARE(mesh.area(), polygonArea(seam_line));

    const QString problem = triangleProblem(mesh, edge_length);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceMesher::windingDoesNotMatter() const
{
    QVector<QPointF> reversed = lShape();
    std::reverse(reversed.begin(), reversed.end());

    const PieceMesher mesher;
    const GarmentMesh forward = mesher.meshPolygon(lShape());
    const GarmentMesh backward = mesher.meshPolygon(reversed);

    QCOMPARE(backward.area(), forward.area());
    QCOMPARE(backward.triangleCount(), forward.triangleCount());

    // The seam line keeps the outline's direction, the triangles all face the same way.
    QCOMPARE(polygonArea(boundaryPolygon(forward)), forward.area());
    QCOMPARE(polygonArea(boundaryPolygon(backward)), -backward.area());
}

//---------------------------------------------------------------------------------------------------------------------
// Halving the edge length should give about four times the triangles, like halving CLO's particle distance.
void TST_PieceMesher::edgeLengthSetsResolution() const
{
    const GarmentMesh coarse = PieceMesher(2.0).meshPolygon(rectangle(40, 40));
    const GarmentMesh fine = PieceMesher(1.0).meshPolygon(rectangle(40, 40));

    QCOMPARE(fine.area(), 1600.0);
    const qreal ratio = static_cast<qreal>(fine.triangleCount()) / coarse.triangleCount();
    QVERIFY2(ratio > 3.5 && ratio < 4.5, qUtf8Printable(QStringLiteral("triangle count ratio %1").arg(ratio)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceMesher::outlineWithoutAreaGivesEmptyMesh() const
{
    const PieceMesher mesher;

    QVERIFY(mesher.meshPolygon(QVector<QPointF>()).isEmpty());
    QVERIFY(mesher.meshPolygon({QPointF(0, 0), QPointF(10, 0)}).isEmpty());
    QVERIFY(mesher.meshPolygon({QPointF(0, 0), QPointF(10, 0), QPointF(20, 0)}).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// Pattern points are stored in pixels; the mesh has to be in cm and sit where the piece sits in the piece scene.
void TST_PieceMesher::pieceIsMeshedInCentimetres() const
{
    const Unit unit = Unit::Cm;
    QScopedPointer<VContainer> data(new VContainer(nullptr, &unit));
    qApp->setPatternUnit(unit);

    const qreal side = ToPixel(10, Unit::Cm);
    data->UpdateGObject(1, new VPointF(0, 0, QStringLiteral("A1"), 0, 0));
    data->UpdateGObject(2, new VPointF(side, 0, QStringLiteral("A2"), 0, 0));
    data->UpdateGObject(3, new VPointF(side, side, QStringLiteral("A3"), 0, 0));
    data->UpdateGObject(4, new VPointF(0, side, QStringLiteral("A4"), 0, 0));

    VPiece piece;
    piece.SetSeamAllowance(false);
    for (quint32 id = 1; id <= 4; ++id)
    {
        piece.GetPath().Append(VPieceNode(id, Tool::NodePoint));
    }
    piece.SetMx(ToPixel(5, Unit::Cm));
    piece.SetMy(ToPixel(3, Unit::Cm));

    const GarmentMesh mesh = PieceMesher().meshPiece(42, piece, data.data());

    QCOMPARE(mesh.piece_id, 42u);
    QCOMPARE(mesh.area(), 100.0);
    QCOMPARE(mesh.bounds(), QRectF(5, 3, 10, 10));
}

//---------------------------------------------------------------------------------------------------------------------
// Seams are sewn from path point to path point, so every path point needs a vertex of its own, even one on a straight
// edge that resampling would otherwise step over.
void TST_PieceMesher::pathPointsBecomeVertices() const
{
    const PieceOutline outline = notchedRectangle();
    const GarmentMesh mesh = PieceMesher().meshOutline(outline);

    QCOMPARE(mesh.nodes.size(), outline.nodes().size());
    for (int k = 0; k < mesh.nodes.size(); ++k)
    {
        const OutlineNode& node = mesh.nodes.at(k);
        const OutlineNode& path_point = outline.nodes().at(k);
        QCOMPARE(node.id, path_point.id);
        QCOMPARE(node.notch, path_point.notch);

        const QPointF vertex = mesh.rest_positions.at(static_cast<int>(mesh.boundary.at(node.index)));
        QVERIFY2(QLineF(vertex, outline.points().at(path_point.index)).length() < 1e-9,
                 qUtf8Printable(QStringLiteral("path point %1 has no vertex").arg(node.id)));
        QVERIFY(k == 0 || node.index > mesh.nodes.at(k - 1).index);
    }

    const QString problem = triangleProblem(mesh, PieceMesher::defaultEdgeLength());
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceMesher::meshStretchNamesVertices() const
{
    const GarmentMesh mesh = PieceMesher().meshOutline(notchedRectangle());
    const quint32 offset = 100;
    const SeamStretch top = mesh.stretch(1, 2, offset);

    QVERIFY(qAbs(top.length() - 30.0) < 1e-9);
    QCOMPARE(top.notches().size(), 1);
    QVERIFY(qAbs(top.notches().first() - 11.0) < 1e-9);
    QCOMPARE(top.vertices().size(), top.points().size());
    QVERIFY(QLineF(top.points().first(), QPointF(0, 0)).length() < 1e-9);
    QVERIFY(QLineF(top.points().last(), QPointF(30, 0)).length() < 1e-9);
    for (int i = 0; i < top.vertices().size(); ++i)
    {
        const int vertex = static_cast<int>(top.vertices().at(i) - offset);
        QVERIFY(QLineF(mesh.rest_positions.at(vertex), top.points().at(i)).length() < 1e-9);
    }
}
