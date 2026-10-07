//---------------------------------------------------------------------------------------------------------------------
//  @file   topstitch.cpp
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

#include "topstitch.h"

#include <QLineF>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace
{
// Topstitching usually runs 6 mm inside the edge, in stitches 3 mm long, in thread a little under 1 mm thick.
const qreal default_distance = 0.6;
const qreal default_stitch_length = 0.3;
const qreal default_thread_width = 0.08;

// Of each stitch's length, this share shows as thread on top of the cloth; the rest is where it goes through.
const qreal thread_share = 0.8;

// A row's points nearer the line it follows than this share of the row's distance are where it loops back on itself,
// inside a curve too tight for it or past a corner it turns.
const qreal kept_share = 0.9;

// A row turns a sharp corner at most this many times its distance away from the corner.
const qreal miter_limit = 4.0;

// A row's end reaches on to the piece's edge when the edge is at most this many times the row's distance further.
const qreal reach = 3.0;

// Points closer than this, in cm, are the same point.
const qreal same_point = 1e-6;

// Barycentric weights this far below 0 still count as inside a triangle.
const qreal inside_tolerance = 1e-9;

// The style patterns topstitch in unless they say otherwise.
const QString default_style = QStringLiteral("single");

// How high a stitch rises at its middle, and how far off the cloth it lies so the cloth doesn't show through it, as
// shares of the thread's width.
const float thread_height = 0.45f;
const float thread_lift = 0.25f;

// How far the normals at a stitch's ends and sides lean along it and across it, so it lights up round.
const float end_lean = 0.5f;
const float side_lean = 2.0f;

// A stitch has five corners on each face of the cloth, its two ends, its two sides and its ridge, and four triangles
// between them.
const int corners_per_face = 5;
const quint32 face_triangles[] = {0, 4, 2, 0, 3, 4, 1, 2, 4, 1, 4, 3};

//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

//---------------------------------------------------------------------------------------------------------------------
qreal length(const QPointF& vector)
{
    return std::hypot(vector.x(), vector.y());
}

//---------------------------------------------------------------------------------------------------------------------
QPointF unit(const QPointF& vector)
{
    const qreal size = length(vector);
    return size > 0 ? vector / size : QPointF();
}

//---------------------------------------------------------------------------------------------------------------------
// Positive where the points run anticlockwise in a y up frame.
qreal signedArea(const QVector<QPointF>& points)
{
    qreal area = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        area += cross(points.at(i), points.at((i + 1) % points.size()));
    }
    return area / 2;
}

//---------------------------------------------------------------------------------------------------------------------
qreal distanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const qreal length_squared = QPointF::dotProduct(ab, ab);
    const qreal t = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - a, ab) / length_squared, 1.0) : 0.0;
    return length(point - (a + ab * t));
}

//---------------------------------------------------------------------------------------------------------------------
// Distance from the point to the line, which a closed line has from its last point back to its first too.
qreal distanceToLine(const QPointF& point, const QVector<QPointF>& line, bool closed)
{
    qreal nearest = std::numeric_limits<qreal>::infinity();
    const int count = static_cast<int>(line.size());
    for (int i = 0; i < (closed ? count : count - 1); ++i)
    {
        nearest = qMin(nearest, distanceToSegment(point, line.at(i), line.at((i + 1) % count)));
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether the point is inside the closed outline.
bool contains(const QVector<QPointF>& outline, const QPointF& point)
{
    bool inside = false;
    const int count = static_cast<int>(outline.size());
    for (int i = 0, j = count - 1; i < count; j = i++)
    {
        const QPointF& a = outline.at(i);
        const QPointF& b = outline.at(j);
        if ((a.y() > point.y()) != (b.y() > point.y())
            && point.x() < a.x() + (b.x() - a.x()) * (point.y() - a.y()) / (b.y() - a.y()))
        {
            inside = !inside;
        }
    }
    return inside;
}

//---------------------------------------------------------------------------------------------------------------------
// The parts of the line inside the closed outline. Where the line crosses the outline, a part ends or starts right on
// it.
QVector<QVector<QPointF>> partsInside(const QVector<QPointF>& line, const QVector<QPointF>& outline)
{
    QVector<QVector<QPointF>> parts;
    QVector<QPointF> part;
    const int count = static_cast<int>(outline.size());
    for (int i = 0; i + 1 < line.size(); ++i)
    {
        const QPointF& a = line.at(i);
        const QPointF along = line.at(i + 1) - a;

        QVector<qreal> cuts = {0.0, 1.0};
        for (int k = 0; k < count; ++k)
        {
            const QPointF& c = outline.at(k);
            const QPointF edge = outline.at((k + 1) % count) - c;
            const qreal denominator = cross(along, edge);
            if (qAbs(denominator) > same_point * same_point)
            {
                const qreal t = cross(c - a, edge) / denominator;
                const qreal s = cross(c - a, along) / denominator;
                if (t > 0 && t < 1 && s >= 0 && s <= 1)
                {
                    cuts.append(t);
                }
            }
        }
        std::sort(cuts.begin(), cuts.end());

        for (int k = 0; k + 1 < cuts.size(); ++k)
        {
            const QPointF from = a + along * cuts.at(k);
            const QPointF to = a + along * cuts.at(k + 1);
            if (length(to - from) <= same_point)
            {
                continue;
            }
            if (contains(outline, (from + to) / 2))
            {
                if (part.isEmpty())
                {
                    part.append(from);
                }
                part.append(to);
            }
            else if (!part.isEmpty())
            {
                if (part.size() > 1)
                {
                    parts.append(part);
                }
                part.clear();
            }
        }
    }
    if (part.size() > 1)
    {
        parts.append(part);
    }
    return parts;
}

//---------------------------------------------------------------------------------------------------------------------
// Where the ray from the origin, in the given direction of unit length, first meets the closed outline, no further
// than the given length.
bool rayHit(const QPointF& origin, const QPointF& direction, const QVector<QPointF>& outline, qreal length,
            QPointF* hit)
{
    qreal nearest = length;
    bool found = false;
    for (int i = 0; i < outline.size(); ++i)
    {
        const QPointF& a = outline.at(i);
        const QPointF edge = outline.at((i + 1) % outline.size()) - a;
        const qreal denominator = cross(direction, edge);
        if (qAbs(denominator) > same_point * same_point)
        {
            const QPointF to_edge = a - origin;
            const qreal along_ray = cross(to_edge, edge) / denominator;
            const qreal along_edge = cross(to_edge, direction) / denominator;
            if (along_ray >= 0 && along_ray <= nearest && along_edge >= 0 && along_edge <= 1)
            {
                nearest = along_ray;
                found = true;
            }
        }
    }
    if (found)
    {
        *hit = origin + direction * nearest;
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
// The points without those that repeat the one before; a closed line also without a last point that repeats its first.
QVector<QPointF> withoutRepeats(const QVector<QPointF>& points, bool closed)
{
    QVector<QPointF> kept;
    kept.reserve(points.size());
    for (const QPointF& point : points)
    {
        if (kept.isEmpty() || length(point - kept.last()) > same_point)
        {
            kept.append(point);
        }
    }
    if (closed && kept.size() > 1 && length(kept.last() - kept.first()) <= same_point)
    {
        kept.removeLast();
    }
    return kept;
}

//---------------------------------------------------------------------------------------------------------------------
// Which way, and how far in units of the distance, a row moves a point of the line where the line turns from the
// segment with the side normal before to the one with the side normal after: far enough out to keep the distance
// from both, but no more than miter_limit.
QPointF miter(const QPointF& before, const QPointF& after)
{
    const QPointF sum = before + after;
    const qreal size = length(sum);
    if (size <= same_point)
    {
        return after;  // the line turns right back
    }
    const QPointF middle = sum / size;
    return middle / qMax(QPointF::dotProduct(middle, after), 1.0 / miter_limit);
}

//---------------------------------------------------------------------------------------------------------------------
// The row the distance from the line, on its left (side 1) or its right (side -1), as far as it lies inside the
// outline: for a closed line, a closed row; for an open one, the parts of the row inside the outline. Where an open
// row stops short of the outline near its end, as at a corner where it meets another edge, it goes on to meet it.
QVector<QVector<QPointF>> offsetRow(const QVector<QPointF>& line, bool closed, qreal side, qreal distance,
                                    const QVector<QPointF>& outline)
{
    const QVector<QPointF> points = withoutRepeats(line, closed);
    const int count = static_cast<int>(points.size());
    if (count < (closed ? 3 : 2))
    {
        return QVector<QVector<QPointF>>();
    }

    const int segment_count = closed ? count : count - 1;
    QVector<QPointF> normals(segment_count);
    for (int i = 0; i < segment_count; ++i)
    {
        const QPointF along = unit(points.at((i + 1) % count) - points.at(i));
        normals[i] = QPointF(-along.y(), along.x()) * side;
    }

    QVector<QPointF> row;
    for (int i = 0; i < count; ++i)
    {
        const int before = closed ? (i - 1 + count) % count : qMax(i - 1, 0);
        const int after = closed ? i : qMin(i, segment_count - 1);
        const QPointF moved = points.at(i) + miter(normals.at(before), normals.at(after)) * distance;
        const bool end = !closed && (i == 0 || i == count - 1);
        if (end || distanceToLine(moved, points, closed) >= kept_share * distance)
        {
            row.append(moved);
        }
    }

    if (closed)
    {
        row.erase(std::remove_if(row.begin(), row.end(), [&outline](const QPointF& point)
        {
            return !contains(outline, point);
        }), row.end());
        if (row.size() < 3)
        {
            return QVector<QVector<QPointF>>();
        }
        row.append(row.first());
        return {row};
    }

    QPointF hit;
    const QPointF backwards = unit(points.at(0) - points.at(1));
    if (rayHit(row.first(), backwards, outline, reach * distance, &hit))
    {
        row.prepend(hit);
    }
    const QPointF forwards = unit(points.at(count - 1) - points.at(count - 2));
    if (rayHit(row.last(), forwards, outline, reach * distance, &hit))
    {
        row.append(hit);
    }
    return partsInside(row, outline);
}

//---------------------------------------------------------------------------------------------------------------------
TopstitchStyle makeStyle(const QString& name, const QVector<qreal>& distances, qreal stitch_length,
                         qreal thread_width)
{
    TopstitchStyle style;
    style.name = name;
    style.distances = distances;
    style.stitch_length = stitch_length;
    style.thread_width = thread_width;
    return style;
}

//---------------------------------------------------------------------------------------------------------------------
// Any direction square to the given one, for a stitch whose cloth normal runs along it.
QVector3D squareTo(const QVector3D& direction)
{
    const QVector3D other = qAbs(direction.x()) < 0.9f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
    return QVector3D::crossProduct(direction, other).normalized();
}

//---------------------------------------------------------------------------------------------------------------------
// The point the given length along the line, whose lengths up to each of its points are given.
QPointF pointAlong(const QVector<QPointF>& line, const QVector<qreal>& lengths, qreal along)
{
    const auto after = std::upper_bound(lengths.cbegin(), lengths.cend(), along);
    const int next = qBound(1, static_cast<int>(std::distance(lengths.cbegin(), after)),
                            static_cast<int>(lengths.size()) - 1);
    const qreal span = lengths.at(next) - lengths.at(next - 1);
    const qreal t = span > 0 ? qBound(0.0, (along - lengths.at(next - 1)) / span, 1.0) : 0.0;
    return line.at(next - 1) + (line.at(next) - line.at(next - 1)) * t;
}

//---------------------------------------------------------------------------------------------------------------------
// Finds which triangle of a mesh's flat shape a point lies in, or for a point outside it the nearest triangle, through
// a grid of cells that each know the triangles reaching into them.
class Locator
{
public:
    explicit Locator(const GarmentMesh& mesh);

    SurfacePoint locate(const QPointF& point) const;

private:
    const GarmentMesh&    m_mesh;
    QRectF                m_bounds;
    qreal                 m_cell;
    int                   m_columns;
    int                   m_rows;
    QVector<QVector<int>> m_cells;

    int                   column(qreal x) const;
    int                   row(qreal y) const;
    qreal                 nearest(int triangle, const QPointF& point, SurfacePoint* surface_point) const;
};

//---------------------------------------------------------------------------------------------------------------------
Locator::Locator(const GarmentMesh& mesh)
    : m_mesh(mesh)
    , m_bounds(mesh.bounds())
    , m_cell(1)
    , m_columns(1)
    , m_rows(1)
    , m_cells()
{
    const int triangle_count = mesh.triangleCount();
    const qreal area = m_bounds.width() * m_bounds.height();
    m_cell = qMax(2.0 * std::sqrt(area / qMax(triangle_count, 1)), 1e-3);
    m_columns = qMax(1, static_cast<int>(std::ceil(m_bounds.width() / m_cell)));
    m_rows = qMax(1, static_cast<int>(std::ceil(m_bounds.height() / m_cell)));
    m_cells.resize(m_columns * m_rows);

    for (int t = 0; t < triangle_count; ++t)
    {
        QRectF box;
        for (int k = 0; k < 3; ++k)
        {
            const QPointF& corner = mesh.rest_positions.at(static_cast<int>(mesh.indices.at(3 * t + k)));
            box = k == 0 ? QRectF(corner, corner) : box.united(QRectF(corner, corner));
        }
        for (int y = row(box.top()); y <= row(box.bottom()); ++y)
        {
            for (int x = column(box.left()); x <= column(box.right()); ++x)
            {
                m_cells[y * m_columns + x].append(t);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Searches outwards from the point's cell, ring by ring, until no triangle further out can be nearer.
SurfacePoint Locator::locate(const QPointF& point) const
{
    SurfacePoint best;
    qreal best_distance = std::numeric_limits<qreal>::infinity();
    const int x0 = column(point.x());
    const int y0 = row(point.y());
    const int rings = qMax(m_columns, m_rows);
    for (int ring = 0; ring <= rings && best_distance > (ring - 1) * m_cell; ++ring)
    {
        for (int y = y0 - ring; y <= y0 + ring; ++y)
        {
            for (int x = x0 - ring; x <= x0 + ring; ++x)
            {
                const bool on_ring = qAbs(x - x0) == ring || qAbs(y - y0) == ring;
                if (!on_ring || x < 0 || y < 0 || x >= m_columns || y >= m_rows)
                {
                    continue;
                }
                for (const int triangle : m_cells.at(y * m_columns + x))
                {
                    SurfacePoint candidate;
                    const qreal distance = nearest(triangle, point, &candidate);
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        best = candidate;
                        if (distance <= 0)
                        {
                            return best;
                        }
                    }
                }
            }
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
int Locator::column(qreal x) const
{
    return qBound(0, static_cast<int>(std::floor((x - m_bounds.left()) / m_cell)), m_columns - 1);
}

//---------------------------------------------------------------------------------------------------------------------
int Locator::row(qreal y) const
{
    return qBound(0, static_cast<int>(std::floor((y - m_bounds.top()) / m_cell)), m_rows - 1);
}

//---------------------------------------------------------------------------------------------------------------------
// The point of the triangle nearest to the given point, and how far away it is: 0 inside the triangle.
qreal Locator::nearest(int triangle, const QPointF& point, SurfacePoint* surface_point) const
{
    quint32 corners[3];
    QPointF positions[3];
    for (int k = 0; k < 3; ++k)
    {
        corners[k] = m_mesh.indices.at(3 * triangle + k);
        positions[k] = m_mesh.rest_positions.at(static_cast<int>(corners[k]));
        surface_point->corners[k] = corners[k];
    }

    const QPointF ab = positions[1] - positions[0];
    const QPointF ac = positions[2] - positions[0];
    const qreal area = cross(ab, ac);
    if (qAbs(area) > same_point * same_point)
    {
        const QPointF ap = point - positions[0];
        const qreal v = cross(ap, ac) / area;
        const qreal w = cross(ab, ap) / area;
        const qreal u = 1.0 - v - w;
        if (u >= -inside_tolerance && v >= -inside_tolerance && w >= -inside_tolerance)
        {
            surface_point->weights[0] = static_cast<float>(u);
            surface_point->weights[1] = static_cast<float>(v);
            surface_point->weights[2] = static_cast<float>(w);
            return 0;
        }
    }

    // Outside: the nearest point is on one of the edges.
    qreal best = std::numeric_limits<qreal>::infinity();
    for (int k = 0; k < 3; ++k)
    {
        const QPointF& a = positions[k];
        const QPointF edge = positions[(k + 1) % 3] - a;
        const qreal length_squared = QPointF::dotProduct(edge, edge);
        const qreal t = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - a, edge) / length_squared, 1.0)
                                           : 0.0;
        const qreal distance = length(point - (a + edge * t));
        if (distance < best)
        {
            best = distance;
            surface_point->weights[k] = static_cast<float>(1.0 - t);
            surface_point->weights[(k + 1) % 3] = static_cast<float>(t);
            surface_point->weights[(k + 2) % 3] = 0.0f;
        }
    }
    return qMax(best, std::numeric_limits<qreal>::min());
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the point is in the flat piece, in cm.
QPointF SurfacePoint::restPosition(const GarmentMesh& mesh) const
{
    QPointF position;
    for (int k = 0; k < 3; ++k)
    {
        if (static_cast<int>(corners[k]) < mesh.rest_positions.size())
        {
            position += mesh.rest_positions.at(static_cast<int>(corners[k])) * weights[k];
        }
    }
    return position;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the point is with the mesh's vertices at the given positions.
QVector3D SurfacePoint::position(const QVector<QVector3D>& positions) const
{
    QVector3D position;
    for (int k = 0; k < 3; ++k)
    {
        if (static_cast<int>(corners[k]) < positions.size())
        {
            position += positions.at(static_cast<int>(corners[k])) * weights[k];
        }
    }
    return position;
}

//---------------------------------------------------------------------------------------------------------------------
bool SurfacePoint::operator==(const SurfacePoint& other) const
{
    return std::equal(std::begin(corners), std::end(corners), std::begin(other.corners))
           && std::equal(std::begin(weights), std::end(weights), std::begin(other.weights));
}

//---------------------------------------------------------------------------------------------------------------------
bool SurfacePoint::operator!=(const SurfacePoint& other) const
{
    return !(*this == other);
}

//---------------------------------------------------------------------------------------------------------------------
bool ThreadStitch::operator==(const ThreadStitch& other) const
{
    return start == other.start && middle == other.middle && end == other.end && qFuzzyCompare(width, other.width);
}

//---------------------------------------------------------------------------------------------------------------------
bool ThreadStitch::operator!=(const ThreadStitch& other) const
{
    return !(*this == other);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The styles the 3D View offers, the default first: one row 6 mm inside the edge; an edge stitch 1.5 mm
/// inside; an edge stitch with a row 6 mm further in; a twin needle's two rows 4 mm apart, as on hems; and jeans'
/// two rows in heavy thread.
QVector<TopstitchStyle> TopstitchStyle::presets()
{
    return {makeStyle(default_style, {default_distance}, default_stitch_length, default_thread_width),
            makeStyle(QStringLiteral("edge"), {0.15}, 0.25, 0.07),
            makeStyle(QStringLiteral("double"), {0.15, 0.75}, 0.3, 0.08),
            makeStyle(QStringLiteral("twinNeedle"), {0.6, 1.0}, 0.3, 0.07),
            makeStyle(QStringLiteral("jeans"), {0.2, 0.85}, 0.35, 0.12)};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The preset of that name, or the default style for a name it doesn't know.
TopstitchStyle TopstitchStyle::preset(const QString& name)
{
    const QVector<TopstitchStyle> styles = presets();
    for (const TopstitchStyle& style : styles)
    {
        if (style.name == name)
        {
            return style;
        }
    }
    return styles.first();
}

//---------------------------------------------------------------------------------------------------------------------
QString TopstitchStyle::defaultName()
{
    return default_style;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far inside the edge topstitching runs, in cm.
qreal Topstitching::defaultDistance()
{
    return default_distance;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How long a stitch is, in cm: the thread on top and where it goes through the cloth.
qreal Topstitching::defaultStitchLength()
{
    return default_stitch_length;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How thick topstitching's thread is, in cm.
qreal Topstitching::defaultThreadWidth()
{
    return default_thread_width;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Rows of topstitching the distance inside the outline's stitched segments, in cm. Segments stitched one after
/// the other share a row, which turns where they meet; with every segment stitched the row is closed, its last point
/// the same as its first. An open row reaches on to the edges it ends at. A row leaves out what would loop back on
/// itself, in a curve too tight for it, and what would lie outside the piece.
QVector<QVector<QPointF>> Topstitching::rows(const PieceOutline& outline, const QVector<bool>& stitched,
                                             qreal distance)
{
    QVector<QVector<QPointF>> rows;
    const int segment_count = outline.segmentCount();
    if (segment_count == 0 || stitched.size() != segment_count || distance <= 0)
    {
        return rows;
    }

    const QVector<QPointF>& points = outline.points();
    const QVector<OutlineNode>& nodes = outline.nodes();
    const int count = static_cast<int>(points.size());
    const qreal side = signedArea(points) > 0 ? 1.0 : -1.0;

    if (!stitched.contains(false))
    {
        return offsetRow(points, true, side, distance, points);
    }

    for (int segment = 0; segment < segment_count; ++segment)
    {
        if (!stitched.at(segment) || stitched.at((segment - 1 + segment_count) % segment_count))
        {
            continue;  // not where a run of stitched segments starts
        }

        int last = segment;
        while (stitched.at((last + 1) % segment_count))
        {
            ++last;
        }

        const int from = nodes.at(segment).index;
        const int to = nodes.at((last + 1) % segment_count).index;
        const int steps = (to - from + count) % count;
        QVector<QPointF> line;
        for (int step = 0; step <= steps; ++step)
        {
            line.append(points.at((from + step) % count));
        }

        rows += offsetRow(line, false, side, distance, points);
    }
    return rows;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Stitches of the given length and thread width along the rows, laid onto the mesh, whose flat shape the rows
/// are drawn on. An open row's stitches sit in its middle, a closed row's go all the way round, a little longer or
/// shorter to fit. Rows shorter than a stitch get none.
QVector<ThreadStitch> Topstitching::stitches(const GarmentMesh& mesh, const QVector<QVector<QPointF>>& rows,
                                             qreal stitch_length, qreal thread_width)
{
    QVector<ThreadStitch> stitches;
    if (mesh.triangleCount() == 0 || stitch_length <= 0)
    {
        return stitches;
    }

    QVector<QPointF> points;  // each stitch's start, middle and end
    for (const QVector<QPointF>& row : rows)
    {
        if (row.size() < 2)
        {
            continue;
        }

        QVector<qreal> lengths;
        lengths.reserve(row.size());
        lengths.append(0);
        for (int i = 1; i < row.size(); ++i)
        {
            lengths.append(lengths.last() + length(row.at(i) - row.at(i - 1)));
        }
        const qreal total = lengths.last();
        const bool closed = row.size() > 2 && length(row.last() - row.first()) <= same_point;

        int count = 0;
        qreal spacing = stitch_length;
        qreal start = 0;
        if (closed)
        {
            count = qMax(1, qRound(total / stitch_length));
            spacing = total / count;
        }
        else
        {
            // A row a whole number of stitches long, but for rounding, takes all of them.
            count = static_cast<int>(std::floor((total + same_point) / stitch_length));
            start = (total - count * spacing) / 2;
        }

        const qreal thread = spacing * thread_share;
        start += (spacing - thread) / 2;
        for (int k = 0; k < count; ++k)
        {
            const qreal along = start + k * spacing;
            points << pointAlong(row, lengths, along) << pointAlong(row, lengths, along + thread / 2)
                   << pointAlong(row, lengths, along + thread);
        }
    }

    const QVector<SurfacePoint> located = locate(mesh, points);
    stitches.reserve(located.size() / 3);
    for (int i = 0; i + 2 < located.size(); i += 3)
    {
        stitches.append({located.at(i), located.at(i + 1), located.at(i + 2), static_cast<float>(thread_width)});
    }
    return stitches;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stitches' thread, each stitch a short spindle from where it comes out of the cloth to where it goes back
/// in, raised a little off the cloth, on both faces of it, with normals that light it round. The cloth's vertices are
/// at the given positions, with the given normals of unit length; scale makes the thread thicker and lifts it further,
/// as for stitches shown over others. The cloth's faces lie offset cm either side of its vertices, as where its
/// thickness is drawn; the thread lies on them.
ThreadMesh Topstitching::threadMesh(const QVector<ThreadStitch>& stitches, const QVector<QVector3D>& positions,
                                    const QVector<QVector3D>& normals, qreal scale, qreal offset)
{
    ThreadMesh mesh;
    const int corner_count = static_cast<int>(stitches.size()) * 2 * corners_per_face;
    mesh.positions.reserve(corner_count);
    mesh.normals.reserve(corner_count);
    mesh.indices.reserve(static_cast<int>(stitches.size()) * 2 * static_cast<int>(std::size(face_triangles)));

    const float size = static_cast<float>(scale);
    for (const ThreadStitch& stitch : stitches)
    {
        const QVector3D start = stitch.start.position(positions);
        const QVector3D middle = stitch.middle.position(positions);
        const QVector3D end = stitch.end.position(positions);
        QVector3D normal = stitch.middle.position(normals);
        normal = normal.isNull() ? QVector3D(0, 0, 1) : normal.normalized();
        QVector3D along = end - start;
        along = along.isNull() ? squareTo(normal) : along.normalized();
        QVector3D side = QVector3D::crossProduct(normal, along);
        side = side.isNull() ? squareTo(along) : side.normalized();
        const float width = stitch.width * size;

        // The same spindle on each face of the cloth, turned over for the back, so both face outwards.
        for (const float face : {1.0f, -1.0f})
        {
            const QVector3D up = normal * face;
            const QVector3D left = side * face;
            const QVector3D lift = up * (static_cast<float>(offset) + thread_lift * width);
            const quint32 first = static_cast<quint32>(mesh.positions.size());
            mesh.positions << start + lift << end + lift << middle + lift + left * (width / 2)
                           << middle + lift - left * (width / 2) << middle + lift + up * (thread_height * width);
            mesh.normals << (up - along * end_lean).normalized() << (up + along * end_lean).normalized()
                         << (up + left * side_lean).normalized() << (up - left * side_lean).normalized() << up;
            for (const quint32 corner : face_triangles)
            {
                mesh.indices.append(first + corner);
            }
        }
    }
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The points of the mesh's flat shape, in cm, as points on its surface; a point outside the shape goes to the
/// nearest point on its edge.
QVector<SurfacePoint> Topstitching::locate(const GarmentMesh& mesh, const QVector<QPointF>& points)
{
    QVector<SurfacePoint> located;
    if (mesh.triangleCount() == 0)
    {
        return located;
    }

    const Locator locator(mesh);
    located.reserve(points.size());
    for (const QPointF& point : points)
    {
        located.append(locator.locate(point));
    }
    return located;
}
