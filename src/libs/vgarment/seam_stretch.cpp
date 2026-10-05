//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_stretch.cpp
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

#include "seam_stretch.h"

#include <QLineF>
#include <QSet>

#include <algorithm>

namespace
{
// A stitch landing this close to an end of an edge, as a share of the edge, lands on that end's vertex.
const qreal vertex_tolerance = 1e-6;

//---------------------------------------------------------------------------------------------------------------------
quint64 pairKey(quint32 a, quint32 b)
{
    return (static_cast<quint64>(qMin(a, b)) << 32) | qMax(a, b);
}

//---------------------------------------------------------------------------------------------------------------------
// Takes a distance along one side over to the other, through the places where they meet, easing evenly in between.
qreal matchingDistance(const QVector<SeamMatch>& matches, qreal distance, bool from_second)
{
    qreal matching = 0;
    bool found = false;
    for (int k = 0; k + 1 < matches.size() && !found; ++k)
    {
        const SeamMatch& start = matches.at(k);
        const SeamMatch& end = matches.at(k + 1);
        const qreal from_start = from_second ? start.second : start.first;
        const qreal from_end = from_second ? end.second : end.first;
        const qreal onto_start = from_second ? start.first : start.second;
        const qreal onto_end = from_second ? end.first : end.second;

        if (distance <= from_end || k + 2 == matches.size())
        {
            const qreal span = from_end - from_start;
            const qreal share = span > 0 ? qBound(0.0, (distance - from_start) / span, 1.0) : 0.0;
            matching = onto_start + share * (onto_end - onto_start);
            found = true;
        }
    }
    return matching;
}

//---------------------------------------------------------------------------------------------------------------------
// Sews every vertex of one side onto the matching point of the other.
QVector<Stitch> sewOnto(const SeamStretch& from, const SeamStretch& onto, const QVector<SeamMatch>& matches,
                        bool from_second)
{
    QVector<Stitch> stitches;
    const QVector<qreal>& onto_distances = onto.distances();
    for (int i = 0; i < from.vertices().size(); ++i)
    {
        const qreal distance = matchingDistance(matches, from.distances().at(i), from_second);

        // The edge the point falls on, by its end, from the second point to the last.
        const auto after = std::upper_bound(onto_distances.cbegin() + 1, onto_distances.cend() - 1, distance);
        const int end = static_cast<int>(after - onto_distances.cbegin());
        const qreal edge_length = onto_distances.at(end) - onto_distances.at(end - 1);
        const qreal along = edge_length > 0
                            ? qBound(0.0, (distance - onto_distances.at(end - 1)) / edge_length, 1.0) : 0.0;

        Stitch stitch;
        stitch.vertex = from.vertices().at(i);
        stitch.edge_start = onto.vertices().at(end - 1);
        stitch.edge_end = onto.vertices().at(end);
        stitch.along = along;
        if (along <= vertex_tolerance)
        {
            stitch.edge_end = stitch.edge_start;
            stitch.along = 0;
        }
        else if (along >= 1.0 - vertex_tolerance)
        {
            stitch.edge_start = stitch.edge_end;
            stitch.along = 0;
        }

        // Where a seam runs into itself, as a dart's legs meet at its point, a vertex can land on itself.
        if (stitch.vertex != stitch.edge_start || stitch.edge_start != stitch.edge_end)
        {
            stitches.append(stitch);
        }
    }
    return stitches;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Notches are given as distances from the first point; ones off the stretch are left out. Vertices, if
/// given, name the mesh vertex of each point.
SeamStretch::SeamStretch(const QVector<QPointF>& points, const QVector<qreal>& notches,
                         const QVector<quint32>& vertices)
    : m_points(points)
    , m_notches()
    , m_vertices(vertices.size() == points.size() ? vertices : QVector<quint32>())
    , m_distances(points.size(), 0.0)
{
    for (int i = 1; i < m_points.size(); ++i)
    {
        m_distances[i] = m_distances.at(i - 1) + QLineF(m_points.at(i - 1), m_points.at(i)).length();
    }

    const qreal total = length();
    for (const qreal notch : notches)
    {
        if (notch > 0 && notch < total)
        {
            m_notches.append(notch);
        }
    }
    std::sort(m_notches.begin(), m_notches.end());
}

//---------------------------------------------------------------------------------------------------------------------
const QVector<QPointF>& SeamStretch::points() const
{
    return m_points;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far along the stretch its notches are, in cm, nearest first. Notches at its ends don't count.
const QVector<qreal>& SeamStretch::notches() const
{
    return m_notches;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mesh vertex of each point, or nothing if the stretch didn't come from a mesh.
const QVector<quint32>& SeamStretch::vertices() const
{
    return m_vertices;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far along the stretch each point is, in cm.
const QVector<qreal>& SeamStretch::distances() const
{
    return m_distances;
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamStretch::isEmpty() const
{
    return m_points.size() < 2 || length() <= 0;
}

//---------------------------------------------------------------------------------------------------------------------
qreal SeamStretch::length() const
{
    return m_distances.isEmpty() ? 0 : m_distances.last();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The point this far along the stretch, in cm; distances beyond its ends give its ends.
QPointF SeamStretch::pointAt(qreal distance) const
{
    QPointF point;
    if (!m_points.isEmpty())
    {
        const auto after = std::upper_bound(m_distances.cbegin(), m_distances.cend(), distance);
        if (after == m_distances.cbegin())
        {
            point = m_points.first();
        }
        else if (after == m_distances.cend())
        {
            point = m_points.last();
        }
        else
        {
            const int i = static_cast<int>(after - m_distances.cbegin());
            const qreal segment_length = m_distances.at(i) - m_distances.at(i - 1);
            const qreal t = (distance - m_distances.at(i - 1)) / segment_length;
            point = m_points.at(i - 1) + (m_points.at(i) - m_points.at(i - 1)) * t;
        }
    }
    return point;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The same stretch sewn the other way round.
SeamStretch SeamStretch::reversed() const
{
    QVector<QPointF> points = m_points;
    std::reverse(points.begin(), points.end());

    QVector<quint32> vertices = m_vertices;
    std::reverse(vertices.begin(), vertices.end());

    QVector<qreal> notches;
    for (const qreal notch : m_notches)
    {
        notches.append(length() - notch);
    }
    return SeamStretch(points, notches, vertices);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where two stretches sewn together meet: at their starts, at their notches, and at their ends.
///
/// Notches are matched in order, the way they are lined up when sewing, so only when both stretches have as many.
/// Otherwise there is no telling which belong together and only the ends are matched. In between, the stretches are
/// eased onto each other evenly.
QVector<SeamMatch> SeamStretch::matches(const SeamStretch& first, const SeamStretch& second)
{
    QVector<SeamMatch> found;
    if (!first.isEmpty() && !second.isEmpty())
    {
        found.append({0.0, 0.0});
        if (first.notches().size() == second.notches().size())
        {
            for (int i = 0; i < first.notches().size(); ++i)
            {
                found.append({first.notches().at(i), second.notches().at(i)});
            }
        }
        found.append({first.length(), second.length()});
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Sews two sides of a seam together, both taken from meshes so they know their vertices.
///
/// Every vertex of each side is sewn onto the point of the other side it meets: the sides meet at their ends and
/// lined up notches, and are eased evenly onto each other in between. A vertex meeting a vertex gets one stitch,
/// not one each way.
QVector<Stitch> SeamStretch::stitches(const SeamStretch& first, const SeamStretch& second)
{
    QVector<Stitch> made;
    const QVector<SeamMatch> found = matches(first, second);
    if (!found.isEmpty() && !first.vertices().isEmpty() && !second.vertices().isEmpty())
    {
        made = sewOnto(first, second, found, false);

        QSet<quint64> vertex_pairs;
        for (const Stitch& stitch : made)
        {
            if (stitch.edge_start == stitch.edge_end)
            {
                vertex_pairs.insert(pairKey(stitch.vertex, stitch.edge_start));
            }
        }

        for (const Stitch& stitch : sewOnto(second, first, found, true))
        {
            const bool onto_vertex = stitch.edge_start == stitch.edge_end;
            if (!onto_vertex || !vertex_pairs.contains(pairKey(stitch.vertex, stitch.edge_start)))
            {
                made.append(stitch);
            }
        }
    }
    return made;
}
