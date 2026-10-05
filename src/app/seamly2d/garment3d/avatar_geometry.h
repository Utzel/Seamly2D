//---------------------------------------------------------------------------------------------------------------------
//  @file   avatar_geometry.h
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

#ifndef AVATAR_GEOMETRY_H
#define AVATAR_GEOMETRY_H

#include <QVector3D>
#include <QVector>
#include <QtQuick3D/QQuick3DGeometry>

/// @brief Hands the avatar's skin to Qt Quick 3D, with smooth normals. Units are cm, y up, facing +z.
class AvatarGeometry : public QQuick3DGeometry
{
    Q_OBJECT

public:
    explicit           AvatarGeometry(QQuick3DObject* parent = nullptr);

    void               setBody(const QVector<QVector3D>& positions, const QVector<quint32>& triangles,
                               int skin_vertex_count);

private:
    Q_DISABLE_COPY(AvatarGeometry)
};

#endif // AVATAR_GEOMETRY_H
