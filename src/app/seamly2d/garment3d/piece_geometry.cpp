//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_geometry.cpp
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

#include "piece_geometry.h"

#include <QByteArray>
#include <QVector3D>

#include <limits>

#include "../vgarment/garment_mesh.h"

namespace
{
// Position, normal and texture coordinate, as floats.
const int floats_per_vertex = 3 + 3 + 2;
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
PieceGeometry::PieceGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the mesh. Texture coordinates are the flat piece's positions in cm, so a fabric
/// texture can later be scaled to its real repeat size.
void PieceGeometry::setMesh(const GarmentMesh& mesh)
{
    const int vertex_bytes = floats_per_vertex * static_cast<int>(sizeof(float));

    QByteArray vertex_data(mesh.vertexCount() * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, 0.0f);
    QVector3D maximum(-largest, -largest, 0.0f);

    for (const QPointF& position : mesh.rest_positions)
    {
        const float x = static_cast<float>(position.x());
        const float y = static_cast<float>(-position.y());

        *vertex++ = x;
        *vertex++ = y;
        *vertex++ = 0.0f;
        *vertex++ = 0.0f;
        *vertex++ = 0.0f;
        *vertex++ = 1.0f;
        *vertex++ = static_cast<float>(position.x());
        *vertex++ = static_cast<float>(position.y());

        minimum.setX(qMin(minimum.x(), x));
        minimum.setY(qMin(minimum.y(), y));
        maximum.setX(qMax(maximum.x(), x));
        maximum.setY(qMax(maximum.y(), y));
    }

    // Flipping y turns every triangle around; swapping two corners turns them back to face the camera.
    QByteArray index_data(mesh.indices.size() * static_cast<int>(sizeof(quint32)), Qt::Uninitialized);
    quint32* index = reinterpret_cast<quint32*>(index_data.data());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        *index++ = mesh.indices.at(i);
        *index++ = mesh.indices.at(i + 2);
        *index++ = mesh.indices.at(i + 1);
    }

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, 3 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::TexCoord0Semantic, 6 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the mesh's seam line, as line segments, so overlapping pieces of the same color
/// can still be told apart.
void PieceGeometry::setOutline(const GarmentMesh& mesh)
{
    const int vertex_bytes = 3 * static_cast<int>(sizeof(float));
    const int point_count = static_cast<int>(mesh.boundary.size());

    QByteArray vertex_data(point_count * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, 0.0f);
    QVector3D maximum(-largest, -largest, 0.0f);

    for (const quint32 index : mesh.boundary)
    {
        const QPointF& position = mesh.rest_positions.at(static_cast<int>(index));
        const float x = static_cast<float>(position.x());
        const float y = static_cast<float>(-position.y());

        *vertex++ = x;
        *vertex++ = y;
        *vertex++ = 0.0f;

        minimum.setX(qMin(minimum.x(), x));
        minimum.setY(qMin(minimum.y(), y));
        maximum.setX(qMax(maximum.x(), x));
        maximum.setY(qMax(maximum.y(), y));
    }

    QByteArray index_data(point_count * 2 * static_cast<int>(sizeof(quint32)), Qt::Uninitialized);
    quint32* index = reinterpret_cast<quint32*>(index_data.data());
    for (int i = 0; i < point_count; ++i)
    {
        *index++ = static_cast<quint32>(i);
        *index++ = static_cast<quint32>((i + 1) % point_count);
    }

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Lines);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
}
