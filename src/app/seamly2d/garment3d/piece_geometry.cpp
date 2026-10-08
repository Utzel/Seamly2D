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
#include <QHash>
#include <QPointF>
#include <QSet>
#include <QVector3D>
#include <QtMath>

#include <limits>

#include "../vgarment/garment_mesh.h"

namespace
{
// Position, normal and texture coordinates, as floats, and the color when the vertices have one.
const int floats_per_vertex = 3 + 3 + 2 + 2;
const int floats_per_color = 4;

// The triangles' edges are drawn this far off the cloth's faces, in cm, so the faces don't hide them.
const float edge_lift = 0.02f;

//---------------------------------------------------------------------------------------------------------------------
// The color in linear RGB, as vertex colors are.
QVector3D linear(const QColor& color)
{
    return QVector3D(qPow(static_cast<float>(color.redF()), 2.2f), qPow(static_cast<float>(color.greenF()), 2.2f),
                     qPow(static_cast<float>(color.blueF()), 2.2f));
}

//---------------------------------------------------------------------------------------------------------------------
quint64 edgeKey(quint32 a, quint32 b)
{
    return (static_cast<quint64>(qMin(a, b)) << 32) | qMax(a, b);
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
/// @brief Replaces the geometry with the mesh, at the given positions in cm, or flat on the board without them, with a
/// color for each vertex if there is one for each. Texture coordinates are the flat piece's positions in cm, and where
/// it lies in the fabric, in cm across and along the grain, which runs at grain_angle degrees anticlockwise from the
/// piece's x axis, as the piece scene shows it. With a thickness, in cm, the cloth has a front face and a back face
/// that far apart, joined around its edge.
void PieceGeometry::setMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions,
                            const QVector<QColor>& colors, qreal grain_angle, qreal thickness)
{
    const QVector<QVector3D> placed = placedPositions(mesh, positions);
    const bool colored = colors.size() == mesh.vertexCount();
    const QVector<QVector3D> normals = vertexNormals(mesh, placed);
    const int count = mesh.vertexCount();
    const float half = static_cast<float>(qMax(thickness, 0.0) / 2.0);

    // The corners drawn: where each is, which way it faces, and which of the mesh's vertices it is of.
    QVector<QVector3D> drawn;
    QVector<QVector3D> facing;
    QVector<int> of_vertex;
    auto add = [&drawn, &facing, &of_vertex](const QVector3D& position, const QVector3D& normal, int vertex)
    {
        drawn.append(position);
        facing.append(normal);
        of_vertex.append(vertex);
        return static_cast<quint32>(drawn.size() - 1);
    };

    // The front face. The piece scene's y axis points down, the 3D scene's up, which turns every triangle around;
    // swapping two corners turns them back to face outwards.
    for (int i = 0; i < count; ++i)
    {
        add(placed.at(i) + normals.at(i) * half, normals.at(i), i);
    }
    QVector<quint32> corners;
    corners.reserve(mesh.indices.size());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        corners << mesh.indices.at(i) << mesh.indices.at(i + 2) << mesh.indices.at(i + 1);
    }

    if (half > 0)
    {
        // The back face, turned over.
        for (int i = 0; i < count; ++i)
        {
            add(placed.at(i) - normals.at(i) * half, -normals.at(i), i);
        }
        const int front_corners = static_cast<int>(corners.size());
        for (int i = 0; i + 2 < front_corners; i += 3)
        {
            corners << corners.at(i) + static_cast<quint32>(count) << corners.at(i + 2) + static_cast<quint32>(count)
                    << corners.at(i + 1) + static_cast<quint32>(count);
        }

        // Around the edge, a strip from the front face to the back, facing away from the triangle inside it.
        QHash<quint64, quint32> inside;
        for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            for (int k = 0; k < 3; ++k)
            {
                inside.insert(edgeKey(mesh.indices.at(i + k), mesh.indices.at(i + (k + 1) % 3)),
                              mesh.indices.at(i + (k + 2) % 3));
            }
        }
        const int edge_count = static_cast<int>(mesh.boundary.size());
        for (int k = 0; k < edge_count; ++k)
        {
            const quint32 a = mesh.boundary.at(k);
            const quint32 b = mesh.boundary.at((k + 1) % edge_count);
            if (!inside.contains(edgeKey(a, b)))
            {
                continue;
            }
            const QVector3D& at_a = placed.at(static_cast<int>(a));
            const QVector3D& at_b = placed.at(static_cast<int>(b));
            const QVector3D along = (at_b - at_a).normalized();
            const QVector3D normal = (normals.at(static_cast<int>(a)) + normals.at(static_cast<int>(b))).normalized();
            QVector3D out = (at_a + at_b) / 2.0f - placed.at(static_cast<int>(inside.value(edgeKey(a, b))));
            out -= along * QVector3D::dotProduct(out, along) + normal * QVector3D::dotProduct(out, normal);
            if (out.isNull())
            {
                continue;
            }
            out.normalize();
            const quint32 front_a = add(at_a + normals.at(static_cast<int>(a)) * half, out, static_cast<int>(a));
            const quint32 front_b = add(at_b + normals.at(static_cast<int>(b)) * half, out, static_cast<int>(b));
            const quint32 back_a = add(at_a - normals.at(static_cast<int>(a)) * half, out, static_cast<int>(a));
            const quint32 back_b = add(at_b - normals.at(static_cast<int>(b)) * half, out, static_cast<int>(b));
            const QVector3D& corner = drawn.at(static_cast<int>(front_a));
            const QVector3D turn = QVector3D::crossProduct(drawn.at(static_cast<int>(back_a)) - corner,
                                                           drawn.at(static_cast<int>(back_b)) - corner);
            if (QVector3D::dotProduct(turn, out) >= 0)
            {
                corners << front_a << back_a << back_b << front_a << back_b << front_b;
            }
            else
            {
                corners << front_a << back_b << back_a << front_a << front_b << back_b;
            }
        }
    }

    const QVector<QPointF> in_fabric = grainPositions(mesh, grain_angle);
    const int vertex_floats = floats_per_vertex + (colored ? floats_per_color : 0);
    const int vertex_bytes = vertex_floats * static_cast<int>(sizeof(float));
    QByteArray vertex_data(static_cast<int>(drawn.size()) * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    for (int corner = 0; corner < drawn.size(); ++corner)
    {
        const int i = of_vertex.at(corner);
        const QVector3D& position = drawn.at(corner);
        const QVector3D& normal = facing.at(corner);
        const QPointF& rest = mesh.rest_positions.at(i);
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
        *vertex++ = normal.x();
        *vertex++ = normal.y();
        *vertex++ = normal.z();
        *vertex++ = static_cast<float>(rest.x());
        *vertex++ = static_cast<float>(rest.y());
        *vertex++ = static_cast<float>(in_fabric.at(i).x());
        *vertex++ = static_cast<float>(in_fabric.at(i).y());
        if (colored)
        {
            const QVector3D color = linear(colors.at(i));
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
    bounds(drawn, &minimum, &maximum);

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, 3 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::TexCoord0Semantic, 6 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::TexCoord1Semantic, 8 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    if (colored)
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
/// @brief Where each of the mesh's vertices lies in the fabric, in cm across the grain and along it, towards where the
/// grainline points; the grain runs at grain_angle degrees anticlockwise from the piece's x axis, as the piece scene
/// shows it.
QVector<QPointF> PieceGeometry::grainPositions(const GarmentMesh& mesh, qreal grain_angle)
{
    // Along the grain (the warp) and across it (the weft), in the piece scene's coordinates, whose y points down.
    const qreal grain = qDegreesToRadians(grain_angle);
    const QPointF warp(qCos(grain), -qSin(grain));
    const QPointF weft(qSin(grain), qCos(grain));

    QVector<QPointF> positions;
    positions.reserve(mesh.rest_positions.size());
    for (const QPointF& rest : mesh.rest_positions)
    {
        positions.append(QPointF(QPointF::dotProduct(rest, weft), QPointF::dotProduct(rest, warp)));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with lines along the edges of the mesh's triangles, at the given positions in cm, or
/// flat on the board without them, on the front face and the back face of cloth as thick as given, in cm. An empty mesh
/// leaves nothing to draw.
void PieceGeometry::setEdges(const GarmentMesh& mesh, const QVector<QVector3D>& positions, qreal thickness)
{
    clear();
    if (mesh.vertexCount() == 0)
    {
        update();
        return;
    }

    const QVector<QVector3D> placed = placedPositions(mesh, positions);
    const QVector<QVector3D> normals = vertexNormals(mesh, placed);
    const float off = static_cast<float>(qMax(thickness, 0.0) / 2.0) + edge_lift;
    const int count = static_cast<int>(placed.size());

    // Each vertex on the front face, then on the back face.
    QVector<QVector3D> drawn;
    drawn.reserve(2 * count);
    for (int face = 0; face < 2; ++face)
    {
        const float side = face == 0 ? off : -off;
        for (int i = 0; i < count; ++i)
        {
            drawn.append(placed.at(i) + normals.at(i) * side);
        }
    }

    QSet<quint64> edges;
    QVector<quint32> ends;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const quint32 a = mesh.indices.at(i + k);
            const quint32 b = mesh.indices.at(i + (k + 1) % 3);
            if (!edges.contains(edgeKey(a, b)))
            {
                edges.insert(edgeKey(a, b));
                ends << a << b << a + static_cast<quint32>(count) << b + static_cast<quint32>(count);
            }
        }
    }

    const int vertex_bytes = 3 * static_cast<int>(sizeof(float));
    QByteArray vertex_data(static_cast<int>(drawn.size()) * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    for (const QVector3D& position : drawn)
    {
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
    }
    const QByteArray index_data(reinterpret_cast<const char*>(ends.constData()),
                                static_cast<int>(ends.size() * static_cast<int>(sizeof(quint32))));

    QVector3D minimum;
    QVector3D maximum;
    bounds(drawn, &minimum, &maximum);

    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Lines);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
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
/// board, so overlapping pieces of the same color can still be told apart. On cloth with a thickness, in cm, it lies
/// on the front face.
void PieceGeometry::setOutline(const GarmentMesh& mesh, const QVector<QVector3D>& positions, qreal thickness)
{
    QVector<QVector3D> placed = placedPositions(mesh, positions);
    if (thickness > 0)
    {
        const QVector<QVector3D> normals = vertexNormals(mesh, placed);
        for (int i = 0; i < placed.size(); ++i)
        {
            placed[i] += normals.at(i) * static_cast<float>(thickness / 2.0);
        }
    }
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
