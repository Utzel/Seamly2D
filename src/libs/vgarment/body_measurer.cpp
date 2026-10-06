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

// Near the armpits the arms join the torso in one slice outline. An outline reaching further out than this share of
// the shoulder joint's distance has arms in it, so the bust isn't measured there.
const float chest_share = 1.25f;

// Slice outlines whose centre is nearer to the leg's middle than this share of the hip joint's distance from the
// middle of the body belong to the leg.
const float leg_share = 0.7f;

// The calf is searched for between these shares of the way down from the knee to the ankle, below the knee itself.
const float calf_from = 0.1f;
const float calf_to = 0.5f;

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

//---------------------------------------------------------------------------------------------------------------------
// The closed outlines where a horizontal slice at a height cuts the mesh, as points in x and z.
QVector<QVector<QPointF>> sliceOutlines(const QVector<QVector3D>& positions, const QVector<quint32>& triangles,
                                        float level)
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

    // Follow the segments around into closed outlines.
    QVector<QVector<QPointF>> outlines;
    QVector<bool> used(segments.size(), false);
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
            outlines.append(outline);
        }
    }
    return outlines;
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
    measurements.upper_arm = upperArm(positions);
    measurements.lower_arm = lowerArm(positions);
    measurements.arm = measurements.upper_arm + measurements.lower_arm;
    measurements.crotch = crotch(positions);
    measurements.knee_height = kneeHeight(positions);
    measurements.knee = knee(positions);
    measurements.calf = calf(positions);
    return measurements;
}

//---------------------------------------------------------------------------------------------------------------------
qreal BodyMeasurer::height(const QVector<QVector3D>& positions) const
{
    float highest = -std::numeric_limits<float>::max();
    for (int i = 0; i < m_model.skinVertexCount(); ++i)
    {
        highest = qMax(highest, positions.at(i).y());
    }
    return m_model.skinVertexCount() > 0 ? highest - lowest(positions) : 0;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Fullest girth between the waist and the armpits, where the arms are still clear of the chest.
qreal BodyMeasurer::bust(const QVector<QVector3D>& positions) const
{
    const float tall = static_cast<float>(height(positions));
    return extremeGirth(positions, m_model.joint(positions, QStringLiteral("spine-2")).y(),
                        m_model.joint(positions, QStringLiteral("l-shoulder")).y() - 0.04f * tall, true,
                        chest_share * shoulderDistance(positions));
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
/// @brief Fullest girth around hips and seat, from a little below the hip joints to a little above the pelvis, but
/// not below the crotch, where the tape would go around the thighs.
qreal BodyMeasurer::hip(const QVector<QVector3D>& positions) const
{
    const float tall = static_cast<float>(height(positions));
    return extremeGirth(positions,
                        qMax(m_model.joint(positions, QStringLiteral("l-upper-leg")).y() - 0.075f * tall,
                             m_model.crotch(positions).y() + search_step),
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
/// within max_center_x of the body's middle (x = 0). With a max_extent_x, a kept outline reaching further out than
/// that makes the slice unusable and the result 0.
qreal BodyMeasurer::tapeGirth(const QVector<QVector3D>& positions, const QVector<quint32>& triangles, float level,
                              float max_center_x, float max_extent_x)
{
    QVector<QPointF> kept_points;
    bool too_wide = false;
    for (const QVector<QPointF>& outline : sliceOutlines(positions, triangles, level))
    {
        qreal center_x = 0;
        qreal extent_x = 0;
        for (const QPointF& point : outline)
        {
            center_x += point.x();
            extent_x = qMax(extent_x, qAbs(point.x()));
        }
        center_x /= outline.size();
        if (qAbs(center_x) <= max_center_x)
        {
            kept_points += outline;
            too_wide = too_wide || (max_extent_x > 0 && extent_x > max_extent_x);
        }
    }

    return (kept_points.size() >= 3 && !too_wide) ? hullPerimeter(kept_points) : 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Largest or smallest tape girth between two heights, leaving out unusable slices.
qreal BodyMeasurer::extremeGirth(const QVector<QVector3D>& positions, float from, float to, bool largest,
                                 float max_extent_x) const
{
    const float reach = torso_share * shoulderDistance(positions);
    auto girth = [this, &positions, reach, max_extent_x](float level)
    {
        return tapeGirth(positions, m_model.triangles(), level, reach, max_extent_x);
    };
    auto better = [largest](qreal candidate, qreal best)
    {
        return candidate > 0 && (best <= 0 || (largest ? candidate > best : candidate < best));
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
// How far the shoulder joint is from the body's middle.
float BodyMeasurer::shoulderDistance(const QVector<QVector3D>& positions) const
{
    return qAbs(m_model.joint(positions, QStringLiteral("l-shoulder")).x());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the shoulder tip to the elbow, the left arm's.
qreal BodyMeasurer::upperArm(const QVector<QVector3D>& positions) const
{
    return (m_model.joint(positions, QStringLiteral("l-elbow")) - shoulderTip(positions)).length();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the elbow to the wrist, the left arm's.
qreal BodyMeasurer::lowerArm(const QVector<QVector3D>& positions) const
{
    return (m_model.joint(positions, QStringLiteral("l-hand")) - m_model.joint(positions, QStringLiteral("l-elbow")))
        .length();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the shoulder tip to the wrist, the left arm's.
qreal BodyMeasurer::arm(const QVector<QVector3D>& positions) const
{
    return upperArm(positions) + lowerArm(positions);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The left shoulder tip, where arm lengths start: where the skin is highest straight above the shoulder joint;
/// the joint itself if no skin is above it.
QVector3D BodyMeasurer::shoulderTip(const QVector<QVector3D>& positions) const
{
    const QVector3D joint = m_model.joint(positions, QStringLiteral("l-shoulder"));
    const QPointF above(joint.x(), joint.z());
    const QVector<quint32>& triangles = m_model.triangles();
    QVector3D tip = joint;
    for (int t = 0; t + 2 < triangles.size(); t += 3)
    {
        const QVector3D a = positions.at(static_cast<int>(triangles.at(t)));
        const QVector3D b = positions.at(static_cast<int>(triangles.at(t + 1)));
        const QVector3D c = positions.at(static_cast<int>(triangles.at(t + 2)));

        // Where the vertical line through the joint meets the triangle, seen from above.
        const qreal area = cross(QPointF(a.x(), a.z()), QPointF(b.x(), b.z()), QPointF(c.x(), c.z()));
        if (qAbs(area) > 1e-9)
        {
            const qreal u = cross(above, QPointF(b.x(), b.z()), QPointF(c.x(), c.z())) / area;
            const qreal v = cross(QPointF(a.x(), a.z()), above, QPointF(c.x(), c.z())) / area;
            const qreal w = 1.0 - u - v;
            const float height = static_cast<float>(u * a.y() + v * b.y() + w * c.y());
            if (u >= 0 && v >= 0 && w >= 0 && height > tip.y())
            {
                tip = QVector3D(joint.x(), height, joint.z());
            }
        }
    }
    return tip;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the floor to the crotch, the inside leg.
qreal BodyMeasurer::crotch(const QVector<QVector3D>& positions) const
{
    return m_model.crotch(positions).y() - lowest(positions);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the floor to the knee joint, as high as the fold at the back of the knee.
qreal BodyMeasurer::kneeHeight(const QVector<QVector3D>& positions) const
{
    return m_model.joint(positions, QStringLiteral("l-knee")).y() - lowest(positions);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Girth around the knee joint.
qreal BodyMeasurer::knee(const QVector<QVector3D>& positions) const
{
    const float knee_level = m_model.joint(positions, QStringLiteral("l-knee")).y();
    return legGirth(positions, knee_level, knee_level);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Fullest girth of the lower leg, below the knee.
qreal BodyMeasurer::calf(const QVector<QVector3D>& positions) const
{
    const float knee_level = m_model.joint(positions, QStringLiteral("l-knee")).y();
    const float ankle_level = m_model.joint(positions, QStringLiteral("l-ankle")).y();
    return legGirth(positions, knee_level - calf_to * (knee_level - ankle_level),
                    knee_level - calf_from * (knee_level - ankle_level));
}

//---------------------------------------------------------------------------------------------------------------------
// The height of the soles of the feet.
float BodyMeasurer::lowest(const QVector<QVector3D>& positions) const
{
    float soles = std::numeric_limits<float>::max();
    for (int i = 0; i < m_model.skinVertexCount(); ++i)
    {
        soles = qMin(soles, positions.at(i).y());
    }
    return m_model.skinVertexCount() > 0 ? soles : 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Fullest girth of the left leg between two heights: of the slice outlines around the leg's middle, which runs from
// the hip joint through the knee to the ankle.
qreal BodyMeasurer::legGirth(const QVector<QVector3D>& positions, float from, float to) const
{
    const QVector3D joints[3] = {m_model.joint(positions, QStringLiteral("l-upper-leg")),
                                 m_model.joint(positions, QStringLiteral("l-knee")),
                                 m_model.joint(positions, QStringLiteral("l-ankle"))};
    const float reach = leg_share * qAbs(joints[0].x() - m_model.joint(positions, QStringLiteral("pelvis")).x());

    qreal fullest = 0;
    for (float level = from; level <= to; level += search_step / 4.0f)
    {
        float middle = level >= joints[0].y() ? joints[0].x() : joints[2].x();
        for (int k = 0; k < 2; ++k)
        {
            if (level < joints[k].y() && level >= joints[k + 1].y())
            {
                const float t = (joints[k].y() - level) / (joints[k].y() - joints[k + 1].y());
                middle = joints[k].x() + t * (joints[k + 1].x() - joints[k].x());
            }
        }

        QVector<QPointF> kept_points;
        for (const QVector<QPointF>& outline : sliceOutlines(positions, m_model.triangles(), level))
        {
            qreal center_x = 0;
            for (const QPointF& point : outline)
            {
                center_x += point.x();
            }
            center_x /= outline.size();
            if (qAbs(center_x - middle) <= reach)
            {
                kept_points += outline;
            }
        }
        if (kept_points.size() >= 3)
        {
            fullest = qMax(fullest, hullPerimeter(kept_points));
        }
    }
    return fullest;
}
