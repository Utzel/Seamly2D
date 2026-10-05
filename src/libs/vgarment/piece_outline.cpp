//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_outline.cpp
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

#include "piece_outline.h"

#include <QHash>
#include <QLineF>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <limits>

#include "../vgeometry/vpointf.h"
#include "../vmisc/def.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vpatterndb/vpiecepath.h"

namespace
{
// Path points closer than this to a seam line point, in pixels, are that point.
const qreal same_point_tolerance = 0.01;

// Projections this close to a segment's end, as a share of its length, are taken as that end.
const qreal end_tolerance = 1e-6;

// Ids of mirror images, of path points on a piece's unfolded half or of a piece's mirrored copy, have this bit set.
const quint32 mirror_bit = 0x80000000u;

// A fold line runs straight along a side of the piece; its points may be this far off that side, in cm.
const qreal fold_tolerance = 0.05;

// Straight stretches shorter than this, in cm, aren't taken for a fold line.
const qreal shortest_fold = 5.0;

//---------------------------------------------------------------------------------------------------------------------
// The point mirrored across the line through a and b.
QPointF reflect(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF along = b - a;
    const qreal length_squared = QPointF::dotProduct(along, along);
    const QPointF offset = point - a;
    const QPointF onto = along * (QPointF::dotProduct(offset, along) / length_squared);
    return a + onto * 2.0 - offset;
}

//---------------------------------------------------------------------------------------------------------------------
// Distance from the point to the segment, and how far along the segment its nearest point is, from 0 to 1.
qreal distanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b, qreal* along)
{
    const QPointF ab = b - a;
    const qreal length_squared = QPointF::dotProduct(ab, ab);
    qreal t = 0;
    if (length_squared > 0)
    {
        t = qBound(0.0, QPointF::dotProduct(point - a, ab) / length_squared, 1.0);
    }
    *along = t;
    return QLineF(point, a + ab * t).length();
}

//---------------------------------------------------------------------------------------------------------------------
// Puts the point onto the closed seam line where it is nearest, as a new point unless it falls on one, and returns
// that point's index.
int insertProjection(QVector<QPointF>& seam_line, const QPointF& point)
{
    const int count = static_cast<int>(seam_line.size());
    int nearest = 0;
    qreal nearest_along = 0;
    qreal nearest_distance = std::numeric_limits<qreal>::infinity();
    for (int i = 0; i < count; ++i)
    {
        qreal along = 0;
        const qreal distance = distanceToSegment(point, seam_line.at(i), seam_line.at((i + 1) % count), &along);
        if (distance < nearest_distance)
        {
            nearest = i;
            nearest_along = along;
            nearest_distance = distance;
        }
    }

    int index = nearest;
    if (nearest_along >= 1.0 - end_tolerance)
    {
        index = (nearest + 1) % count;
    }
    else if (nearest_along > end_tolerance)
    {
        const QPointF& a = seam_line.at(nearest);
        const QPointF& b = seam_line.at((nearest + 1) % count);
        index = nearest + 1;
        seam_line.insert(index, a + (b - a) * nearest_along);
    }
    return index;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool OutlineNode::operator==(const OutlineNode& other) const
{
    return id == other.id && index == other.index && notch == other.notch;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The nodes have to come in the order of the points they lie on.
PieceOutline::PieceOutline(const QVector<QPointF>& points, const QVector<OutlineNode>& nodes)
    : m_points(points)
    , m_nodes(nodes)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The piece's seam line in cm, at the piece's position in the piece scene, with its path points on it.
///
/// Path points the seam line doesn't run through exactly, because it was tidied up, are put where it passes
/// closest. Throws VException if a point of the piece can't be found.
PieceOutline PieceOutline::fromPiece(const VPiece& piece, const VContainer* data)
{
    QVector<QPointF> points = piece.mainPathPoints(data);
    QVector<OutlineNode> nodes;

    if (points.size() >= 2)
    {
        const VPiecePath& path = piece.GetPath();
        int search_from = 0;
        for (int i = 0; i < path.nodeCount(); ++i)
        {
            const VPieceNode& node = path.at(i);
            if (node.GetTypeTool() == Tool::NodePoint && !node.isExcluded())
            {
                const QPointF position = static_cast<QPointF>(*data->GeometricObject<VPointF>(node.GetId()));

                int index = -1;
                for (int k = search_from; k < points.size() && index < 0; ++k)
                {
                    if (QLineF(points.at(k), position).length() <= same_point_tolerance)
                    {
                        index = k;
                    }
                }

                if (index < 0)
                {
                    const int count = static_cast<int>(points.size());
                    index = insertProjection(points, position);
                    if (points.size() > count)
                    {
                        for (OutlineNode& earlier : nodes)
                        {
                            earlier.index += earlier.index >= index ? 1 : 0;
                        }
                    }
                }

                OutlineNode outline_node;
                outline_node.id = node.GetId();
                outline_node.index = index;
                outline_node.notch = node.isNotch();
                nodes.append(outline_node);
                search_from = index;
            }
        }

        std::stable_sort(nodes.begin(), nodes.end(), [](const OutlineNode& a, const OutlineNode& b)
        {
            return a.index < b.index;
        });
    }

    for (QPointF& point : points)
    {
        point = QPointF(FromPixel(point.x() + piece.GetMx(), Unit::Cm), FromPixel(point.y() + piece.GetMy(), Unit::Cm));
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seam line's points in cm. The line closes from the last point back to the first.
const QVector<QPointF>& PieceOutline::points() const
{
    return m_points;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The path points on the seam line, in the order the seam line passes them.
const QVector<OutlineNode>& PieceOutline::nodes() const
{
    return m_nodes;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceOutline::isEmpty() const
{
    return m_points.size() < 3;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceOutline::hasNode(quint32 id) const
{
    return nodePosition(id) >= 0;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief As many as there are path points: each segment runs from one to the next, the last one back to the first.
int PieceOutline::segmentCount() const
{
    return isEmpty() ? 0 : static_cast<int>(m_nodes.size());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The path node a segment starts at.
quint32 PieceOutline::segmentStart(int segment) const
{
    return m_nodes.at(segment).id;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The path node a segment ends at; with a single path point, the one it starts at.
quint32 PieceOutline::segmentEnd(int segment) const
{
    return m_nodes.at((segment + 1) % m_nodes.size()).id;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Finds the segment closest to the point, given in cm.
OutlineHit PieceOutline::hit(const QPointF& point) const
{
    OutlineHit result;
    const int count = static_cast<int>(m_points.size());
    if (segmentCount() > 0)
    {
        int nearest_edge = 0;
        qreal nearest_along = 0;
        result.distance = std::numeric_limits<qreal>::infinity();
        for (int i = 0; i < count; ++i)
        {
            qreal along = 0;
            const qreal distance = distanceToSegment(point, m_points.at(i), m_points.at((i + 1) % count), &along);
            if (distance < result.distance)
            {
                nearest_edge = i;
                nearest_along = along;
                result.distance = distance;
            }
        }

        // The segment is the one of the last path point before the edge, or the one going round from the last path
        // point to the first.
        result.segment = static_cast<int>(m_nodes.size()) - 1;
        for (int k = 0; k < m_nodes.size(); ++k)
        {
            if (m_nodes.at(k).index <= nearest_edge)
            {
                result.segment = k;
            }
        }

        qreal travelled = 0;
        for (int i = m_nodes.at(result.segment).index; i != nearest_edge; i = (i + 1) % count)
        {
            travelled += QLineF(m_points.at(i), m_points.at((i + 1) % count)).length();
        }
        const QLineF nearest_line(m_points.at(nearest_edge), m_points.at((nearest_edge + 1) % count));
        travelled += nearest_along * nearest_line.length();

        const qreal segment_length = stretch(segmentStart(result.segment), segmentEnd(result.segment)).length();
        result.along = segment_length > 0 ? qBound(0.0, travelled / segment_length, 1.0) : 0.0;
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seam line from one path point forward to another, with the notches in between. From a point to itself
/// is once around. Unknown points give an empty stretch. The stretch's vertices are the indices of its points in
/// the outline.
SeamStretch PieceOutline::stretch(quint32 start_node, quint32 end_node) const
{
    const int start = nodePosition(start_node);
    const int end = nodePosition(end_node);
    if (start < 0 || end < 0 || isEmpty())
    {
        return SeamStretch();
    }

    const int count = static_cast<int>(m_points.size());
    const int first = m_nodes.at(start).index;
    const int steps = start == end ? count : (m_nodes.at(end).index - first + count) % count;

    QVector<QPointF> points{m_points.at(first)};
    QVector<quint32> indices{static_cast<quint32>(first)};
    QHash<int, qreal> distance_at{{first, 0.0}};
    qreal travelled = 0;
    for (int step = 0, i = first; step < steps; ++step, i = (i + 1) % count)
    {
        const int next = (i + 1) % count;
        travelled += QLineF(m_points.at(i), m_points.at(next)).length();
        points.append(m_points.at(next));
        indices.append(static_cast<quint32>(next));
        if (!distance_at.contains(next))
        {
            distance_at.insert(next, travelled);
        }
    }

    QVector<qreal> notches;
    const int node_count = static_cast<int>(m_nodes.size());
    for (int k = (start + 1) % node_count; k != end && k != start; k = (k + 1) % node_count)
    {
        if (m_nodes.at(k).notch)
        {
            notches.append(distance_at.value(m_nodes.at(k).index));
        }
    }
    return SeamStretch(points, notches, indices);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Finds where a piece cut on the fold is folded: the longest straight run of segments along a side of the
/// piece's bounding box, where centre front and centre back lie. False if there is none.
bool PieceOutline::findFoldLine(quint32* start_node, quint32* end_node) const
{
    const int segments = segmentCount();
    if (segments < 2)
    {
        return false;
    }

    const QRectF bounds = QPolygonF(m_points).boundingRect();
    auto on_side = [&bounds](const QPointF& point, int side)
    {
        switch (side)
        {
            case 0:
                return qAbs(point.x() - bounds.left()) <= fold_tolerance;
            case 1:
                return qAbs(point.x() - bounds.right()) <= fold_tolerance;
            case 2:
                return qAbs(point.y() - bounds.top()) <= fold_tolerance;
            default:
                return qAbs(point.y() - bounds.bottom()) <= fold_tolerance;
        }
    };

    const int count = static_cast<int>(m_points.size());
    qreal longest = shortest_fold;
    bool found = false;
    for (int side = 0; side < 4; ++side)
    {
        // Which segments lie along this side, all their points.
        QVector<bool> along(segments, true);
        for (int k = 0; k < segments; ++k)
        {
            const int last = m_nodes.at((k + 1) % segments).index;
            for (int i = m_nodes.at(k).index; along.at(k); i = (i + 1) % count)
            {
                along[k] = on_side(m_points.at(i), side);
                if (i == last)
                {
                    break;
                }
            }
        }

        // The longest run of such segments, going round past the last one to the first.
        for (int first = 0; first < segments; ++first)
        {
            const bool starts_run = along.at(first) && !along.at((first + segments - 1) % segments);
            if (starts_run)
            {
                int last = first;
                while (along.at((last + 1) % segments) && (last + 1) % segments != first)
                {
                    last = (last + 1) % segments;
                }
                const OutlineNode& from = m_nodes.at(first);
                const OutlineNode& to = m_nodes.at((last + 1) % segments);
                const qreal length = QLineF(m_points.at(from.index), m_points.at(to.index)).length();
                if (length > longest)
                {
                    longest = length;
                    *start_node = from.id;
                    *end_node = to.id;
                    found = true;
                }
            }
        }
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The whole piece of a piece cut on the fold: the half unfolded across the straight fold line from one path
/// point to another, which is left out.
///
/// The seam line runs from the fold's end round the half to the fold's start, as before, and on round the mirrored
/// half back to the fold's end. Path points on the mirrored half have mirrored ids; the fold's ends stay as they are.
PieceOutline PieceOutline::unfolded(quint32 start_node, quint32 end_node) const
{
    const int start = nodePosition(start_node);
    const int end = nodePosition(end_node);
    if (start < 0 || end < 0 || start == end || isEmpty())
    {
        return *this;
    }

    const int count = static_cast<int>(m_points.size());
    const QPointF fold_start = m_points.at(m_nodes.at(start).index);
    const QPointF fold_end = m_points.at(m_nodes.at(end).index);
    if (QLineF(fold_start, fold_end).length() <= same_point_tolerance)
    {
        return *this;
    }

    // The half, from the fold's end forward to its start.
    const int from = m_nodes.at(end).index;
    const int steps = (m_nodes.at(start).index - from + count) % count;
    QVector<QPointF> points;
    for (int step = 0; step <= steps; ++step)
    {
        points.append(m_points.at((from + step) % count));
    }

    QVector<OutlineNode> nodes;
    const int node_count = static_cast<int>(m_nodes.size());
    for (int k = end;; k = (k + 1) % node_count)
    {
        OutlineNode node = m_nodes.at(k);
        node.index = (node.index - from + count) % count;
        nodes.append(node);
        if (k == start)
        {
            break;
        }
    }

    // The mirrored half, from the fold's start back to its end, leaving out the fold's ends, which it shares.
    const int half_count = static_cast<int>(points.size());
    for (int p = half_count - 2; p >= 1; --p)
    {
        points.append(reflect(points.at(p), fold_start, fold_end));
    }
    for (int k = static_cast<int>(nodes.size()) - 1; k >= 0; --k)
    {
        const OutlineNode& node = nodes.at(k);
        if (node.index > 0 && node.index < half_count - 1)
        {
            OutlineNode mirrored = node;
            mirrored.id = mirrorId(node.id);
            mirrored.index = 2 * half_count - 2 - node.index;
            nodes.append(mirrored);
        }
    }
    return PieceOutline(points, nodes);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The id of a path point's mirror image on an unfolded piece, or of a piece's mirrored copy. Mirroring an id
/// twice gives it back.
quint32 PieceOutline::mirrorId(quint32 id)
{
    return id ^ mirror_bit;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceOutline::isMirrorId(quint32 id)
{
    return (id & mirror_bit) != 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceOutline::operator==(const PieceOutline& other) const
{
    return m_points == other.m_points && m_nodes == other.m_nodes;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceOutline::operator!=(const PieceOutline& other) const
{
    return !(*this == other);
}

//---------------------------------------------------------------------------------------------------------------------
int PieceOutline::nodePosition(quint32 id) const
{
    int position = -1;
    for (int k = 0; k < m_nodes.size() && position < 0; ++k)
    {
        if (m_nodes.at(k).id == id)
        {
            position = k;
        }
    }
    return position;
}
