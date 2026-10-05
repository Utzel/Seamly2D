//---------------------------------------------------------------------------------------------------------------------
//  @file   avatar_geometry.cpp
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

#include "avatar_geometry.h"

#include <QByteArray>

#include <limits>

//---------------------------------------------------------------------------------------------------------------------
AvatarGeometry::AvatarGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the first skin_vertex_count positions and the triangles between them. Each
/// vertex normal is the area weighted average of the normals of the triangles around it.
void AvatarGeometry::setBody(const QVector<QVector3D>& positions, const QVector<quint32>& triangles,
                             int skin_vertex_count)
{
    QVector<QVector3D> normals(skin_vertex_count);
    for (int i = 0; i + 2 < triangles.size(); i += 3)
    {
        const int a = static_cast<int>(triangles.at(i));
        const int b = static_cast<int>(triangles.at(i + 1));
        const int c = static_cast<int>(triangles.at(i + 2));
        // The cross product's length is twice the triangle's area, which gives the weighting.
        const QVector3D normal = QVector3D::crossProduct(positions.at(b) - positions.at(a),
                                                         positions.at(c) - positions.at(a));
        normals[a] += normal;
        normals[b] += normal;
        normals[c] += normal;
    }

    const int vertex_bytes = 6 * static_cast<int>(sizeof(float));
    QByteArray vertex_data(skin_vertex_count * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum(-largest, -largest, -largest);
    for (int i = 0; i < skin_vertex_count; ++i)
    {
        const QVector3D& position = positions.at(i);
        const QVector3D normal = normals.at(i).normalized();
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
        *vertex++ = normal.x();
        *vertex++ = normal.y();
        *vertex++ = normal.z();

        minimum = QVector3D(qMin(minimum.x(), position.x()), qMin(minimum.y(), position.y()),
                            qMin(minimum.z(), position.z()));
        maximum = QVector3D(qMax(maximum.x(), position.x()), qMax(maximum.y(), position.y()),
                            qMax(maximum.z(), position.z()));
    }

    QByteArray index_data(reinterpret_cast<const char*>(triangles.constData()),
                          static_cast<int>(triangles.size() * static_cast<int>(sizeof(quint32))));

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, 3 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
}
