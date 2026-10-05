//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_outline.h
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

#ifndef PIECE_OUTLINE_H
#define PIECE_OUTLINE_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include "seam_stretch.h"

class VContainer;
class VPiece;

/// @brief A point of a piece's path that lies on the piece's seam line.
struct OutlineNode
{
    quint32 id = 0;       ///< the path node's id
    int     index = 0;    ///< which point of the outline it is
    bool    notch = false;

    bool    operator==(const OutlineNode& other) const;
};

/// @brief Where a point is closest to an outline: on which segment and how far along it.
struct OutlineHit
{
    int   segment = -1;  ///< the segment starting at nodes()[segment], or -1 for none
    qreal distance = 0;  ///< from the point to the seam line, in cm
    qreal along = 0;     ///< how far along the segment, from 0 at its start to 1 at its end
};

/// @brief A piece's seam line in cm, at the piece's position in the piece scene, with the points of the piece's path
/// that it runs through.
///
/// Between two path points that follow each other lies a segment, the stretch of seam line that is sewn to another
/// piece's segment. A seam can also run over several segments.
class PieceOutline
{
public:
                                PieceOutline() = default;
                                PieceOutline(const QVector<QPointF>& points, const QVector<OutlineNode>& nodes);

    static PieceOutline         fromPiece(const VPiece& piece, const VContainer* data);

    const QVector<QPointF>&     points() const;
    const QVector<OutlineNode>& nodes() const;
    bool                        isEmpty() const;
    bool                        hasNode(quint32 id) const;

    int                         segmentCount() const;
    quint32                     segmentStart(int segment) const;
    quint32                     segmentEnd(int segment) const;
    OutlineHit                  hit(const QPointF& point) const;

    SeamStretch                 stretch(quint32 start_node, quint32 end_node) const;

    bool                        operator==(const PieceOutline& other) const;
    bool                        operator!=(const PieceOutline& other) const;

private:
    QVector<QPointF>            m_points;
    QVector<OutlineNode>        m_nodes;

    int                         nodePosition(quint32 id) const;
};

#endif // PIECE_OUTLINE_H
