//---------------------------------------------------------------------------------------------------------------------
//  @file   limb_line.cpp
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

#include "limb_line.h"

#include <QtMath>

#include <limits>

namespace
{
// The line is kept as points this many cm apart.
const qreal step = 0.5;

//---------------------------------------------------------------------------------------------------------------------
// The part of a vector square to a direction, as a unit vector; the fallback when there is none.
QVector3D squareTo(const QVector3D& vector, const QVector3D& direction, const QVector3D& fallback)
{
    const QVector3D square = vector - direction * QVector3D::dotProduct(vector, direction);
    return square.lengthSquared() > 1e-8f ? square.normalized() : fallback;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The line through the joints, in order, bent in an arc from `bend` cm before to `bend` cm after each joint
/// between the first and the last, and going on `before` cm before the first joint and `after` cm after the last.
LimbLine::LimbLine(const QVector<QVector3D>& joints, qreal bend, qreal before, qreal after)
{
    QVector<QVector3D> directions;
    QVector<qreal> corners;  // where along the line each joint between the first and the last is
    for (int i = 0; i + 1 < joints.size(); ++i)
    {
        const QVector3D bone = joints.at(i + 1) - joints.at(i);
        if (bone.lengthSquared() > 1e-8f)
        {
            if (!directions.isEmpty())
            {
                corners.append(m_length);
            }
            directions.append(bone.normalized());
            m_length += bone.length();
        }
    }
    if (directions.isEmpty())
    {
        return;
    }

    // Each joint's arc can't reach past the middle of the bones on either side of it.
    auto directionAlong = [&](qreal along)
    {
        QVector3D direction = directions.first();
        for (int k = 0; k < corners.size(); ++k)
        {
            const qreal previous = k > 0 ? corners.at(k - 1) : 0;
            const qreal next = k + 1 < corners.size() ? corners.at(k + 1) : m_length;
            const qreal reach = qMin(bend, qMin(corners.at(k) - previous, next - corners.at(k)) / 2.0);
            if (along >= corners.at(k) + reach)
            {
                direction = directions.at(k + 1);
            }
            else if (along > corners.at(k) - reach)
            {
                const float t = static_cast<float>((along - corners.at(k) + reach) / (2.0 * reach));
                direction = (directions.at(k) * (1.0f - t) + directions.at(k + 1) * t).normalized();
            }
        }
        return direction;
    };

    const int count = qCeil((before + m_length + after) / step) + 1;
    m_start = -before;
    QVector3D point = joints.first() - directions.first() * static_cast<float>(before);
    QVector3D front = squareTo(QVector3D(0, 0, 1), directions.first(), QVector3D(1, 0, 0));
    m_points.reserve(count);
    m_directions.reserve(count);
    m_fronts.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        const QVector3D direction = directionAlong(alongPoint(i));
        if (i > 0)
        {
            point += directionAlong(alongPoint(i) - step / 2.0) * static_cast<float>(step);
        }
        front = squareTo(front, direction, front);
        m_points.append(point);
        m_directions.append(direction);
        m_fronts.append(front);
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool LimbLine::isEmpty() const
{
    return m_points.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief From the first joint to the last, along the bones.
qreal LimbLine::length() const
{
    return m_length;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The point of the line this far along it, going on straight beyond its ends.
QVector3D LimbLine::pointAt(qreal along) const
{
    QVector3D point;
    if (!isEmpty())
    {
        int index = 0;
        float t = 0;
        sample(along, &index, &t);
        const qreal beyond = along - alongPoint(index) - t * step;
        point = m_points.at(index) + (m_points.at(index + 1) - m_points.at(index)) * t
                + m_directions.at(index) * static_cast<float>(beyond);
    }
    return point;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Which way the line runs there, as a unit vector.
QVector3D LimbLine::directionAt(qreal along) const
{
    QVector3D direction;
    if (!isEmpty())
    {
        int index = 0;
        float t = 0;
        sample(along, &index, &t);
        direction = (m_directions.at(index) * (1.0f - t) + m_directions.at(index + 1) * t).normalized();
    }
    return direction;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The front there: the unit vector square to the line at 0 degrees.
QVector3D LimbLine::frontAt(qreal along) const
{
    QVector3D front;
    if (!isEmpty())
    {
        int index = 0;
        float t = 0;
        sample(along, &index, &t);
        front = squareTo(m_fronts.at(index) * (1.0f - t) + m_fronts.at(index + 1) * t, directionAt(along),
                         m_fronts.at(index));
    }
    return front;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The unit vector square to the line there that is the angle in degrees around it from the front.
QVector3D LimbLine::aroundAt(qreal along, qreal angle) const
{
    const QVector3D front = frontAt(along);
    const QVector3D quarter = QVector3D::crossProduct(front, directionAt(along));
    const qreal radians = qDegreesToRadians(angle);
    return front * static_cast<float>(qCos(radians)) + quarter * static_cast<float>(qSin(radians));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far along the line its point nearest to the point is, between the line's ends as kept; how far the
/// point is from it goes to distance.
qreal LimbLine::alongNearest(const QVector3D& point, qreal* distance) const
{
    qreal nearest_along = 0;
    float nearest = std::numeric_limits<float>::infinity();
    for (int i = 0; i + 1 < m_points.size(); ++i)
    {
        const QVector3D a = m_points.at(i);
        const QVector3D ab = m_points.at(i + 1) - a;
        const float t = qBound(0.0f, QVector3D::dotProduct(point - a, ab) / ab.lengthSquared(), 1.0f);
        const float squared = (point - (a + ab * t)).lengthSquared();
        if (squared < nearest)
        {
            nearest = squared;
            nearest_along = alongPoint(i) + t * step;
        }
    }
    if (distance != nullptr)
    {
        *distance = isEmpty() ? 0 : qSqrt(nearest);
    }
    return nearest_along;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far along the line it is at this height above the floor, for a limb that points downwards all along,
/// as arms and legs do as they hang.
qreal LimbLine::alongAtHeight(qreal height) const
{
    qreal along = 0;
    if (!isEmpty())
    {
        int below = 0;
        while (below < m_points.size() - 1 && m_points.at(below).y() > height)
        {
            ++below;
        }

        // Beyond the ends the line goes on straight.
        const int from = qBound(0, below - 1, m_points.size() - 2);
        const qreal drop = m_points.at(from).y() - m_points.at(from + 1).y();
        along = alongPoint(from) + (drop > 1e-6 ? (m_points.at(from).y() - height) / drop * step : 0);
    }
    return along;
}

//---------------------------------------------------------------------------------------------------------------------
qreal LimbLine::alongPoint(int index) const
{
    return m_start + index * step;
}

//---------------------------------------------------------------------------------------------------------------------
// Between which two kept points a place along the line is, and how far from the first to the second; beyond the
// ends, the end pair.
void LimbLine::sample(qreal along, int* index, float* t) const
{
    const qreal position = (along - m_start) / step;
    *index = qBound(0, qFloor(position), m_points.size() - 2);
    *t = static_cast<float>(qBound(0.0, position - *index, 1.0));
}
