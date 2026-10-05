//---------------------------------------------------------------------------------------------------------------------
//  @file   body_measurer.cpp
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

#include "body_measurer.h"

#include <QHash>
#include <QLineF>
#include <QPointF>

#include <algorithm>
#include <limits>

#include "body_model.h"

namespace
{
// Girths are searched in steps of this many cm, then refined around the best one in quarter steps.
const float search_step = 1.0f;

// Slice outlines whose centre is further from the middle than this share of the shoulder joint's distance belong
// to the arms.
const float torso_share = 0.8f;

struct Segment
{
    quint64 start;
    quint64 end;
};

//---------------------------------------------------------------------------------------------------------------------
quint64 edgeKey(quint32 a, quint32 b)
{
    const quint64 low = qMin(a, b);
    const quint64 high = qMax(a, b);
    return (low << 32) | high;
}

//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF& origin, const QPointF& a, const QPointF& b)
{
    return (a.x() - origin.x()) * (b.y() - origin.y()) - (a.y() - origin.y()) * (b.x() - origin.x());
}

//---------------------------------------------------------------------------------------------------------------------
// Length around the convex hull of the points (Andrew's monotone chain).
qreal hullPerimeter(QVector<QPointF> points)
{
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b)
    {
        return a.x() < b.x() || (!(b.x() < a.x()) && a.y() < b.y());
    });

    QVector<QPointF> hull;
    for (int pass = 0; pass < 2; ++pass)
    {
        const int pass_start = hull.size();
        for (const QPointF& point : points)
        {
            while (hull.size() >= pass_start + 2 && cross(hull.at(hull.size() - 2), hull.last(), point) <= 0)
            {
                hull.removeLast();
            }
            hull.append(point);
        }
        hull.removeLast();
        std::reverse(points.begin(), points.end());
    }

    qreal perimeter = 0;
    for (int i = 0; i < hull.size() && hull.size() >= 2; ++i)
    {
        perimeter += QLineF(hull.at(i), hull.at((i + 1) % hull.size())).length();
    }
    return perimeter;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurer::BodyMeasurer(const BodyModel& model)
    : m_model(model)
{}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements BodyMeasurer::measure(const QVector<QVector3D>& positions) const
{
    BodyMeasurements measurements;
    measurements.height = height(positions);
    measurements.bust = bust(positions);
    measurements.waist = waist(positions);
    measurements.hip = hip(positions);
    measurements.neck = neck(positions);
    return measurements;
}

//---------------------------------------------------------------------------------------------------------------------
qreal BodyMeasurer::height(const QVector<QVector3D>& positions) const
{
    float lowest = std::numeric_limits<float>::max();
    float highest = -lowest;
    for (int i = 0; i < m_model.skinVertexCount(); ++i)
    {
        lowest = qMin(lowest, positions.at(i).y());
        highest = qMax(highest, positions.at(i).y());
    }
    return m_model.skinVertexCount() > 0 ? highest - lowest : 0;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Fullest girth between the waist and the armpits.
qreal BodyMeasurer::bust(const QVector<QVector3D>& positions) const
{
    const float tall = static_cast<float>(height(positions));
    return extremeGirth(positions, m_model.joint(positions, QStringLiteral("spine-2")).y(),
                        m_model.joint(positions, QStringLiteral("l-shoulder")).y() - 0.02f * tall, true);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Smallest girth between the hips and the lower ribs.
qreal BodyMeasurer::waist(const QVector<QVector3D>& positions) const
{
    const float tall = static_cast<float>(height(positions));
    return extremeGirth(positions, m_model.joint(positions, QStringLiteral("pelvis")).y() + 0.03f * tall,
                        m_model.joint(positions, QStringLiteral("spine-1")).y() - 0.03f * tall, false);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Fullest girth around hips and seat, from a little below the hip joints to a little above the pelvis.
qreal BodyMeasurer::hip(const QVector<QVector3D>& positions) const
{
    const float tall = static_cast<float>(height(positions));
    return extremeGirth(positions, m_model.joint(positions, QStringLiteral("l-upper-leg")).y() - 0.075f * tall,
                        m_model.joint(positions, QStringLiteral("pelvis")).y() + 0.05f * tall, true);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Smallest girth between the base of the neck and the head.
qreal BodyMeasurer::neck(const QVector<QVector3D>& positions) const
{
    return extremeGirth(positions, m_model.joint(positions, QStringLiteral("neck")).y(),
                        m_model.joint(positions, QStringLiteral("head")).y(), false);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Length of a tape around the body at a height: the convex hull of the slice outlines whose centre lies
/// within max_center_x of the body's middle (x = 0).
qreal BodyMeasurer::tapeGirth(const QVector<QVector3D>& positions, const QVector<quint32>& triangles, float level,
                              float max_center_x)
{
    // Where the slice crosses a mesh edge, keyed by that edge, so neighbouring triangles share their crossings.
    QHash<quint64, QPointF> crossings;
    QVector<Segment> segments;
    QHash<quint64, QVector<int>> segments_at;

    for (int i = 0; i + 2 < triangles.size(); i += 3)
    {
        const quint32 corners[3] = {triangles.at(i), triangles.at(i + 1), triangles.at(i + 2)};
        quint64 keys[2] = {0, 0};
        int found = 0;
        for (int edge = 0; edge < 3; ++edge)
        {
            const quint32 a = corners[edge];
            const quint32 b = corners[(edge + 1) % 3];
            const QVector3D& pa = positions.at(static_cast<int>(a));
            const QVector3D& pb = positions.at(static_cast<int>(b));
            if ((pa.y() >= level) != (pb.y() >= level) && found < 2)
            {
                const float t = (level - pa.y()) / (pb.y() - pa.y());
                keys[found] = edgeKey(a, b);
                crossings.insert(keys[found], QPointF(pa.x() + t * (pb.x() - pa.x()), pa.z() + t * (pb.z() - pa.z())));
                ++found;
            }
        }
        if (found == 2)
        {
            segments_at[keys[0]].append(segments.size());
            segments_at[keys[1]].append(segments.size());
            segments.append({keys[0], keys[1]});
        }
    }

    // Follow the segments around into closed outlines and keep those of the torso or legs.
    QVector<bool> used(segments.size(), false);
    QVector<QPointF> kept_points;
    for (int first = 0; first < segments.size(); ++first)
    {
        if (!used.at(first))
        {
            QVector<QPointF> outline;
            int current = first;
            quint64 key = segments.at(first).start;
            while (current >= 0)
            {
                used[current] = true;
                outline.append(crossings.value(key));
                key = segments.at(current).start == key ? segments.at(current).end : segments.at(current).start;

                int next = -1;
                for (const int candidate : segments_at.value(key))
                {
                    if (!used.at(candidate))
                    {
                        next = candidate;
                    }
                }
                current = next;
            }

            qreal center_x = 0;
            for (const QPointF& point : outline)
            {
                center_x += point.x();
            }
            center_x /= outline.size();
            if (qAbs(center_x) <= max_center_x)
            {
                kept_points += outline;
            }
        }
    }

    return kept_points.size() >= 3 ? hullPerimeter(kept_points) : 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Largest or smallest tape girth between two heights.
qreal BodyMeasurer::extremeGirth(const QVector<QVector3D>& positions, float from, float to, bool largest) const
{
    const float reach = torsoReach(positions);
    auto girth = [this, &positions, reach](float level)
    {
        return tapeGirth(positions, m_model.triangles(), level, reach);
    };
    auto better = [largest](qreal candidate, qreal best)
    {
        return largest ? candidate > best : (candidate > 0 && (best <= 0 || candidate < best));
    };

    float best_level = from;
    qreal best = girth(from);
    for (float level = from + search_step; level <= to; level += search_step)
    {
        const qreal candidate = girth(level);
        if (better(candidate, best))
        {
            best = candidate;
            best_level = level;
        }
    }

    const float refine_step = search_step / 4.0f;
    for (float level = best_level - search_step + refine_step; level < best_level + search_step; level += refine_step)
    {
        const qreal candidate = girth(level);
        if (better(candidate, best))
        {
            best = candidate;
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
float BodyMeasurer::torsoReach(const QVector<QVector3D>& positions) const
{
    return torso_share * qAbs(m_model.joint(positions, QStringLiteral("l-shoulder")).x());
}
