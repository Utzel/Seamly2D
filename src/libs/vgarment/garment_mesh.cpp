//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_mesh.cpp
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

#include "garment_mesh.h"

#include <QPolygonF>

#include <utility>

//---------------------------------------------------------------------------------------------------------------------
bool GarmentMesh::isEmpty() const
{
    return indices.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
int GarmentMesh::vertexCount() const
{
    return static_cast<int>(rest_positions.size());
}

//---------------------------------------------------------------------------------------------------------------------
int GarmentMesh::triangleCount() const
{
    return static_cast<int>(indices.size() / 3);
}

//---------------------------------------------------------------------------------------------------------------------
QRectF GarmentMesh::bounds() const
{
    return QPolygonF(rest_positions).boundingRect();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Sum of the signed triangle areas in square centimetres.
qreal GarmentMesh::area() const
{
    qreal total = 0;
    for (int i = 0; i + 2 < indices.size(); i += 3)
    {
        const QPointF& a = rest_positions.at(static_cast<int>(indices.at(i)));
        const QPointF& b = rest_positions.at(static_cast<int>(indices.at(i + 1)));
        const QPointF& c = rest_positions.at(static_cast<int>(indices.at(i + 2)));
        total += ((b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y())) / 2.0;
    }
    return total;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seam line from one path point forward to another, as PieceOutline::stretch(), naming the mesh vertex of
/// each point. The offset is added to the vertex numbers, for meshes that are put together with others.
SeamStretch GarmentMesh::stretch(quint32 start_node, quint32 end_node, quint32 vertex_offset) const
{
    QVector<QPointF> seam_line;
    seam_line.reserve(boundary.size());
    for (const quint32 index : boundary)
    {
        seam_line.append(rest_positions.at(static_cast<int>(index)));
    }

    const SeamStretch along_seam_line = PieceOutline(seam_line, nodes).stretch(start_node, end_node);

    QVector<quint32> vertices;
    vertices.reserve(along_seam_line.vertices().size());
    for (const quint32 position : along_seam_line.vertices())
    {
        vertices.append(boundary.at(static_cast<int>(position)) + vertex_offset);
    }
    return SeamStretch(along_seam_line.points(), along_seam_line.notches(), vertices);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mirror image of the piece, as cut from the other side of folded fabric: flipped left to right, with
/// the same vertices, seam line and path points, its triangles still facing the same way.
GarmentMesh GarmentMesh::mirrored() const
{
    GarmentMesh mirror = *this;
    for (QPointF& position : mirror.rest_positions)
    {
        position.setX(-position.x());
    }
    for (int i = 0; i + 2 < mirror.indices.size(); i += 3)
    {
        std::swap(mirror.indices[i + 1], mirror.indices[i + 2]);
    }
    return mirror;
}
