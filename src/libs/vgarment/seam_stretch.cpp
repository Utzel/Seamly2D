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

#include <algorithm>

//---------------------------------------------------------------------------------------------------------------------
/// @brief Notches are given as distances from the first point; ones off the stretch are left out.
SeamStretch::SeamStretch(const QVector<QPointF>& points, const QVector<qreal>& notches)
    : m_points(points)
    , m_notches()
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

    QVector<qreal> notches;
    for (const qreal notch : m_notches)
    {
        notches.append(length() - notch);
    }
    return SeamStretch(points, notches);
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
