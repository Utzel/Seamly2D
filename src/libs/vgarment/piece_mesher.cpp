//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_mesher.cpp
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

#include "piece_mesher.h"

#include <QLineF>
#include <QPolygonF>
#include <QSet>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>

#include "../vobj/delaunay.h"

namespace
{
// Outline vertices that turn sharper than this, in degrees, stay where they are when the outline is resampled.
const qreal corner_angle = 25.0;

// Lattice points closer to the seam line than this share of the edge length are dropped, they only make slivers.
const qreal seam_line_clearance = 0.5;

// Smallest edge length accepted, in cm, so a typo can't ask for millions of triangles.
const qreal min_edge_length = 0.1;

// Seam line segments the triangulation leaves out are split at most this many rounds.
const int max_split_rounds = 12;

// Points closer than this, in cm, count as the same point.
const qreal same_point_tolerance = 1e-6;

// A closed seam line, and which of its points each path point of the piece is, or -1 for a path point that was lost.
struct SeamLine
{
    QVector<QPointF> points;
    QVector<int>     node_points;
};

//---------------------------------------------------------------------------------------------------------------------
qreal signedArea(const QVector<QPointF>& polygon)
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
qreal doubledTriangleArea(const QPointF& a, const QPointF& b, const QPointF& c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
}

//---------------------------------------------------------------------------------------------------------------------
// Drops repeated points and the closing point; path points on a dropped point move to the point it repeats.
// Returns a seam line without points if nothing with an area is left.
SeamLine cleanOutline(const PieceOutline& outline)
{
    const QVector<QPointF>& points = outline.points();

    SeamLine cleaned;
    QVector<int> new_index(points.size(), 0);
    for (int i = 0; i < points.size(); ++i)
    {
        if (cleaned.points.isEmpty() || QLineF(cleaned.points.last(), points.at(i)).length() > same_point_tolerance)
        {
            cleaned.points.append(points.at(i));
        }
        new_index[i] = static_cast<int>(cleaned.points.size()) - 1;
    }
    while (cleaned.points.size() > 1 && QLineF(cleaned.points.last(), cleaned.points.first()).length()
                                        <= same_point_tolerance)
    {
        const int removed = static_cast<int>(cleaned.points.size()) - 1;
        cleaned.points.removeLast();
        std::replace(new_index.begin(), new_index.end(), removed, 0);
    }

    const qreal area = cleaned.points.size() >= 3 ? signedArea(cleaned.points) : 0;
    if (qAbs(area) <= same_point_tolerance)
    {
        cleaned.points.clear();
    }
    else
    {
        for (const OutlineNode& node : outline.nodes())
        {
            cleaned.node_points.append(node.index >= 0 && node.index < new_index.size() ? new_index.at(node.index)
                                                                                        : -1);
        }
    }
    return cleaned;
}

//---------------------------------------------------------------------------------------------------------------------
// Appends the run's first point and the points that cut the run into equal parts of about the edge length.
// The run's last point is the next run's first point, so it is left out.
void appendResampledRun(const QVector<QPointF>& run, qreal edge_length, QVector<QPointF>& resampled)
{
    QVector<qreal> distance_along(run.size(), 0.0);
    for (int i = 1; i < run.size(); ++i)
    {
        distance_along[i] = distance_along.at(i - 1) + QLineF(run.at(i - 1), run.at(i)).length();
    }

    const qreal run_length = distance_along.last();
    const int parts = qMax(1, qRound(run_length / edge_length));
    const qreal step = run_length / parts;

    resampled.append(run.first());
    int segment = 1;
    for (int part = 1; part < parts; ++part)
    {
        const qreal target = part * step;
        while (segment < run.size() - 1 && distance_along.at(segment) < target)
        {
            ++segment;
        }
        const qreal segment_length = distance_along.at(segment) - distance_along.at(segment - 1);
        const qreal t = segment_length > 0 ? (target - distance_along.at(segment - 1)) / segment_length : 0;
        resampled.append(run.at(segment - 1) + (run.at(segment) - run.at(segment - 1)) * t);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Resamples the seam line so its segments are about the edge length long. Corners and path points are kept, the
// curves between them are spaced out evenly.
SeamLine resampleOutline(const SeamLine& seam_line, qreal edge_length)
{
    const QVector<QPointF>& outline = seam_line.points;
    const int count = static_cast<int>(outline.size());

    QVector<int> kept;
    for (int i = 0; i < count; ++i)
    {
        const QPointF& previous = outline.at((i + count - 1) % count);
        const QPointF& next = outline.at((i + 1) % count);
        qreal turn = QLineF(previous, outline.at(i)).angleTo(QLineF(outline.at(i), next));
        if (turn > 180.0)
        {
            turn -= 360.0;
        }
        if (qAbs(turn) > corner_angle || seam_line.node_points.contains(i))
        {
            kept.append(i);
        }
    }
    if (kept.isEmpty())
    {
        kept.append(0);
    }

    SeamLine resampled;
    QVector<int> kept_at(count, -1);
    for (int k = 0; k < kept.size(); ++k)
    {
        const int end = kept.at((k + 1) % kept.size());
        int i = kept.at(k);
        kept_at[i] = static_cast<int>(resampled.points.size());

        QVector<QPointF> run{outline.at(i)};
        do
        {
            i = (i + 1) % count;
            run.append(outline.at(i));
        } while (i != end);
        appendResampledRun(run, edge_length, resampled.points);
    }

    for (const int point : seam_line.node_points)
    {
        resampled.node_points.append(point >= 0 ? kept_at.at(point) : -1);
    }
    return resampled;
}

//---------------------------------------------------------------------------------------------------------------------
qreal distanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal length_squared = QPointF::dotProduct(ab, ab);
    qreal t = 0;
    if (length_squared > 0)
    {
        t = qBound(0.0, QPointF::dotProduct(point - a, ab) / length_squared, 1.0);
    }
    return QLineF(point, a + ab * t).length();
}

//---------------------------------------------------------------------------------------------------------------------
// A lattice point has to keep its distance from the seam line, and it has to stay outside every segment's
// diametral circle: a point inside that circle keeps the Delaunay triangulation from using the segment.
bool keepsClearOfSeamLine(const QVector<QPointF>& seam_line, const QPointF& point, qreal clearance)
{
    bool clear = true;
    for (int i = 0; i < seam_line.size() && clear; ++i)
    {
        const QPointF& a = seam_line.at(i);
        const QPointF& b = seam_line.at((i + 1) % seam_line.size());
        const qreal radius = QLineF(a, b).length() / 2.0;
        clear = distanceToSegment(point, a, b) >= clearance && QLineF(point, (a + b) / 2.0).length() > radius * 1.01;
    }
    return clear;
}

//---------------------------------------------------------------------------------------------------------------------
// Points of a triangular lattice with the edge length as spacing that lie inside the seam line, centred on the
// piece so symmetric pieces get symmetric meshes.
QVector<QPointF> latticePoints(const QVector<QPointF>& seam_line, qreal edge_length)
{
    const QPolygonF polygon(seam_line);
    const QRectF bounds = polygon.boundingRect();
    const QPointF center = bounds.center();
    const qreal row_height = edge_length * qSqrt(3.0) / 2.0;
    const int half_rows = qCeil(bounds.height() / 2.0 / row_height);
    const int half_columns = qCeil(bounds.width() / 2.0 / edge_length) + 1;
    const qreal clearance = edge_length * seam_line_clearance;

    QVector<QPointF> points;
    for (int row = -half_rows; row <= half_rows; ++row)
    {
        const qreal shift = (qAbs(row) % 2 == 0) ? 0.0 : edge_length / 2.0;
        for (int column = -half_columns; column <= half_columns; ++column)
        {
            const QPointF point(center.x() + column * edge_length + shift, center.y() + row * row_height);
            if (polygon.containsPoint(point, Qt::OddEvenFill) && keepsClearOfSeamLine(seam_line, point, clearance))
            {
                points.append(point);
            }
        }
    }
    return points;
}

//---------------------------------------------------------------------------------------------------------------------
// The Delaunay code can't take the same point twice. That only happens when a seam line touches itself.
bool hasDuplicates(QVector<QPointF> points)
{
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b)
    {
        return a.x() < b.x() || (!(b.x() < a.x()) && a.y() < b.y());
    });

    bool duplicates = false;
    for (int i = 1; i < points.size() && !duplicates; ++i)
    {
        duplicates = QLineF(points.at(i - 1), points.at(i)).length() <= same_point_tolerance;
    }
    return duplicates;
}

//---------------------------------------------------------------------------------------------------------------------
// Delaunay triangles of the points, three indices each. Faces with more corners, from points on a common circle,
// are split into a fan. Returns no triangles if the points can't be triangulated.
QVector<quint32> delaunayTriangles(const QVector<QPointF>& points)
{
    QVector<quint32> triangles;
    if (points.size() >= 3 && !hasDuplicates(points))
    {
        QVector<del_point2d_t> input(points.size());
        for (int i = 0; i < points.size(); ++i)
        {
            input[i].x = points.at(i).x();
            input[i].y = points.at(i).y();
        }

        delaunay2d_t* result = delaunay2d_from(input.data(), static_cast<quint32>(input.size()));
        quint32 offset = 0;
        for (quint32 face = 0; face < result->num_faces; ++face)
        {
            const quint32 corner_count = result->faces[offset];
            const quint32* corners = result->faces + offset + 1;
            // The first face is the outside of the convex hull.
            for (quint32 k = 1; face > 0 && k + 1 < corner_count; ++k)
            {
                triangles << corners[0] << corners[k] << corners[k + 1];
            }
            offset += corner_count + 1;
        }
        delaunay2d_release(result);
    }
    return triangles;
}

//---------------------------------------------------------------------------------------------------------------------
quint64 edgeKey(quint32 a, quint32 b)
{
    const quint64 low = qMin(a, b);
    const quint64 high = qMax(a, b);
    return (low << 32) | high;
}

//---------------------------------------------------------------------------------------------------------------------
// Seam line segments (by their first point's index) that aren't an edge of any triangle. The seam line points come
// first in the triangulated points, so their indices match. Without triangles there is nothing to repair.
QVector<int> missingSegments(const QVector<quint32>& triangles, const QVector<QPointF>& seam_line)
{
    const int seam_line_count = triangles.isEmpty() ? 0 : static_cast<int>(seam_line.size());

    QSet<quint64> edges;
    for (int i = 0; i + 2 < triangles.size(); i += 3)
    {
        edges.insert(edgeKey(triangles.at(i), triangles.at(i + 1)));
        edges.insert(edgeKey(triangles.at(i + 1), triangles.at(i + 2)));
        edges.insert(edgeKey(triangles.at(i + 2), triangles.at(i)));
    }

    QVector<int> missing;
    for (int i = 0; i < seam_line_count; ++i)
    {
        const quint32 a = static_cast<quint32>(i);
        const quint32 b = static_cast<quint32>((i + 1) % seam_line_count);
        if (!edges.contains(edgeKey(a, b)))
        {
            missing.append(i);
        }
    }
    return missing;
}

//---------------------------------------------------------------------------------------------------------------------
// Splits the given seam line segments in half. The halves' diametral circles lie inside the whole segment's, so the
// lattice points stay clear of them.
SeamLine splitSegments(const SeamLine& seam_line, const QVector<int>& segments)
{
    SeamLine split;
    split.points.reserve(seam_line.points.size() + segments.size());
    QVector<int> new_index(seam_line.points.size(), 0);
    int next_segment = 0;
    for (int i = 0; i < seam_line.points.size(); ++i)
    {
        new_index[i] = static_cast<int>(split.points.size());
        split.points.append(seam_line.points.at(i));
        if (next_segment < segments.size() && segments.at(next_segment) == i)
        {
            const QPointF& next = seam_line.points.at((i + 1) % seam_line.points.size());
            split.points.append((seam_line.points.at(i) + next) / 2.0);
            ++next_segment;
        }
    }

    for (const int point : seam_line.node_points)
    {
        split.node_points.append(point >= 0 ? new_index.at(point) : -1);
    }
    return split;
}

//---------------------------------------------------------------------------------------------------------------------
// Keeps the triangles inside the seam line, winds them all the same way and drops points no triangle uses.
GarmentMesh buildMesh(const SeamLine& seam_line, const QVector<OutlineNode>& nodes, const QVector<QPointF>& interior,
                      const QVector<quint32>& triangles)
{
    const QVector<QPointF> points = seam_line.points + interior;
    const QPolygonF polygon(seam_line.points);

    QVector<quint32> kept;
    kept.reserve(triangles.size());
    for (int i = 0; i + 2 < triangles.size(); i += 3)
    {
        const quint32 a = triangles.at(i);
        quint32 b = triangles.at(i + 1);
        quint32 c = triangles.at(i + 2);
        const QPointF& point_a = points.at(static_cast<int>(a));
        const QPointF& point_b = points.at(static_cast<int>(b));
        const QPointF& point_c = points.at(static_cast<int>(c));
        const qreal doubled_area = doubledTriangleArea(point_a, point_b, point_c);
        const QPointF centroid = (point_a + point_b + point_c) / 3.0;
        if (qAbs(doubled_area) > same_point_tolerance && polygon.containsPoint(centroid, Qt::OddEvenFill))
        {
            if (doubled_area < 0)
            {
                std::swap(b, c);
            }
            kept << a << b << c;
        }
    }

    QVector<int> new_index(points.size(), -1);
    for (const quint32 index : kept)
    {
        new_index[static_cast<int>(index)] = 0;
    }

    GarmentMesh mesh;
    QVector<int> boundary_position(seam_line.points.size(), -1);
    for (int i = 0; i < points.size(); ++i)
    {
        if (new_index.at(i) >= 0)
        {
            new_index[i] = static_cast<int>(mesh.rest_positions.size());
            mesh.rest_positions.append(points.at(i));
            if (i < seam_line.points.size())
            {
                boundary_position[i] = static_cast<int>(mesh.boundary.size());
                mesh.boundary.append(static_cast<quint32>(new_index.at(i)));
            }
        }
    }

    mesh.indices.reserve(kept.size());
    for (const quint32 index : kept)
    {
        mesh.indices.append(static_cast<quint32>(new_index.at(static_cast<int>(index))));
    }

    for (int k = 0; k < nodes.size() && k < seam_line.node_points.size(); ++k)
    {
        const int point = seam_line.node_points.at(k);
        if (point >= 0 && boundary_position.at(point) >= 0)
        {
            OutlineNode node = nodes.at(k);
            node.index = boundary_position.at(point);
            mesh.nodes.append(node);
        }
    }
    return mesh;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
PieceMesher::PieceMesher(qreal edge_length)
    : m_edge_length(defaultEdgeLength())
{
    setEdgeLength(edge_length);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The edge length meshes get unless told otherwise: 2 cm, CLO's recommended particle distance for editing.
qreal PieceMesher::defaultEdgeLength()
{
    return 2.0;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Target length of the triangle edges in cm.
qreal PieceMesher::edgeLength() const
{
    return m_edge_length;
}

//---------------------------------------------------------------------------------------------------------------------
void PieceMesher::setEdgeLength(qreal edge_length)
{
    m_edge_length = qMax(min_edge_length, edge_length);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Meshes the area inside a closed outline given in cm.
///
/// The outline may or may not repeat its first point at the end, and may be wound either way. An outline without
/// an area, or one that touches itself, gives an empty mesh.
GarmentMesh PieceMesher::meshPolygon(const QVector<QPointF>& outline) const
{
    return meshOutline(PieceOutline(outline, QVector<OutlineNode>()));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Meshes the area inside a piece's outline, as meshPolygon(), with a vertex on each of its path points so
/// seams can be sewn from point to point.
GarmentMesh PieceMesher::meshOutline(const PieceOutline& outline) const
{
    GarmentMesh mesh;
    const SeamLine cleaned = cleanOutline(outline);
    if (!cleaned.points.isEmpty())
    {
        SeamLine seam_line = resampleOutline(cleaned, m_edge_length);
        const QVector<QPointF> interior = latticePoints(seam_line.points, m_edge_length);

        QVector<quint32> triangles = delaunayTriangles(seam_line.points + interior);
        QVector<int> missing = missingSegments(triangles, seam_line.points);
        for (int round = 0; round < max_split_rounds && !missing.isEmpty(); ++round)
        {
            seam_line = splitSegments(seam_line, missing);
            triangles = delaunayTriangles(seam_line.points + interior);
            missing = missingSegments(triangles, seam_line.points);
        }

        if (!triangles.isEmpty())
        {
            mesh = buildMesh(seam_line, outline.nodes(), interior, triangles);
        }
    }
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Meshes the area inside a piece's seam line, at the piece's position in the piece scene, in cm.
///
/// Seam allowance is left out: it folds to the inside of the garment and doesn't change its shape.
GarmentMesh PieceMesher::meshPiece(quint32 piece_id, const VPiece& piece, const VContainer* data) const
{
    GarmentMesh mesh = meshOutline(PieceOutline::fromPiece(piece, data));
    mesh.piece_id = piece_id;
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The Delaunay triangles of points as they are, such as the vertices of a mesh known only by them, without an
/// outline: the triangles fill their convex hull. Points that can't be triangulated give a mesh without triangles.
GarmentMesh PieceMesher::meshPoints(const QVector<QPointF>& points)
{
    GarmentMesh mesh;
    mesh.rest_positions = points;
    mesh.indices = delaunayTriangles(points);
    return mesh;
}
