//---------------------------------------------------------------------------------------------------------------------
//  @file   body_collider.cpp
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

#include "body_collider.h"

#include <QtMath>

#include <algorithm>
#include <limits>

namespace
{
// Grid cubes are about this many triangle edges wide, within these sizes in cm.
const float cell_edges = 3.0f;
const float smallest_cell = 1.0f;
const float largest_cell = 10.0f;

// Triangles this much closer than another, in square cm, are taken as closer; nearer than that the one facing the
// point better wins, which keeps the inside and outside apart where triangles meet at an edge.
const float same_distance = 1e-8f;

//---------------------------------------------------------------------------------------------------------------------
// The point of triangle abc closest to p (Ericson, Real-Time Collision Detection, 5.1.5).
QVector3D closestOnTriangle(const QVector3D& p, const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D ap = p - a;
    const float d1 = QVector3D::dotProduct(ab, ap);
    const float d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
    {
        return a;
    }

    const QVector3D bp = p - b;
    const float d3 = QVector3D::dotProduct(ab, bp);
    const float d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
    {
        return b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        return a + ab * (d1 / (d1 - d3));
    }

    const QVector3D cp = p - c;
    const float d5 = QVector3D::dotProduct(ab, cp);
    const float d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
    {
        return c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        return a + ac * (d2 / (d2 - d6));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }

    const float denominator = 1.0f / (va + vb + vc);
    return a + ab * (vb * denominator) + ac * (vc * denominator);
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The triangles are three vertex indices each, counter-clockwise seen from outside the body.
BodyCollider::BodyCollider(const QVector<QVector3D>& positions, const QVector<quint32>& triangles)
    : m_positions(positions)
    , m_triangles(triangles)
{
    const int triangle_count = static_cast<int>(m_triangles.size() / 3);
    if (triangle_count == 0)
    {
        return;
    }

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum(-largest, -largest, -largest);
    float edge_sum = 0;
    m_normals.reserve(triangle_count);
    for (int t = 0; t < triangle_count; ++t)
    {
        const QVector3D& a = m_positions.at(static_cast<int>(m_triangles.at(3 * t)));
        const QVector3D& b = m_positions.at(static_cast<int>(m_triangles.at(3 * t + 1)));
        const QVector3D& c = m_positions.at(static_cast<int>(m_triangles.at(3 * t + 2)));
        m_normals.append(QVector3D::crossProduct(b - a, c - a).normalized());
        edge_sum += (b - a).length();
        for (const QVector3D& corner : {a, b, c})
        {
            minimum = QVector3D(qMin(minimum.x(), corner.x()), qMin(minimum.y(), corner.y()),
                                qMin(minimum.z(), corner.z()));
            maximum = QVector3D(qMax(maximum.x(), corner.x()), qMax(maximum.y(), corner.y()),
                                qMax(maximum.z(), corner.z()));
        }
    }

    m_cell_size = qBound(smallest_cell, cell_edges * edge_sum / triangle_count, largest_cell);
    m_grid_origin = minimum - QVector3D(m_cell_size, m_cell_size, m_cell_size);
    const QVector3D extent = maximum - m_grid_origin + QVector3D(m_cell_size, m_cell_size, m_cell_size);
    m_cells_x = qMax(1, qCeil(extent.x() / m_cell_size));
    m_cells_y = qMax(1, qCeil(extent.y() / m_cell_size));
    m_cells_z = qMax(1, qCeil(extent.z() / m_cell_size));

    // Each triangle goes into every cube its bounding box touches: count them, then fill.
    auto cell_range = [this](int t, int* from, int* to)
    {
        QVector3D low = m_positions.at(static_cast<int>(m_triangles.at(3 * t)));
        QVector3D high = low;
        for (int k = 1; k < 3; ++k)
        {
            const QVector3D& corner = m_positions.at(static_cast<int>(m_triangles.at(3 * t + k)));
            low = QVector3D(qMin(low.x(), corner.x()), qMin(low.y(), corner.y()), qMin(low.z(), corner.z()));
            high = QVector3D(qMax(high.x(), corner.x()), qMax(high.y(), corner.y()), qMax(high.z(), corner.z()));
        }
        const QVector3D cell_low = (low - m_grid_origin) / m_cell_size;
        const QVector3D cell_high = (high - m_grid_origin) / m_cell_size;
        from[0] = static_cast<int>(cell_low.x());
        from[1] = static_cast<int>(cell_low.y());
        from[2] = static_cast<int>(cell_low.z());
        to[0] = static_cast<int>(cell_high.x());
        to[1] = static_cast<int>(cell_high.y());
        to[2] = static_cast<int>(cell_high.z());
    };

    QVector<int> counts(m_cells_x * m_cells_y * m_cells_z + 1, 0);
    for (int t = 0; t < triangle_count; ++t)
    {
        int from[3];
        int to[3];
        cell_range(t, from, to);
        for (int z = from[2]; z <= to[2]; ++z)
        {
            for (int y = from[1]; y <= to[1]; ++y)
            {
                for (int x = from[0]; x <= to[0]; ++x)
                {
                    ++counts[cellIndex(x, y, z) + 1];
                }
            }
        }
    }

    m_cell_start = counts;
    for (int i = 1; i < m_cell_start.size(); ++i)
    {
        m_cell_start[i] += m_cell_start.at(i - 1);
    }

    m_cell_triangles.resize(m_cell_start.last());
    QVector<int> filled = m_cell_start;
    for (int t = 0; t < triangle_count; ++t)
    {
        int from[3];
        int to[3];
        cell_range(t, from, to);
        for (int z = from[2]; z <= to[2]; ++z)
        {
            for (int y = from[1]; y <= to[1]; ++y)
            {
                for (int x = from[0]; x <= to[0]; ++x)
                {
                    m_cell_triangles[filled[cellIndex(x, y, z)]++] = t;
                }
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyCollider::isEmpty() const
{
    return m_normals.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The triangles that may come within the radius of the point, each once.
QVector<int> BodyCollider::trianglesNear(const QVector3D& point, float radius) const
{
    QVector<int> near;
    if (!isEmpty())
    {
        const QVector3D reach(radius, radius, radius);
        const QVector3D low = (point - reach - m_grid_origin) / m_cell_size;
        const QVector3D high = (point + reach - m_grid_origin) / m_cell_size;
        const int from_x = qMax(0, static_cast<int>(qFloor(low.x())));
        const int from_y = qMax(0, static_cast<int>(qFloor(low.y())));
        const int from_z = qMax(0, static_cast<int>(qFloor(low.z())));
        const int to_x = qMin(m_cells_x - 1, static_cast<int>(qFloor(high.x())));
        const int to_y = qMin(m_cells_y - 1, static_cast<int>(qFloor(high.y())));
        const int to_z = qMin(m_cells_z - 1, static_cast<int>(qFloor(high.z())));

        for (int z = from_z; z <= to_z; ++z)
        {
            for (int y = from_y; y <= to_y; ++y)
            {
                for (int x = from_x; x <= to_x; ++x)
                {
                    const int cell = cellIndex(x, y, z);
                    for (int i = m_cell_start.at(cell); i < m_cell_start.at(cell + 1); ++i)
                    {
                        near.append(m_cell_triangles.at(i));
                    }
                }
            }
        }
        std::sort(near.begin(), near.end());
        near.erase(std::unique(near.begin(), near.end()), near.end());
    }
    return near;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The triangles that come within the radius of the point.
QVector<int> BodyCollider::trianglesWithin(const QVector3D& point, float radius) const
{
    QVector<int> within;
    const float radius_squared = radius * radius;
    for (const int t : trianglesNear(point, radius))
    {
        const QVector3D on_triangle = closestOnTriangle(point,
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t))),
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t + 1))),
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t + 2))));
        if ((point - on_triangle).lengthSquared() <= radius_squared)
        {
            within.append(t);
        }
    }
    return within;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Finds the point of the candidate triangles closest to the point. False if there are no candidates.
bool BodyCollider::closest(const QVector3D& point, const QVector<int>& candidates, BodyContact* contact) const
{
    int nearest = -1;
    QVector3D nearest_point;
    float nearest_squared = std::numeric_limits<float>::max();
    float nearest_alignment = 0;
    for (const int t : candidates)
    {
        const QVector3D on_triangle = closestOnTriangle(point,
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t))),
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t + 1))),
                                                        m_positions.at(static_cast<int>(m_triangles.at(3 * t + 2))));
        const QVector3D offset = point - on_triangle;
        const float squared = offset.lengthSquared();
        const float alignment = qAbs(QVector3D::dotProduct(offset.normalized(), m_normals.at(t)));
        if (squared < nearest_squared - same_distance
            || (squared <= nearest_squared + same_distance && alignment > nearest_alignment))
        {
            nearest = t;
            nearest_point = on_triangle;
            nearest_squared = squared;
            nearest_alignment = alignment;
        }
    }

    if (nearest >= 0)
    {
        const QVector3D offset = point - nearest_point;
        const float length = offset.length();
        const bool inside = QVector3D::dotProduct(offset, m_normals.at(nearest)) < 0;

        contact->point = nearest_point;
        contact->distance = inside ? -length : length;
        contact->normal = length > 1e-5f ? (inside ? -offset : offset) / length : m_normals.at(nearest);
    }
    return nearest >= 0;
}

//---------------------------------------------------------------------------------------------------------------------
int BodyCollider::cellIndex(int x, int y, int z) const
{
    return (z * m_cells_y + y) * m_cells_x + x;
}
