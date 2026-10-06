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
#include <QColor>
#include <QPointF>
#include <QVector3D>
#include <QtMath>

#include <limits>

#include "../vgarment/garment_mesh.h"

namespace
{
// Position, normal and texture coordinate, as floats, and the color when the strain is shown.
const int floats_per_vertex = 3 + 3 + 2 + 2;
const int floats_per_color = 4;

// Strain is shown green where there is none, yellow at half the full strain and red from the full strain on.
const qreal full_strain = 0.1;

//---------------------------------------------------------------------------------------------------------------------
QVector3D colorVector(const QColor& color)
{
    return QVector3D(color.redF(), color.greenF(), color.blueF());
}

//---------------------------------------------------------------------------------------------------------------------
// The color that shows the strain, in linear RGB as vertex colors are.
QVector3D strainColor(qreal strain)
{
    const QVector<QColor> colors = PieceGeometry::strainColors();
    const QVector3D none = colorVector(colors.at(0));
    const QVector3D half = colorVector(colors.at(1));
    const QVector3D full = colorVector(colors.at(2));
    const float share = static_cast<float>(qBound(0.0, strain / full_strain, 1.0));
    const QVector3D color = share < 0.5f ? none + (half - none) * (share * 2.0f)
                                         : half + (full - half) * (share * 2.0f - 1.0f);
    return QVector3D(qPow(color.x(), 2.2f), qPow(color.y(), 2.2f), qPow(color.z(), 2.2f));
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
/// repeat size. With the strain shown, each vertex gets the color of how much the cloth around it is stretched.
/// @brief The piece's mesh, where it is put or else lying flat, optionally colored by its strain. The grain runs at
/// grain_angle degrees anticlockwise from the piece's x axis, as the piece scene shows it.
void PieceGeometry::setMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions, bool strain_shown,
                            qreal grain_angle)
{
    const QVector<QVector3D> placed = placedPositions(mesh, positions);
    const QVector<qreal> strain = strain_shown ? mesh.strain(placed) : QVector<qreal>();
    const QVector<QVector3D> normals = vertexNormals(mesh, placed);

    // The piece scene's y axis points down, the 3D scene's up, which turns every triangle around; swapping two
    // corners turns them back to face outwards.
    QVector<quint32> corners;
    corners.reserve(mesh.indices.size());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        corners << mesh.indices.at(i) << mesh.indices.at(i + 2) << mesh.indices.at(i + 1);
    }

    // Along the grain (the warp) and across it (the weft), in the piece scene's coordinates, whose y points down.
    const qreal grain = qDegreesToRadians(grain_angle);
    const QPointF warp(qCos(grain), -qSin(grain));
    const QPointF weft(qSin(grain), qCos(grain));

    const int vertex_floats = floats_per_vertex + (strain_shown ? floats_per_color : 0);
    const int vertex_bytes = vertex_floats * static_cast<int>(sizeof(float));
    QByteArray vertex_data(mesh.vertexCount() * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        const QVector3D& position = placed.at(i);
        const QVector3D& normal = normals.at(i);
        const QPointF& rest = mesh.rest_positions.at(i);
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
        *vertex++ = normal.x();
        *vertex++ = normal.y();
        *vertex++ = normal.z();
        *vertex++ = static_cast<float>(rest.x());
        *vertex++ = static_cast<float>(rest.y());
        *vertex++ = static_cast<float>(QPointF::dotProduct(rest, weft));
        *vertex++ = static_cast<float>(QPointF::dotProduct(rest, warp));
        if (strain_shown)
        {
            const QVector3D color = strainColor(strain.at(i));
            *vertex++ = color.x();
            *vertex++ = color.y();
            *vertex++ = color.z();
            *vertex++ = 1.0f;
        }
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
    addAttribute(Attribute::TexCoord1Semantic, 8 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    if (strain_shown)
    {
        addAttribute(Attribute::ColorSemantic, floats_per_vertex * static_cast<int>(sizeof(float)),
                     Attribute::F32Type);
    }
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The strain shown in full red, as a share of the drafted size: cloth 10% longer than drafted.
qreal PieceGeometry::fullStrain()
{
    return full_strain;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The colors the strain is shown in: for none, for half the full strain and for the full strain.
QVector<QColor> PieceGeometry::strainColors()
{
    return {QColor(61, 178, 74), QColor(255, 212, 0), QColor(230, 26, 26)};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The given positions, or with none the piece lying flat on the board, facing the camera.
QVector<QVector3D> PieceGeometry::placedPositions(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
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
/// @brief The mesh's normals of unit length at the given positions, smoothed over the triangles around each vertex,
/// on the side the piece faces in the 3D scene: towards the camera on the board.
QVector<QVector3D> PieceGeometry::vertexNormals(const GarmentMesh& mesh, const QVector<QVector3D>& placed)
{
    // The piece scene's y axis points down, the 3D scene's up, which turns every triangle around; the normals follow
    // the triangles turned back.
    QVector<QVector3D> normals(placed.size());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const int a = static_cast<int>(mesh.indices.at(i));
        const int b = static_cast<int>(mesh.indices.at(i + 2));
        const int c = static_cast<int>(mesh.indices.at(i + 1));
        if (a < placed.size() && b < placed.size() && c < placed.size())
        {
            const QVector3D face = QVector3D::crossProduct(placed.at(b) - placed.at(a), placed.at(c) - placed.at(a));
            normals[a] += face;
            normals[b] += face;
            normals[c] += face;
        }
    }
    for (QVector3D& normal : normals)
    {
        normal = normal.isNull() ? QVector3D(0, 0, 1) : normal.normalized();
    }
    return normals;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the mesh's seam line, as line segments, at the given positions or flat on the
/// board, so overlapping pieces of the same color can still be told apart.
void PieceGeometry::setOutline(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    const QVector<QVector3D> placed = placedPositions(mesh, positions);
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
