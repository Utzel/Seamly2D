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
#include <limits>

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

// Ends of lines inside the piece this close to the seam line, as a share of the edge length, end on it.
const qreal line_snap = 0.25;

// A point of one line this close to another line, in cm, lies on it.
const qreal line_touch = 1e-4;

// A closed seam line, and which of its points each path point of the piece is, or -1 for a path point that was lost;
// and points it keeps besides, where lines inside the piece end on it.
struct SeamLine
{
    QVector<QPointF> points;
    QVector<int>     node_points;
    QVector<int>     kept_points;
};

// A line inside the seam line as it is meshed: its points in order, and for each end that lies on the seam line, which
// of the seam line's kept points it is, else -1.
struct InnerLine
{
    quint32          id = 0;
    QVector<QPointF> points;
    int              start_kept = -1;
    int              end_kept = -1;
};

// The points inside the seam line, those of the lines first, then the lattice's, and for each line which point each
// of its points is, counting the seam line's points first.
struct LaidOut
{
    QVector<QPointF>      interior;
    QVector<QVector<int>> chains;
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
        if (qAbs(turn) > corner_angle || seam_line.node_points.contains(i) || seam_line.kept_points.contains(i))
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
    for (const int point : seam_line.kept_points)
    {
        resampled.kept_points.append(kept_at.at(point));
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
// A point keeps clear of the lines inside the piece as lattice points keep clear of the seam line.
bool keepsClearOfLines(const QVector<InnerLine>& lines, const QPointF& point, qreal clearance)
{
    bool clear = true;
    for (int l = 0; l < lines.size() && clear; ++l)
    {
        const QVector<QPointF>& points = lines.at(l).points;
        for (int i = 0; i + 1 < points.size() && clear; ++i)
        {
            const QPointF& a = points.at(i);
            const QPointF& b = points.at(i + 1);
            const qreal radius = QLineF(a, b).length() / 2.0;
            clear = distanceToSegment(point, a, b) >= clearance
                    && QLineF(point, (a + b) / 2.0).length() > radius * 1.01;
        }
    }
    return clear;
}

//---------------------------------------------------------------------------------------------------------------------
// Puts a point onto the seam line where it passes closest, as a new point unless it falls on one, and returns which
// point it is; the points after a new one move up.
int addSeamPoint(SeamLine& seam_line, const QPointF& point)
{
    const int count = static_cast<int>(seam_line.points.size());
    int nearest = 0;
    qreal nearest_distance = std::numeric_limits<qreal>::infinity();
    for (int i = 0; i < count; ++i)
    {
        const qreal distance = distanceToSegment(point, seam_line.points.at(i), seam_line.points.at((i + 1) % count));
        if (distance < nearest_distance)
        {
            nearest = i;
            nearest_distance = distance;
        }
    }

    const QPointF& a = seam_line.points.at(nearest);
    const QPointF& b = seam_line.points.at((nearest + 1) % count);
    const QPointF ab = b - a;
    const qreal length_squared = QPointF::dotProduct(ab, ab);
    const qreal along = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - a, ab) / length_squared, 1.0)
                                           : 0.0;
    const QPointF on_line = a + ab * along;
    if (QLineF(on_line, a).length() <= same_point_tolerance)
    {
        return nearest;
    }
    if (QLineF(on_line, b).length() <= same_point_tolerance)
    {
        return (nearest + 1) % count;
    }

    const int index = nearest + 1;
    seam_line.points.insert(index, on_line);
    for (int& node_point : seam_line.node_points)
    {
        node_point += node_point >= index ? 1 : 0;
    }
    for (int& kept_point : seam_line.kept_points)
    {
        kept_point += kept_point >= index ? 1 : 0;
    }
    return index;
}

//---------------------------------------------------------------------------------------------------------------------
// The lines inside the piece as they are meshed: without repeated points, an end close to the seam line put onto it
// and kept there, and points outside the seam line left out.
QVector<InnerLine> startLines(const QVector<OutlineLine>& lines, SeamLine& seam_line, qreal snap)
{
    QVector<InnerLine> started;
    for (const OutlineLine& line : lines)
    {
        QVector<QPointF> points;
        for (const QPointF& point : line.points)
        {
            if (points.isEmpty() || QLineF(points.last(), point).length() > same_point_tolerance)
            {
                points.append(point);
            }
        }
        if (points.size() < 2)
        {
            continue;
        }

        auto seam_distance = [&seam_line](const QPointF& point)
        {
            qreal nearest = std::numeric_limits<qreal>::infinity();
            const int count = static_cast<int>(seam_line.points.size());
            for (int i = 0; i < count; ++i)
            {
                nearest = qMin(nearest, distanceToSegment(point, seam_line.points.at(i),
                                                          seam_line.points.at((i + 1) % count)));
            }
            return nearest;
        };

        InnerLine inner;
        inner.id = line.id;
        const bool start_on_seam = seam_distance(points.first()) <= snap;
        const bool end_on_seam = seam_distance(points.last()) <= snap;
        const QPolygonF polygon(seam_line.points);
        for (int i = 0; i < points.size(); ++i)
        {
            const bool first = i == 0;
            const bool last = i == points.size() - 1;
            if ((first && start_on_seam) || (last && end_on_seam) || polygon.containsPoint(points.at(i),
                                                                                          Qt::OddEvenFill))
            {
                inner.points.append(points.at(i));
            }
        }
        if (inner.points.size() < 2)
        {
            continue;
        }

        if (start_on_seam)
        {
            const int index = addSeamPoint(seam_line, inner.points.first());
            inner.points.first() = seam_line.points.at(index);
            seam_line.kept_points.append(index);
            inner.start_kept = static_cast<int>(seam_line.kept_points.size()) - 1;
        }
        if (end_on_seam)
        {
            const int index = addSeamPoint(seam_line, inner.points.last());
            inner.points.last() = seam_line.points.at(index);
            seam_line.kept_points.append(index);
            inner.end_kept = static_cast<int>(seam_line.kept_points.size()) - 1;
        }
        started.append(inner);
    }
    return started;
}

//---------------------------------------------------------------------------------------------------------------------
// The line's points about the edge length apart, keeping its ends and its corners.
QVector<QPointF> resampledLine(const QVector<QPointF>& line, qreal edge_length)
{
    QVector<int> kept{0};
    for (int i = 1; i + 1 < line.size(); ++i)
    {
        qreal turn = QLineF(line.at(i - 1), line.at(i)).angleTo(QLineF(line.at(i), line.at(i + 1)));
        if (turn > 180.0)
        {
            turn -= 360.0;
        }
        if (qAbs(turn) > corner_angle)
        {
            kept.append(i);
        }
    }
    kept.append(static_cast<int>(line.size()) - 1);

    QVector<QPointF> resampled;
    for (int k = 0; k + 1 < kept.size(); ++k)
    {
        appendResampledRun(line.mid(kept.at(k), kept.at(k + 1) - kept.at(k) + 1), edge_length, resampled);
    }
    resampled.append(line.last());
    return resampled;
}

//---------------------------------------------------------------------------------------------------------------------
// Puts a point onto the line where it lies on one of its segments, unless the line has a point there already.
void insertOnLine(QVector<QPointF>& line, const QPointF& point)
{
    int nearest = -1;
    qreal nearest_distance = line_touch;
    for (int i = 0; i < line.size(); ++i)
    {
        if (QLineF(line.at(i), point).length() <= same_point_tolerance)
        {
            return;
        }
        if (i + 1 < line.size())
        {
            const qreal distance = distanceToSegment(point, line.at(i), line.at(i + 1));
            if (distance <= nearest_distance)
            {
                nearest = i;
                nearest_distance = distance;
            }
        }
    }
    if (nearest >= 0)
    {
        line.insert(nearest + 1, point);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Where lines cross or touch, both get a point there, so they share a vertex: a line passing through a point of
// another, or crossing it between points, would keep the triangulation from following both.
void joinLines(QVector<InnerLine>& lines)
{
    auto cross = [](const QPointF& u, const QPointF& v)
    {
        return u.x() * v.y() - u.y() * v.x();
    };

    for (int a = 0; a < lines.size(); ++a)
    {
        for (int b = a + 1; b < lines.size(); ++b)
        {
            QVector<QPointF> joints;
            const QVector<QPointF>& first = lines.at(a).points;
            const QVector<QPointF>& second = lines.at(b).points;
            for (int i = 0; i + 1 < first.size(); ++i)
            {
                for (int j = 0; j + 1 < second.size(); ++j)
                {
                    const QPointF& p = first.at(i);
                    const QPointF& q = second.at(j);
                    const QPointF r = first.at(i + 1) - p;
                    const QPointF s = second.at(j + 1) - q;
                    for (const QPointF& end : {p, first.at(i + 1)})
                    {
                        if (distanceToSegment(end, q, second.at(j + 1)) <= line_touch)
                        {
                            joints.append(end);
                        }
                    }
                    for (const QPointF& end : {q, second.at(j + 1)})
                    {
                        if (distanceToSegment(end, p, first.at(i + 1)) <= line_touch)
                        {
                            joints.append(end);
                        }
                    }
                    const qreal turn = cross(r, s);
                    if (qAbs(turn) > same_point_tolerance)
                    {
                        const qreal along_first = cross(q - p, s) / turn;
                        const qreal along_second = cross(q - p, r) / turn;
                        if (along_first > 0 && along_first < 1 && along_second > 0 && along_second < 1)
                        {
                            joints.append(p + r * along_first);
                        }
                    }
                }
            }
            for (const QPointF& joint : joints)
            {
                insertOnLine(lines[a].points, joint);
                insertOnLine(lines[b].points, joint);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The points inside the seam line, each line's points that aren't seam line points first, each only once, then the
// lattice's.
LaidOut layOut(const SeamLine& seam_line, const QVector<InnerLine>& lines, const QVector<QPointF>& lattice)
{
    LaidOut laid;
    const int seam_count = static_cast<int>(seam_line.points.size());
    auto index_of = [&seam_line, &laid, seam_count](const QPointF& point)
    {
        for (int i = 0; i < seam_count; ++i)
        {
            if (QLineF(seam_line.points.at(i), point).length() <= same_point_tolerance)
            {
                return i;
            }
        }
        for (int i = 0; i < laid.interior.size(); ++i)
        {
            if (QLineF(laid.interior.at(i), point).length() <= same_point_tolerance)
            {
                return seam_count + i;
            }
        }
        laid.interior.append(point);
        return seam_count + static_cast<int>(laid.interior.size()) - 1;
    };

    for (const InnerLine& line : lines)
    {
        QVector<int> chain;
        for (int i = 0; i < line.points.size(); ++i)
        {
            if (i == 0 && line.start_kept >= 0)
            {
                chain.append(seam_line.kept_points.at(line.start_kept));
            }
            else if (i == line.points.size() - 1 && line.end_kept >= 0)
            {
                chain.append(seam_line.kept_points.at(line.end_kept));
            }
            else
            {
                chain.append(index_of(line.points.at(i)));
            }
        }
        laid.chains.append(chain);
    }
    laid.interior += lattice;
    return laid;
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
// Splits the segments of the lines that aren't an edge of any triangle in half; says whether there were any.
bool splitLineSegments(const QVector<quint32>& triangles, const LaidOut& laid, QVector<InnerLine>& lines)
{
    QSet<quint64> edges;
    for (int i = 0; i + 2 < triangles.size(); i += 3)
    {
        edges.insert(edgeKey(triangles.at(i), triangles.at(i + 1)));
        edges.insert(edgeKey(triangles.at(i + 1), triangles.at(i + 2)));
        edges.insert(edgeKey(triangles.at(i + 2), triangles.at(i)));
    }

    bool split = false;
    for (int l = 0; l < lines.size(); ++l)
    {
        const QVector<int>& chain = laid.chains.at(l);
        QVector<QPointF> points;
        for (int i = 0; i < chain.size(); ++i)
        {
            points.append(lines.at(l).points.at(i));
            if (i + 1 < chain.size() && chain.at(i) != chain.at(i + 1)
                && !edges.contains(edgeKey(static_cast<quint32>(chain.at(i)), static_cast<quint32>(chain.at(i + 1)))))
            {
                points.append((lines.at(l).points.at(i) + lines.at(l).points.at(i + 1)) / 2.0);
                split = true;
            }
        }
        lines[l].points = points;
    }
    return split;
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
    for (const int point : seam_line.kept_points)
    {
        split.kept_points.append(new_index.at(point));
    }
    return split;
}

//---------------------------------------------------------------------------------------------------------------------
// Keeps the triangles inside the seam line, winds them all the same way and drops points no triangle uses. The lines
// go along the vertices of their points.
GarmentMesh buildMesh(const SeamLine& seam_line, const QVector<OutlineNode>& nodes, const LaidOut& laid,
                      const QVector<InnerLine>& lines, const QVector<quint32>& triangles)
{
    const QVector<QPointF> points = seam_line.points + laid.interior;
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

    for (int l = 0; l < lines.size() && l < laid.chains.size(); ++l)
    {
        MeshLine line;
        line.id = lines.at(l).id;
        for (const int point : laid.chains.at(l))
        {
            const int vertex = new_index.at(point);
            if (vertex >= 0 && (line.vertices.isEmpty() || line.vertices.last() != static_cast<quint32>(vertex)))
            {
                line.vertices.append(static_cast<quint32>(vertex));
            }
        }
        if (line.vertices.size() > 1)
        {
            mesh.lines.append(line);
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
    SeamLine cleaned = cleanOutline(outline);
    if (!cleaned.points.isEmpty())
    {
        QVector<InnerLine> lines = startLines(outline.lines(), cleaned, m_edge_length * line_snap);
        SeamLine seam_line = resampleOutline(cleaned, m_edge_length);
        for (InnerLine& line : lines)
        {
            line.points = resampledLine(line.points, m_edge_length);
        }
        joinLines(lines);
        QVector<QPointF> lattice;
        for (const QPointF& point : latticePoints(seam_line.points, m_edge_length))
        {
            if (keepsClearOfLines(lines, point, m_edge_length * seam_line_clearance))
            {
                lattice.append(point);
            }
        }

        LaidOut laid = layOut(seam_line, lines, lattice);
        QVector<quint32> triangles = delaunayTriangles(seam_line.points + laid.interior);
        for (int round = 0; round < max_split_rounds && !triangles.isEmpty(); ++round)
        {
            const QVector<int> missing = missingSegments(triangles, seam_line.points);
            const bool lines_split = splitLineSegments(triangles, laid, lines);
            if (missing.isEmpty() && !lines_split)
            {
                break;
            }
            seam_line = splitSegments(seam_line, missing);
            laid = layOut(seam_line, lines, lattice);
            triangles = delaunayTriangles(seam_line.points + laid.interior);
        }

        if (!triangles.isEmpty())
        {
            mesh = buildMesh(seam_line, outline.nodes(), laid, lines, triangles);
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
