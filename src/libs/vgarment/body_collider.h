//---------------------------------------------------------------------------------------------------------------------
//  @file   body_collider.h
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

#ifndef BODY_COLLIDER_H
#define BODY_COLLIDER_H

#include <QVector3D>
#include <QVector>
#include <QtGlobal>

/// @brief Where a point is closest to the body's surface.
struct BodyContact
{
    QVector3D point;         ///< the closest point on the surface
    QVector3D normal;        ///< pointing out of the body
    float     distance = 0;  ///< from the surface, negative inside the body
};

/// @brief A body the cloth can't go through: a still triangle mesh facing outwards, as the avatar's skin.
///
/// The triangles are sorted into a grid of cubes, so finding the ones near a point only looks at a few.
class BodyCollider
{
public:
                       BodyCollider() = default;
                       BodyCollider(const QVector<QVector3D>& positions, const QVector<quint32>& triangles);

    bool               isEmpty() const;

    QVector<int>       trianglesNear(const QVector3D& point, float radius) const;
    QVector<int>       trianglesWithin(const QVector3D& point, float radius) const;
    bool               closest(const QVector3D& point, const QVector<int>& candidates, BodyContact* contact) const;

private:
    QVector<QVector3D> m_positions;
    QVector<quint32>   m_triangles;
    QVector<QVector3D> m_normals;
    QVector3D          m_grid_origin;
    float              m_cell_size = 1.0f;
    int                m_cells_x = 0;
    int                m_cells_y = 0;
    int                m_cells_z = 0;
    QVector<int>       m_cell_start;
    QVector<int>       m_cell_triangles;

    int                cellIndex(int x, int y, int z) const;
};

#endif // BODY_COLLIDER_H
