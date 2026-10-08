//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_pieceoutline.cpp
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

#include "tst_pieceoutline.h"

#include <QLineF>
#include <QScopedPointer>
#include <QtTest>

#include <algorithm>

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
// Path node ids of the test piece.
const quint32 top_left = 1;
const quint32 top_right = 2;
const quint32 side_notch = 3;
const quint32 bottom_right = 4;
const quint32 bottom_left = 5;

//---------------------------------------------------------------------------------------------------------------------
// A 10 x 10 cm square placed at (5, 3) cm, with a notch 3 cm down its right side. The notch lies on a straight line,
// which the piece's seam line tidies away, so the outline has to put it back. With paths, it has an internal path
// 2 cm in from its left side, from top to bottom, and a slit cut into it.
PieceOutline squareOutline(bool with_paths = false)
{
    const Unit unit = Unit::Cm;
    QScopedPointer<VContainer> data(new VContainer(nullptr, &unit));
    qApp->setPatternUnit(unit);

    auto add_point = [&data](quint32 id, qreal x, qreal y)
    {
        data->UpdateGObject(id, new VPointF(ToPixel(x, Unit::Cm), ToPixel(y, Unit::Cm), QStringLiteral("A%1").arg(id),
                                            0, 0));
    };
    add_point(top_left, 0, 0);
    add_point(top_right, 10, 0);
    add_point(side_notch, 10, 3);
    add_point(bottom_right, 10, 10);
    add_point(bottom_left, 0, 10);

    VPiece piece;
    piece.SetSeamAllowance(false);
    for (const quint32 id : {top_left, top_right, side_notch, bottom_right, bottom_left})
    {
        VPieceNode node(id, Tool::NodePoint);
        node.setNotch(id == side_notch);
        piece.GetPath().Append(node);
    }
    piece.SetMx(ToPixel(5, Unit::Cm));
    piece.SetMy(ToPixel(3, Unit::Cm));

    if (with_paths)
    {
        add_point(6, 2, 0);
        add_point(7, 2, 10);
        add_point(8, 5, 4);
        add_point(9, 7, 6);
        VPiecePath line(PiecePathType::InternalPath);
        line.Append(VPieceNode(6, Tool::NodePoint));
        line.Append(VPieceNode(7, Tool::NodePoint));
        data->UpdatePiecePath(20, line);
        VPiecePath slit(PiecePathType::InternalPath);
        slit.setCutPath(true);
        slit.Append(VPieceNode(8, Tool::NodePoint));
        slit.Append(VPieceNode(9, Tool::NodePoint));
        data->UpdatePiecePath(21, slit);
        piece.SetInternalPaths({20, 21});
    }

    return PieceOutline::fromPiece(piece, data.data());
}

//---------------------------------------------------------------------------------------------------------------------
bool samePoint(const QPointF& a, const QPointF& b)
{
    return QLineF(a, b).length() < 1e-6;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_PieceOutline::TST_PieceOutline(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// An internal path is a line inside the piece, where the piece is, in cm; a slit cut into the piece isn't.
void TST_PieceOutline::internalPathsAreLines() const
{
    QVERIFY(squareOutline().lines().isEmpty());

    const PieceOutline outline = squareOutline(true);
    QCOMPARE(outline.lines().size(), 1);
    const OutlineLine& line = outline.lines().first();
    QCOMPARE(line.id, 20u);
    QCOMPARE(line.points.size(), 2);
    QVERIFY(samePoint(line.points.first(), QPointF(7, 3)));
    QVERIFY(samePoint(line.points.last(), QPointF(7, 13)));
    QVERIFY(squareOutline(true) != squareOutline());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::pathPointsAreOnTheOutline() const
{
    const PieceOutline outline = squareOutline();

    const QVector<quint32> ids = {top_left, top_right, side_notch, bottom_right, bottom_left};
    const QVector<QPointF> places = {QPointF(5, 3), QPointF(15, 3), QPointF(15, 6), QPointF(15, 13), QPointF(5, 13)};
    QCOMPARE(outline.nodes().size(), ids.size());
    QCOMPARE(outline.segmentCount(), ids.size());
    for (int k = 0; k < ids.size(); ++k)
    {
        const OutlineNode& node = outline.nodes().at(k);
        QCOMPARE(node.id, ids.at(k));
        QVERIFY2(samePoint(outline.points().at(node.index), places.at(k)), qUtf8Printable(QString::number(node.id)));
        QCOMPARE(node.notch, node.id == side_notch);
    }
    QCOMPARE(outline.segmentStart(4), bottom_left);
    QCOMPARE(outline.segmentEnd(4), top_left);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::stretchFollowsThePath() const
{
    const PieceOutline outline = squareOutline();

    const SeamStretch side = outline.stretch(top_right, bottom_right);
    QCOMPARE(side.length(), 10.0);
    QCOMPARE(side.notches(), QVector<qreal>({3.0}));
    QVERIFY(samePoint(side.points().first(), QPointF(15, 3)));
    QVERIFY(samePoint(side.points().last(), QPointF(15, 13)));

    // Going on past the last point to the first.
    const SeamStretch closing = outline.stretch(bottom_left, top_left);
    QCOMPARE(closing.length(), 10.0);
    QVERIFY(closing.notches().isEmpty());
    QVERIFY(samePoint(closing.points().first(), QPointF(5, 13)));
    QVERIFY(samePoint(closing.points().last(), QPointF(5, 3)));

    QVERIFY(outline.stretch(top_left, 99).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::stretchGoesAroundOnce() const
{
    const SeamStretch loop = squareOutline().stretch(top_left, top_left);

    QCOMPARE(loop.length(), 40.0);
    QCOMPARE(loop.notches(), QVector<qreal>({13.0}));
    QVERIFY(samePoint(loop.points().last(), loop.points().first()));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::reversedStretchKeepsItsNotches() const
{
    const SeamStretch reversed = squareOutline().stretch(top_right, bottom_right).reversed();

    QCOMPARE(reversed.length(), 10.0);
    QCOMPARE(reversed.notches(), QVector<qreal>({7.0}));
    QVERIFY(samePoint(reversed.points().first(), QPointF(15, 13)));
    QVERIFY(samePoint(reversed.pointAt(7.0), QPointF(15, 6)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::hitFindsTheNearestSegment() const
{
    const OutlineHit hit = squareOutline().hit(QPointF(15.2, 9));

    QCOMPARE(hit.segment, 2);
    QVERIFY(qAbs(hit.distance - 0.2) < 1e-9);
    QVERIFY(qAbs(hit.along - 3.0 / 7.0) < 1e-9);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::hitWrapsAroundTheFirstPoint() const
{
    const OutlineHit hit = squareOutline().hit(QPointF(4.9, 5));

    QCOMPARE(hit.segment, 4);
    QVERIFY(qAbs(hit.distance - 0.1) < 1e-9);
    QVERIFY(qAbs(hit.along - 0.8) < 1e-9);

    QCOMPARE(PieceOutline().hit(QPointF(0, 0)).segment, -1);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::pointAtWalksTheStretch() const
{
    const SeamStretch loop = squareOutline().stretch(top_left, top_left);

    QVERIFY(samePoint(loop.pointAt(12), QPointF(15, 5)));
    QVERIFY(samePoint(loop.pointAt(-1), QPointF(5, 3)));
    QVERIFY(samePoint(loop.pointAt(100), QPointF(5, 3)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PieceOutline::matchingLinesUpNotches() const
{
    const SeamStretch first({QPointF(0, 0), QPointF(10, 0)}, {4});
    const SeamStretch second({QPointF(0, 0), QPointF(0, 20)}, {12});

    const QVector<SeamMatch> matches = SeamStretch::matches(first, second);
    QCOMPARE(matches.size(), 3);
    QCOMPARE(matches.at(0).first, 0.0);
    QCOMPARE(matches.at(0).second, 0.0);
    QCOMPARE(matches.at(1).first, 4.0);
    QCOMPARE(matches.at(1).second, 12.0);
    QCOMPARE(matches.at(2).first, 10.0);
    QCOMPARE(matches.at(2).second, 20.0);
}

//---------------------------------------------------------------------------------------------------------------------
// With a notch more on one side there is no telling which notches belong together.
void TST_PieceOutline::unevenNotchesMatchOnlyTheEnds() const
{
    const SeamStretch first({QPointF(0, 0), QPointF(10, 0)}, {4});
    const SeamStretch second({QPointF(0, 0), QPointF(0, 20)}, {5, 12});

    const QVector<SeamMatch> matches = SeamStretch::matches(first, second);
    QCOMPARE(matches.size(), 2);
    QCOMPARE(matches.at(1).first, 10.0);
    QCOMPARE(matches.at(1).second, 20.0);

    QVERIFY(SeamStretch::matches(first, SeamStretch()).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// A 10 cm side with vertices 0 to 2 sewn to a 12 cm side with vertices 10 to 13: each vertex is sewn onto the point
// of the other side it meets, and where vertex meets vertex there is only one stitch.
void TST_PieceOutline::stitchesSewBothSides() const
{
    const SeamStretch first({QPointF(0, 0), QPointF(5, 0), QPointF(10, 0)}, {}, {0, 1, 2});
    const SeamStretch second({QPointF(0, 10), QPointF(4, 10), QPointF(8, 10), QPointF(12, 10)}, {}, {10, 11, 12, 13});

    const QVector<Stitch> stitches = SeamStretch::stitches(first, second);
    QCOMPARE(stitches.size(), 5);

    auto has_stitch = [&stitches](quint32 vertex, quint32 edge_start, quint32 edge_end, qreal along)
    {
        return std::any_of(stitches.cbegin(), stitches.cend(), [=](const Stitch& stitch)
        {
            return stitch.vertex == vertex && stitch.edge_start == edge_start && stitch.edge_end == edge_end
                   && qAbs(stitch.along - along) < 1e-9;
        });
    };
    QVERIFY(has_stitch(0, 10, 10, 0));
    QVERIFY(has_stitch(1, 11, 12, 0.5));
    QVERIFY(has_stitch(2, 13, 13, 0));
    QVERIFY(has_stitch(11, 0, 1, 4.0 / 6.0));
    QVERIFY(has_stitch(12, 1, 2, 2.0 / 6.0));

    QVERIFY(SeamStretch::stitches(first, SeamStretch({QPointF(0, 0), QPointF(1, 0)})).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// Between lined up notches the sides are eased onto each other piece by piece.
void TST_PieceOutline::stitchesFollowNotches() const
{
    const SeamStretch first({QPointF(0, 0), QPointF(5, 0), QPointF(10, 0)}, {2}, {0, 1, 2});
    const SeamStretch second({QPointF(0, 10), QPointF(4, 10), QPointF(8, 10), QPointF(12, 10)}, {9}, {10, 11, 12, 13});

    const QVector<Stitch> stitches = SeamStretch::stitches(first, second);

    // Vertex 1 is 5 cm along: 3 of the 8 cm from the notch to the end, so 3/8 of the 3 cm past the other notch.
    const auto middle = std::find_if(stitches.cbegin(), stitches.cend(), [](const Stitch& stitch)
    {
        return stitch.vertex == 1;
    });
    QVERIFY(middle != stitches.cend());
    QCOMPARE(middle->edge_start, 12u);
    QCOMPARE(middle->edge_end, 13u);
    QVERIFY(qAbs(middle->along - (9.0 + 3.0 * 3.0 / 8.0 - 8.0) / 4.0) < 1e-9);
}
