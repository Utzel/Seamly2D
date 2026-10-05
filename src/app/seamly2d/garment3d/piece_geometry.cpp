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

//---------------------------------------------------------------------------------------------------------------------
// The given positions, or with none the piece lying flat on the board, facing the camera.
QVector<QVector3D> positionsOrFlat(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    if (positions.size() == mesh.vertexCount())
    {
        return positions;
    }

    QVector<QVector3D> flat;
    flat.reserve(mesh.vertexCount());
    for (const QPointF& point : mesh.rest_positions)
    {
        flat.append(QVector3D(static_cast<float>(point.x()), static_cast<float>(-point.y()), 0.0f));
    }
    return flat;
}

//---------------------------------------------------------------------------------------------------------------------
void bounds(const QVector<QVector3D>& positions, QVector3D* minimum, QVector3D* maximum)
{
    const float largest = std::numeric_limits<float>::max();
    *minimum = QVector3D(largest, largest, largest);
    *maximum = -*minimum;
    for (const QVector3D& position : positions)
    {
        *minimum = QVector3D(qMin(minimum->x(), position.x()), qMin(minimum->y(), position.y()),
                             qMin(minimum->z(), position.z()));
        *maximum = QVector3D(qMax(maximum->x(), position.x()), qMax(maximum->y(), position.y()),
                             qMax(maximum->z(), position.z()));
    }
    if (positions.isEmpty())
    {
        *minimum = QVector3D();
        *maximum = QVector3D();
    }
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
PieceGeometry::PieceGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the mesh, at the given positions in cm, or flat on the board without them.
/// Texture coordinates are the flat piece's positions in cm, so a fabric texture can later be scaled to its real
/// repeat size.
void PieceGeometry::setMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    const QVector<QVector3D> placed = positionsOrFlat(mesh, positions);

    // The piece scene's y axis points down, the 3D scene's up, which turns every triangle around; swapping two
    // corners turns them back to face outwards. The normals follow that turned-back order.
    QVector<quint32> corners;
    corners.reserve(mesh.indices.size());
    QVector<QVector3D> normals(placed.size());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const quint32 a = mesh.indices.at(i);
        const quint32 b = mesh.indices.at(i + 2);
        const quint32 c = mesh.indices.at(i + 1);
        corners << a << b << c;

        const QVector3D& pa = placed.at(static_cast<int>(a));
        const QVector3D face = QVector3D::crossProduct(placed.at(static_cast<int>(b)) - pa,
                                                       placed.at(static_cast<int>(c)) - pa);
        normals[static_cast<int>(a)] += face;
        normals[static_cast<int>(b)] += face;
        normals[static_cast<int>(c)] += face;
    }

    const int vertex_bytes = floats_per_vertex * static_cast<int>(sizeof(float));
    QByteArray vertex_data(mesh.vertexCount() * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        const QVector3D& position = placed.at(i);
        const QVector3D normal = normals.at(i).isNull() ? QVector3D(0, 0, 1) : normals.at(i).normalized();
        const QPointF& rest = mesh.rest_positions.at(i);
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
        *vertex++ = normal.x();
        *vertex++ = normal.y();
        *vertex++ = normal.z();
        *vertex++ = static_cast<float>(rest.x());
        *vertex++ = static_cast<float>(rest.y());
    }

    const QByteArray index_data(reinterpret_cast<const char*>(corners.constData()),
                                static_cast<int>(corners.size() * static_cast<int>(sizeof(quint32))));

    QVector3D minimum;
    QVector3D maximum;
    bounds(placed, &minimum, &maximum);

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
/// @brief Replaces the geometry with the mesh's seam line, as line segments, at the given positions or flat on the
/// board, so overlapping pieces of the same color can still be told apart.
void PieceGeometry::setOutline(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    const QVector<QVector3D> placed = positionsOrFlat(mesh, positions);
    const int vertex_bytes = 3 * static_cast<int>(sizeof(float));
    const int point_count = static_cast<int>(mesh.boundary.size());

    QVector<QVector3D> seam_line;
    seam_line.reserve(point_count);
    QByteArray vertex_data(point_count * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    for (const quint32 index : mesh.boundary)
    {
        const QVector3D& position = placed.at(static_cast<int>(index));
        seam_line.append(position);
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
    }

    QByteArray index_data(point_count * 2 * static_cast<int>(sizeof(quint32)), Qt::Uninitialized);
    quint32* index = reinterpret_cast<quint32*>(index_data.data());
    for (int i = 0; i < point_count; ++i)
    {
        *index++ = static_cast<quint32>(i);
        *index++ = static_cast<quint32>((i + 1) % point_count);
    }

    QVector3D minimum;
    QVector3D maximum;
    bounds(seam_line, &minimum, &maximum);

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
