//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_garmentsymmetry.cpp
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

#include "tst_garmentsymmetry.h"

#include <QLineF>
#include <QtMath>
#include <QtTest>

#include "../vgarment/garment_mesh.h"
#include "../vgarment/garment_symmetry.h"
#include "../vgarment/piece_mesher.h"
#include "../vgarment/piece_outline.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// Half a front, cut on the fold: its centre front runs straight down the right side from path point 2 to 3.
//
//     1 ------- 2
//     |         |
//     5         |
//      \        |
//       4 ----- 3
PieceOutline frontHalf()
{
    const QVector<QPointF> points = {QPointF(0, 0), QPointF(20, 0), QPointF(20, 40), QPointF(5, 40), QPointF(0, 20)};
    const quint32 ids[] = {1, 2, 3, 4, 5};
    QVector<OutlineNode> nodes;
    for (int i = 0; i < points.size(); ++i)
    {
        OutlineNode node;
        node.id = ids[i];
        node.index = i;
        nodes.append(node);
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
qreal area(const QVector<QPointF>& polygon)
{
    qreal doubled = 0;
    for (int i = 0; i < polygon.size(); ++i)
    {
        const QPointF& current = polygon.at(i);
        const QPointF& next = polygon.at((i + 1) % polygon.size());
        doubled += current.x() * next.y() - next.x() * current.y();
    }
    return qAbs(doubled) / 2.0;
}

//---------------------------------------------------------------------------------------------------------------------
const OutlineNode* findNode(const PieceOutline& outline, quint32 id)
{
    const OutlineNode* found = nullptr;
    for (const OutlineNode& node : outline.nodes())
    {
        found = node.id == id ? &node : found;
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
GarmentSeam seam(quint32 first_piece, quint32 first_start, quint32 first_end, quint32 second_piece,
                 quint32 second_start, quint32 second_end, bool reverse)
{
    GarmentSeam made;
    made.first = {first_piece, first_start, first_end};
    made.second = {second_piece, second_start, second_end};
    made.reverse = reverse;
    return made;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_GarmentSymmetry::TST_GarmentSymmetry(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentSymmetry::foldLineIsTheLongestStraightSide() const
{
    quint32 start = 0;
    quint32 end = 0;
    QVERIFY(frontHalf().findFoldLine(&start, &end));
    QCOMPARE(start, 2u);
    QCOMPARE(end, 3u);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentSymmetry::noFoldLineOnACurvedPiece() const
{
    QVector<QPointF> circle;
    QVector<OutlineNode> nodes;
    for (int i = 0; i < 36; ++i)
    {
        circle.append(QPointF(10 * qCos(i * M_PI / 18), 10 * qSin(i * M_PI / 18)));
        if (i % 9 == 0)
        {
            OutlineNode node;
            node.id = static_cast<quint32>(i + 1);
            node.index = i;
            nodes.append(node);
        }
    }

    quint32 start = 0;
    quint32 end = 0;
    QVERIFY(!PieceOutline(circle, nodes).findFoldLine(&start, &end));
}

//---------------------------------------------------------------------------------------------------------------------
// Unfolded across its centre front, the half becomes the whole front: twice as big, its other half mirrored, the
// centre front gone inside it.
void TST_GarmentSymmetry::unfoldingMirrorsTheHalf() const
{
    const PieceOutline half = frontHalf();
    const PieceOutline whole = half.unfolded(2, 3);

    QCOMPARE(whole.points().size(), 8);
    QVERIFY(qAbs(area(whole.points()) - 2 * area(half.points())) < 1e-9);

    // The fold's ends are shared, the other path points come twice.
    for (const quint32 id : {1u, 2u, 3u, 4u, 5u})
    {
        QVERIFY(findNode(whole, id) != nullptr);
    }
    for (const quint32 id : {1u, 4u, 5u})
    {
        QVERIFY(findNode(whole, PieceOutline::mirrorId(id)) != nullptr);
    }
    QVERIFY(findNode(whole, PieceOutline::mirrorId(2)) == nullptr);
    QVERIFY(findNode(whole, PieceOutline::mirrorId(3)) == nullptr);

    const OutlineNode* top_left = findNode(whole, PieceOutline::mirrorId(1));
    QVERIFY(QLineF(whole.points().at(top_left->index), QPointF(40, 0)).length() < 1e-9);
    const OutlineNode* hem = findNode(whole, PieceOutline::mirrorId(4));
    QVERIFY(QLineF(whole.points().at(hem->index), QPointF(35, 40)).length() < 1e-9);
}

//---------------------------------------------------------------------------------------------------------------------
// The side seam from 5 up to 1 is there on both halves; on the mirrored one it runs from the top down.
void TST_GarmentSymmetry::unfoldedSeamsRunOnBothHalves() const
{
    const PieceOutline whole = frontHalf().unfolded(2, 3);

    const SeamStretch side = whole.stretch(5, 1);
    QVERIFY(qAbs(side.length() - 20) < 1e-9);
    QVERIFY(QLineF(side.points().first(), QPointF(0, 20)).length() < 1e-9);

    const SeamStretch mirrored_side = whole.stretch(PieceOutline::mirrorId(1), PieceOutline::mirrorId(5));
    QVERIFY(qAbs(mirrored_side.length() - 20) < 1e-9);
    QVERIFY(QLineF(mirrored_side.points().first(), QPointF(40, 0)).length() < 1e-9);
    QVERIFY(QLineF(mirrored_side.points().last(), QPointF(40, 20)).length() < 1e-9);
}

//---------------------------------------------------------------------------------------------------------------------
// A line inside the half comes twice on the whole, the second mirrored and known by the mirrored id; a line on the
// fold only once.
void TST_GarmentSymmetry::unfoldingMirrorsTheLines() const
{
    PieceOutline half = frontHalf();
    OutlineLine dart;
    dart.id = 30;
    dart.points = {QPointF(10, 10), QPointF(20, 12)};
    OutlineLine centre;
    centre.id = 31;
    centre.points = {QPointF(20, 5), QPointF(20, 30)};
    half.setLines({dart, centre});

    const PieceOutline whole = half.unfolded(2, 3);
    QCOMPARE(whole.lines().size(), 3);
    QCOMPARE(whole.lines().at(0), dart);
    const OutlineLine& mirrored = whole.lines().at(1);
    QCOMPARE(mirrored.id, PieceOutline::mirrorId(30));
    QVERIFY(QLineF(mirrored.points.first(), QPointF(30, 10)).length() < 1e-9);
    QVERIFY(QLineF(mirrored.points.last(), QPointF(20, 12)).length() < 1e-9);
    QCOMPARE(whole.lines().at(2), centre);

    // Meshed, the lines meet where they end on the fold, and the line along the fold passes through there.
    const GarmentMesh mesh = PieceMesher().meshOutline(whole);
    QCOMPARE(mesh.lines.size(), 3);
    QCOMPARE(mesh.lines.at(0).vertices.last(), mesh.lines.at(1).vertices.last());
    QVERIFY(mesh.lines.at(2).vertices.contains(mesh.lines.at(0).vertices.last()));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentSymmetry::mirroredMeshFacesTheSameWay() const
{
    const GarmentMesh mesh = PieceMesher().meshOutline(frontHalf());
    const GarmentMesh mirror = mesh.mirrored();

    QCOMPARE(mirror.vertexCount(), mesh.vertexCount());
    QVERIFY(qAbs(mirror.area() - mesh.area()) < 1e-9);
    QVERIFY(mirror.area() > 0);
    QCOMPARE(mirror.boundary, mesh.boundary);
    QCOMPARE(mirror.nodes, mesh.nodes);
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        QCOMPARE(mirror.rest_positions.at(i), QPointF(-mesh.rest_positions.at(i).x(), mesh.rest_positions.at(i).y()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentSymmetry::seamsBetweenPairsGetTwins() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(10, PieceSymmetry::Pair);
    symmetry.setPiece(20, PieceSymmetry::Pair);

    GarmentSeam turned = seam(10, 1, 2, 20, 3, 4, true);
    turned.angle = 360;
    const QVector<GarmentSeam> made_up = symmetry.madeUp({turned});
    QCOMPARE(made_up.size(), 2);
    const GarmentSeam& twin = made_up.at(1);
    QCOMPARE(twin.first, GarmentSeamSide({PieceOutline::mirrorId(10), 1, 2}));
    QCOMPARE(twin.second, GarmentSeamSide({PieceOutline::mirrorId(20), 3, 4}));
    QVERIFY(twin.reverse);
    QCOMPARE(twin.angle, 360.0);
}

//---------------------------------------------------------------------------------------------------------------------
// On the mirrored half of a piece cut on the fold a side runs the other way: its ends swap, and the twin seam is
// turned unless the other side is turned too.
void TST_GarmentSymmetry::seamsOnFoldsGetTurnedTwins() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(10, PieceSymmetry::Fold, 2, 3);
    symmetry.setPiece(20, PieceSymmetry::Pair);
    symmetry.setPiece(30, PieceSymmetry::Fold, 7, 8);

    const QVector<GarmentSeam> with_pair = symmetry.madeUp({seam(10, 5, 1, 20, 3, 4, false)});
    QCOMPARE(with_pair.size(), 2);
    QCOMPARE(with_pair.at(1).first, GarmentSeamSide({10, PieceOutline::mirrorId(1), PieceOutline::mirrorId(5)}));
    QCOMPARE(with_pair.at(1).second, GarmentSeamSide({PieceOutline::mirrorId(20), 3, 4}));
    QVERIFY(with_pair.at(1).reverse);

    // The fold line's ends are on both halves.
    const QVector<GarmentSeam> at_fold = symmetry.madeUp({seam(10, 1, 2, 30, 6, 7, false)});
    QCOMPARE(at_fold.size(), 2);
    QCOMPARE(at_fold.at(1).first, GarmentSeamSide({10, 2, PieceOutline::mirrorId(1)}));
    QCOMPARE(at_fold.at(1).second, GarmentSeamSide({30, 7, PieceOutline::mirrorId(6)}));
    QVERIFY(!at_fold.at(1).reverse);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentSymmetry::piecesCutOnceHaveNoTwins() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(20, PieceSymmetry::Pair);

    QCOMPARE(symmetry.madeUp({seam(10, 1, 2, 20, 3, 4, false)}).size(), 1);
    QVERIFY(symmetry.symmetry(10) == PieceSymmetry::Single);
}

//---------------------------------------------------------------------------------------------------------------------
// A centre back seam: the back's centre back sewn to itself means sewn to the other back's.
void TST_GarmentSymmetry::sideSewnToItselfMeetsItsMirror() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(10, PieceSymmetry::Pair);

    const QVector<GarmentSeam> made_up = symmetry.madeUp({seam(10, 1, 2, 10, 1, 2, false)});
    QCOMPARE(made_up.size(), 1);
    QCOMPARE(made_up.first().first, GarmentSeamSide({10, 1, 2}));
    QCOMPARE(made_up.first().second, GarmentSeamSide({PieceOutline::mirrorId(10), 1, 2}));

    QVERIFY(GarmentSymmetry().madeUp({seam(10, 1, 2, 10, 1, 2, false)}).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// A sleeve's cap sewn to the front's armhole and on to the back's, the back cut on the fold: the twin sews the other
// sleeve to the other front and on to the back's mirrored half, which runs the other way there.
void TST_GarmentSymmetry::seamsOverSeveralStretchesGetTwins() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(10, PieceSymmetry::Pair);
    symmetry.setPiece(20, PieceSymmetry::Pair);
    symmetry.setPiece(30, PieceSymmetry::Fold, 7, 8);

    GarmentSeam cap = seam(10, 1, 2, 20, 3, 4, false);
    cap.second_more = {{30, 5, 6, false}};
    cap.angle = 180;
    QVector<GarmentSeam> made_up = symmetry.madeUp({cap});
    QCOMPARE(made_up.size(), 2);
    const GarmentSeam& twin = made_up.at(1);
    QCOMPARE(twin.first, GarmentSeamSide({PieceOutline::mirrorId(10), 1, 2, false}));
    QCOMPARE(twin.second, GarmentSeamSide({PieceOutline::mirrorId(20), 3, 4, false}));
    QCOMPARE(twin.second_more,
             QVector<GarmentSeamSide>({{30, PieceOutline::mirrorId(6), PieceOutline::mirrorId(5), true}}));
    QVERIFY(twin.first_more.isEmpty());
    QVERIFY(!twin.reverse);
    QCOMPARE(twin.angle, 180.0);

    // Starting on the back, the twin's second side starts on its mirrored half, running backward there, and goes on
    // to the other front in the same order.
    GarmentSeam from_back = seam(10, 1, 2, 30, 5, 6, true);
    from_back.second_more = {{20, 3, 4, false}};
    made_up = symmetry.madeUp({from_back});
    QCOMPARE(made_up.size(), 2);
    QCOMPARE(made_up.at(1).second, GarmentSeamSide({30, PieceOutline::mirrorId(6), PieceOutline::mirrorId(5), true}));
    QCOMPARE(made_up.at(1).second_more, QVector<GarmentSeamSide>({{PieceOutline::mirrorId(20), 3, 4, false}}));
    QVERIFY(made_up.at(1).reverse);

    // On a mirror image, a stretch's mirror image is on the piece itself.
    GarmentSeamSide mirror;
    bool turned = true;
    QVERIFY(symmetry.mirrored({PieceOutline::mirrorId(20), 3, 4, false}, &mirror, &turned));
    QCOMPARE(mirror, GarmentSeamSide({20, 3, 4, false}));
    QVERIFY(!turned);

    QCOMPARE(reversedSide({{20, 3, 4, false}, {30, 5, 6, true}}),
             QVector<GarmentSeamSide>({{30, 5, 6, false}, {20, 3, 4, true}}));
}

//---------------------------------------------------------------------------------------------------------------------
// A waistband sewn to both fronts, the second a mirror image, has no twin: it would sew the same fronts again. Nor
// does a seam with a stretch on a piece cut once.
void TST_GarmentSymmetry::seamsAroundTheBodyHaveNoTwins() const
{
    GarmentSymmetry symmetry;
    symmetry.setPiece(20, PieceSymmetry::Pair);
    symmetry.setPiece(40, PieceSymmetry::Pair);

    GarmentSeam around = seam(20, 3, 4, 40, 1, 2, false);
    around.first_more = {{PieceOutline::mirrorId(20), 3, 4, true}};
    QCOMPARE(symmetry.madeUp({around}).size(), 1);

    GarmentSeam on_single = seam(20, 3, 4, 50, 1, 2, false);
    on_single.first_more = {{40, 5, 6, false}};
    QCOMPARE(symmetry.madeUp({on_single}).size(), 1);
}
