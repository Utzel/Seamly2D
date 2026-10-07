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

// Tubes are drawn with this many sides.
const int tube_sides = 6;

//---------------------------------------------------------------------------------------------------------------------
QPointF unit(const QPointF& vector)
{
    const qreal length = qSqrt(QPointF::dotProduct(vector, vector));
    return length > 0 ? vector / length : QPointF();
}

//---------------------------------------------------------------------------------------------------------------------
void appendVertex(QVector<float>& vertices, const QVector3D& position, const QColor& color)
{
    vertices << position.x() << position.y() << position.z() << static_cast<float>(color.redF())
             << static_cast<float>(color.greenF()) << static_cast<float>(color.blueF())
             << static_cast<float>(color.alphaF());
}

//---------------------------------------------------------------------------------------------------------------------
void appendVertex(QVector<float>& vertices, const QPointF& position, const QColor& color)
{
    appendVertex(vertices, QVector3D(static_cast<float>(position.x()), static_cast<float>(position.y()), 0.0f), color);
}

//---------------------------------------------------------------------------------------------------------------------
// The tube's points without repeats, which have no direction.
QVector<QVector3D> distinctPoints(const QVector<QVector3D>& points)
{
    QVector<QVector3D> distinct;
    for (const QVector3D& point : points)
    {
        if (distinct.isEmpty() || (point - distinct.last()).length() > 1e-6f)
        {
            distinct.append(point);
        }
    }
    return distinct;
}

//---------------------------------------------------------------------------------------------------------------------
// Any direction square to the given one.
QVector3D squareTo(const QVector3D& direction)
{
    const QVector3D other = qAbs(direction.x()) < 0.9f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
    return QVector3D::crossProduct(direction, other).normalized();
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
/// @brief Replaces the geometry with tubes of the given radii, in cm, along their points in space. Each ring of a tube
/// is turned as little as it can from the one before, so the tube doesn't twist.
void SeamGeometry::setTubes(const QVector<Tube>& tubes)
{
    QVector<float> vertices;
    QVector<quint32> indices;
    for (const Tube& tube : tubes)
    {
        const QVector<QVector3D> points = distinctPoints(tube.points);
        const int count = static_cast<int>(points.size());
        if (count < 2)
        {
            continue;
        }

        const quint32 first = static_cast<quint32>(vertices.size() / floats_per_vertex);
        const float radius = static_cast<float>(tube.radius);
        QVector3D across;
        for (int i = 0; i < count; ++i)
        {
            const QVector3D incoming = i > 0 ? (points.at(i) - points.at(i - 1)).normalized() : QVector3D();
            const QVector3D outgoing = i + 1 < count ? (points.at(i + 1) - points.at(i)).normalized() : QVector3D();
            QVector3D tangent = (incoming + outgoing).normalized();
            tangent = tangent.isNull() ? (i > 0 ? incoming : outgoing) : tangent;

            across = across.isNull() ? squareTo(tangent) : across - tangent * QVector3D::dotProduct(across, tangent);
            across = across.isNull() ? squareTo(tangent) : across.normalized();
            const QVector3D up = QVector3D::crossProduct(tangent, across);
            for (int side = 0; side < tube_sides; ++side)
            {
                const float angle = static_cast<float>(2.0 * M_PI * side / tube_sides);
                appendVertex(vertices, points.at(i) + (across * qCos(angle) + up * qSin(angle)) * radius, tube.color);
            }
        }

        const quint32 ring = static_cast<quint32>(tube_sides);
        for (int i = 0; i + 1 < count; ++i)
        {
            for (int side = 0; side < tube_sides; ++side)
            {
                const quint32 a = first + static_cast<quint32>(i * tube_sides + side);
                const quint32 b = first + static_cast<quint32>(i * tube_sides + (side + 1) % tube_sides);
                indices << a << b << a + ring << b << b + ring << a + ring;
            }
        }
    }
    setVertices(vertices, indices, PrimitiveType::Triangles);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with thin lines between points in space.
void SeamGeometry::setSegments(const QVector<Segment>& segments)
{
    QVector<float> vertices;
    QVector<quint32> indices;
    for (const Segment& segment : segments)
    {
        const quint32 from = static_cast<quint32>(vertices.size() / floats_per_vertex);
        indices << from << from + 1;
        appendVertex(vertices, segment.from, segment.color);
        appendVertex(vertices, segment.to, segment.color);
    }
    setVertices(vertices, indices, PrimitiveType::Lines);
}

//---------------------------------------------------------------------------------------------------------------------
void SeamGeometry::setVertices(const QVector<float>& vertices, const QVector<quint32>& indices,
                               PrimitiveType primitive_type)
{
    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum(-largest, -largest, -largest);
    for (int i = 0; i + 2 < vertices.size(); i += floats_per_vertex)
    {
        const QVector3D position(vertices.at(i), vertices.at(i + 1), vertices.at(i + 2));
        minimum = QVector3D(qMin(minimum.x(), position.x()), qMin(minimum.y(), position.y()),
                            qMin(minimum.z(), position.z()));
        maximum = QVector3D(qMax(maximum.x(), position.x()), qMax(maximum.y(), position.y()),
                            qMax(maximum.z(), position.z()));
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
