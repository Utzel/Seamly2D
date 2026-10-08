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

#include <QColor>
#include <QPointF>
#include <QVector3D>
#include <QVector>
#include <QtQuick3D/QQuick3DGeometry>

struct GarmentMesh;

/// @brief Hands one piece's mesh, or its seam line, to Qt Quick 3D.
///
/// Without positions the piece lies flat in the z = 0 plane, facing the camera; with them it is where they put it,
/// on the avatar or draped. Units stay cm. The piece scene's y axis points down and the 3D scene's points up, so y
/// is flipped on the way. The mesh can come with a color for each vertex, as a fit map colors it.
///
/// Each vertex has two sets of texture coordinates in cm: where it is in the flat piece, and where it is in the
/// fabric the piece is cut from, across the grain and along it, so a fabric's pattern runs along the grainline.
///
/// Cloth with a thickness is drawn as a front face and a back face that far apart, joined around its edge, so its
/// edges show how thick it is and each face is lit as it faces.
class PieceGeometry : public QQuick3DGeometry
{
    Q_OBJECT

public:
    explicit           PieceGeometry(QQuick3DObject* parent = nullptr);

    void               setMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions = QVector<QVector3D>(),
                               const QVector<QColor>& colors = QVector<QColor>(), qreal grain_angle = 90.0,
                               qreal thickness = 0.0);
    void               setEdges(const GarmentMesh& mesh, const QVector<QVector3D>& positions = QVector<QVector3D>(),
                                qreal thickness = 0.0);
    void               setOutline(const GarmentMesh& mesh,
                                  const QVector<QVector3D>& positions = QVector<QVector3D>(), qreal thickness = 0.0);

    static QVector<QVector3D> placedPositions(const GarmentMesh& mesh, const QVector<QVector3D>& positions);
    static QVector<QVector3D> vertexNormals(const GarmentMesh& mesh, const QVector<QVector3D>& placed);
    static QVector<QPointF>   grainPositions(const GarmentMesh& mesh, qreal grain_angle);

private:
    Q_DISABLE_COPY(PieceGeometry)
};

#endif // PIECE_GEOMETRY_H
