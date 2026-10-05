//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_mesh.h
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

#ifndef GARMENT_MESH_H
#define GARMENT_MESH_H

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include "piece_outline.h"

/// @brief Flat triangle mesh of one pattern piece.
///
/// The positions are the piece as drafted: flat, unsewn and unstretched, in centimetres and in the
/// coordinates of the piece scene. The 3D view lays them out as they are, the drape simulation uses them
/// as the rest shape its stretch and shear are measured against.
struct GarmentMesh
{
    quint32              piece_id = 0;
    QVector<QPointF>     rest_positions; ///< vertex positions in cm
    QVector<quint32>     indices;        ///< triangles, three vertex indices each, all with positive signed area
    QVector<quint32>     boundary;       ///< seam line vertices, in the order the piece's path runs
    QVector<OutlineNode> nodes;          ///< the path points, each a vertex; index is its position in boundary

    bool                 isEmpty() const;
    int                  vertexCount() const;
    int                  triangleCount() const;
    QRectF               bounds() const;
    qreal                area() const;

    SeamStretch          stretch(quint32 start_node, quint32 end_node, quint32 vertex_offset = 0) const;
    GarmentMesh          mirrored() const;
};

#endif // GARMENT_MESH_H
