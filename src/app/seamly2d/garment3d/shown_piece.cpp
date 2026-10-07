//---------------------------------------------------------------------------------------------------------------------
//  @file   shown_piece.cpp
//  @author Julius
//  @date   7 Oct, 2026
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

#include "shown_piece.h"

#include <QLineF>

#include <algorithm>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

//---------------------------------------------------------------------------------------------------------------------
// The point mirrored across the line through a and b.
QPointF reflect(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF along = b - a;
    const QPointF offset = point - a;
    const QPointF onto = along * (QPointF::dotProduct(offset, along) / QPointF::dotProduct(along, along));
    return a + onto * 2.0 - offset;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mesh the scene shows by this id, or nullptr.
const ShownMesh* ShownPiece::mesh(quint32 id) const
{
    const auto found = std::find_if(shown.cbegin(), shown.cend(), [id](const ShownMesh& candidate)
    {
        return candidate.id == id;
    });
    return found != shown.cend() ? &*found : nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the piece is on the avatar, rather than on the board.
bool ShownPiece::isPlaced() const
{
    return std::any_of(shown.cbegin(), shown.cend(), [](const ShownMesh& candidate)
    {
        return candidate.placed;
    });
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The segments along the fold line of a piece cut on the fold, from the fold's start to its end; the garment
/// has no edge there.
QVector<bool> ShownPiece::foldSegments() const
{
    const int count = outline.segmentCount();
    QVector<bool> fold(count, false);
    const int start = fold_start != 0 ? nodePosition(fold_start) : -1;
    const int end = fold_end != 0 ? nodePosition(fold_end) : -1;
    if (start >= 0 && end >= 0 && start != end)
    {
        for (int segment = start; segment != end; segment = (segment + 1) % count)
        {
            fold[segment] = true;
        }
    }
    return fold;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a point of a shown mesh's flat shape is on the piece as drafted: on an unfolded piece's mirrored half
/// it is mirrored back across the fold line, on a mirrored copy mirrored back.
QPointF ShownPiece::drafted(PieceLayout layout, const QPointF& point) const
{
    QPointF on_piece = point;
    QPointF start;
    QPointF end;
    if (layout == PieceLayout::Mirrored)
    {
        on_piece.setX(-point.x());
    }
    else if (layout == PieceLayout::Unfolded && foldLine(&start, &end))
    {
        // The drafted half lies on the side of the fold line its points are furthest to.
        qreal drafted_side = 0;
        for (const QPointF& outline_point : outline.points())
        {
            const qreal side = cross(end - start, outline_point - start);
            drafted_side = qAbs(side) > qAbs(drafted_side) ? side : drafted_side;
        }
        if (cross(end - start, point - start) * drafted_side < 0)
        {
            on_piece = reflect(point, start, end);
        }
    }
    return on_piece;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Lines drawn on the piece as drafted, laid out as a shown mesh lies over it: on an unfolded piece also
/// mirrored across the fold line, on a mirrored copy mirrored.
QVector<QVector<QPointF>> ShownPiece::laidOut(const QVector<QVector<QPointF>>& lines, PieceLayout layout) const
{
    QVector<QVector<QPointF>> laid = lines;
    QPointF start;
    QPointF end;
    if (layout == PieceLayout::Mirrored)
    {
        for (QVector<QPointF>& line : laid)
        {
            for (QPointF& point : line)
            {
                point.setX(-point.x());
            }
        }
    }
    else if (layout == PieceLayout::Unfolded && foldLine(&start, &end))
    {
        for (QVector<QPointF> line : lines)
        {
            for (QPointF& point : line)
            {
                point = reflect(point, start, end);
            }
            laid.append(line);
        }
    }
    return laid;
}

//---------------------------------------------------------------------------------------------------------------------
// The ends of the fold line of a piece cut on the fold.
bool ShownPiece::foldLine(QPointF* start, QPointF* end) const
{
    const int first = fold_start != 0 ? nodePosition(fold_start) : -1;
    const int last = fold_end != 0 ? nodePosition(fold_end) : -1;
    if (first < 0 || last < 0)
    {
        return false;
    }
    *start = outline.points().at(outline.nodes().at(first).index);
    *end = outline.points().at(outline.nodes().at(last).index);
    return QLineF(*start, *end).length() > 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Which of the outline's path points the node is, or -1.
int ShownPiece::nodePosition(quint32 id) const
{
    const QVector<OutlineNode>& nodes = outline.nodes();
    for (int k = 0; k < nodes.size(); ++k)
    {
        if (nodes.at(k).id == id)
        {
            return k;
        }
    }
    return -1;
}
