//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_geometry.h
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

#ifndef PIECE_GEOMETRY_H
#define PIECE_GEOMETRY_H

#include <QtQuick3D/QQuick3DGeometry>

struct GarmentMesh;

/// @brief Hands one piece's mesh, or its seam line, to Qt Quick 3D.
///
/// The piece lies flat in the z = 0 plane, facing the camera. Units stay cm. The piece scene's y axis points down
/// and the 3D scene's points up, so y is flipped on the way.
class PieceGeometry : public QQuick3DGeometry
{
    Q_OBJECT

public:
    explicit           PieceGeometry(QQuick3DObject* parent = nullptr);

    void               setMesh(const GarmentMesh& mesh);
    void               setOutline(const GarmentMesh& mesh);

private:
    Q_DISABLE_COPY(PieceGeometry)
};

#endif // PIECE_GEOMETRY_H
