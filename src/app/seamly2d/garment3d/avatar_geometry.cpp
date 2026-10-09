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

#include <QtMath>

#include <limits>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// The color in linear RGB, as vertex colors are.
QVector3D linear(const QColor& color)
{
    return QVector3D(qPow(static_cast<float>(color.redF()), 2.2f), qPow(static_cast<float>(color.greenF()), 2.2f),
                     qPow(static_cast<float>(color.blueF()), 2.2f));
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
AvatarGeometry::AvatarGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
    , m_positions()
    , m_normals()
    , m_index_data()
    , m_colors()
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the first skin_vertex_count positions and the triangles between them. Each
/// vertex normal is the area weighted average of the normals of the triangles around it. The skin is plain again.
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
    for (QVector3D& normal : normals)
    {
        normal.normalize();
    }

    m_positions = positions.mid(0, skin_vertex_count);
    m_normals = normals;
    m_index_data = QByteArray(reinterpret_cast<const char*>(triangles.constData()),
                              static_cast<int>(triangles.size() * static_cast<int>(sizeof(quint32))));
    m_colors.clear();
    build();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Colors the skin vertex by vertex, as a fit map does, or plain again with no colors. Colors for another number
/// of vertices are left out.
void AvatarGeometry::setColors(const QVector<QColor>& colors)
{
    if ((colors.isEmpty() && m_colors.isEmpty()) || (!colors.isEmpty() && colors.size() != m_positions.size()))
    {
        return;
    }
    m_colors = colors;
    build();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How many vertices the skin has.
int AvatarGeometry::vertexCount() const
{
    return static_cast<int>(m_positions.size());
}

//---------------------------------------------------------------------------------------------------------------------
// Hands the skin to Qt Quick 3D: each vertex's position and normal, and its color if it is colored.
void AvatarGeometry::build()
{
    const bool colored = !m_colors.isEmpty();
    const int floats_per_vertex = colored ? 10 : 6;
    const int vertex_bytes = floats_per_vertex * static_cast<int>(sizeof(float));
    QByteArray vertex_data(static_cast<int>(m_positions.size()) * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum(-largest, -largest, -largest);
    for (int i = 0; i < m_positions.size(); ++i)
    {
        const QVector3D& position = m_positions.at(i);
        const QVector3D& normal = m_normals.at(i);
        *vertex++ = position.x();
        *vertex++ = position.y();
        *vertex++ = position.z();
        *vertex++ = normal.x();
        *vertex++ = normal.y();
        *vertex++ = normal.z();
        if (colored)
        {
            const QVector3D color = linear(m_colors.at(i));
            *vertex++ = color.x();
            *vertex++ = color.y();
            *vertex++ = color.z();
            *vertex++ = 1.0f;
        }

        minimum = QVector3D(qMin(minimum.x(), position.x()), qMin(minimum.y(), position.y()),
                            qMin(minimum.z(), position.z()));
        maximum = QVector3D(qMax(maximum.x(), position.x()), qMax(maximum.y(), position.y()),
                            qMax(maximum.z(), position.z()));
    }

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(PrimitiveType::Triangles);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::NormalSemantic, 3 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    if (colored)
    {
        addAttribute(Attribute::ColorSemantic, 6 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    }
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(m_index_data);
    setBounds(minimum, maximum);
    update();
}
