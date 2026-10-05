//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_geometry.cpp
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

#include "seam_geometry.h"

#include <QByteArray>
#include <QLineF>
#include <QVector3D>
#include <QtMath>

#include <limits>

namespace
{
// Position and color, as floats.
const int floats_per_vertex = 3 + 4;

// Where a band turns sharply its corners would shoot far out; they get no further out than this many half widths.
const qreal longest_miter = 3.0;

//---------------------------------------------------------------------------------------------------------------------
QPointF unit(const QPointF& vector)
{
    const qreal length = qSqrt(QPointF::dotProduct(vector, vector));
    return length > 0 ? vector / length : QPointF();
}

//---------------------------------------------------------------------------------------------------------------------
void appendVertex(QVector<float>& vertices, const QPointF& position, const QColor& color)
{
    vertices << static_cast<float>(position.x()) << static_cast<float>(position.y()) << 0.0f
             << static_cast<float>(color.redF()) << static_cast<float>(color.greenF())
             << static_cast<float>(color.blueF()) << static_cast<float>(color.alphaF());
}

//---------------------------------------------------------------------------------------------------------------------
// The band's points without repeats, which have no direction.
QVector<QPointF> distinctPoints(const QVector<QPointF>& points)
{
    QVector<QPointF> distinct;
    for (const QPointF& point : points)
    {
        if (distinct.isEmpty() || QLineF(distinct.last(), point).length() > 1e-6)
        {
            distinct.append(point);
        }
    }
    return distinct;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
SeamGeometry::SeamGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with bands of the given widths, in cm, centred on their points.
void SeamGeometry::setBands(const QVector<Band>& bands)
{
    QVector<float> vertices;
    QVector<quint32> indices;
    for (const Band& band : bands)
    {
        const QVector<QPointF> points = distinctPoints(band.points);
        const int count = static_cast<int>(points.size());
        if (count >= 2)
        {
            const quint32 first = static_cast<quint32>(vertices.size() / floats_per_vertex);
            const qreal half_width = band.width / 2.0;
            for (int i = 0; i < count; ++i)
            {
                const QPointF incoming = i > 0 ? unit(points.at(i) - points.at(i - 1)) : QPointF();
                const QPointF outgoing = i + 1 < count ? unit(points.at(i + 1) - points.at(i)) : QPointF();
                const QPointF tangent = unit(incoming + outgoing).isNull() ? (i > 0 ? incoming : outgoing)
                                                                           : unit(incoming + outgoing);
                const QPointF normal(-tangent.y(), tangent.x());

                // Corners are pushed out so the band keeps its width along both sides of the turn.
                const QPointF along = i > 0 ? incoming : outgoing;
                const QPointF side(-along.y(), along.x());
                const qreal miter = qMin(1.0 / qMax(QPointF::dotProduct(normal, side), 1e-6), longest_miter);

                appendVertex(vertices, points.at(i) + normal * half_width * miter, band.color);
                appendVertex(vertices, points.at(i) - normal * half_width * miter, band.color);
            }

            for (int i = 0; i + 1 < count; ++i)
            {
                const quint32 left = first + static_cast<quint32>(2 * i);
                indices << left << left + 1 << left + 2 << left + 1 << left + 3 << left + 2;
            }
        }
    }
    setVertices(vertices, indices, PrimitiveType::Triangles);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with thin lines.
void SeamGeometry::setLines(const QVector<Line>& lines)
{
    QVector<float> vertices;
    QVector<quint32> indices;
    for (const Line& line : lines)
    {
        const quint32 from = static_cast<quint32>(vertices.size() / floats_per_vertex);
        indices << from << from + 1;
        appendVertex(vertices, line.from, line.color);
        appendVertex(vertices, line.to, line.color);
    }
    setVertices(vertices, indices, PrimitiveType::Lines);
}

//---------------------------------------------------------------------------------------------------------------------
void SeamGeometry::setVertices(const QVector<float>& vertices, const QVector<quint32>& indices,
                               PrimitiveType primitive_type)
{
    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, 0.0f);
    QVector3D maximum(-largest, -largest, 0.0f);
    for (int i = 0; i + 1 < vertices.size(); i += floats_per_vertex)
    {
        minimum.setX(qMin(minimum.x(), vertices.at(i)));
        minimum.setY(qMin(minimum.y(), vertices.at(i + 1)));
        maximum.setX(qMax(maximum.x(), vertices.at(i)));
        maximum.setY(qMax(maximum.y(), vertices.at(i + 1)));
    }
    if (vertices.isEmpty())
    {
        minimum = QVector3D();
        maximum = QVector3D();
    }

    const int vertex_bytes = floats_per_vertex * static_cast<int>(sizeof(float));
    const QByteArray vertex_data(reinterpret_cast<const char*>(vertices.constData()),
                                 static_cast<int>(vertices.size() * static_cast<int>(sizeof(float))));
    const QByteArray index_data(reinterpret_cast<const char*>(indices.constData()),
                                static_cast<int>(indices.size() * static_cast<int>(sizeof(quint32))));

    clear();
    setStride(vertex_bytes);
    setPrimitiveType(primitive_type);
    addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
    addAttribute(Attribute::ColorSemantic, 3 * static_cast<int>(sizeof(float)), Attribute::F32Type);
    addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
    setVertexData(vertex_data);
    setIndexData(index_data);
    setBounds(minimum, maximum);
    update();
}
