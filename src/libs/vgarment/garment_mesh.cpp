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
#include <QtMath>

#include <limits>
#include <utility>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
qreal distanceToSide(const QPointF& point, const QPointF& start, const QPointF& end)
{
    const QPointF side = end - start;
    const qreal length_squared = QPointF::dotProduct(side, side);
    const qreal along = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - start, side) / length_squared, 1.0)
                                           : 0.0;
    const QPointF apart = point - (start + side * along);
    return qSqrt(QPointF::dotProduct(apart, apart));
}
} // anonymous namespace

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

//---------------------------------------------------------------------------------------------------------------------
/// @brief How much the cloth around each vertex is stretched with the vertices at the given positions, as a share of
/// its drafted size: 0.1 is 10% longer than drafted, 0 or less not stretched at all.
///
/// A triangle's stretch is the largest of its principal stretches, so moving, turning or bending the cloth doesn't
/// count, and stretching it in any direction does. A vertex gets the mean of the triangles around it, weighed by
/// their drafted area. Positions for another mesh leave every vertex at 0.
QVector<qreal> GarmentMesh::strain(const QVector<QVector3D>& positions) const
{
    const int count = vertexCount();
    QVector<qreal> strain(count, 0.0);
    if (positions.size() != count)
    {
        return strain;
    }

    QVector<qreal> weight(count, 0.0);
    for (int i = 0; i + 2 < indices.size(); i += 3)
    {
        const int a = static_cast<int>(indices.at(i));
        const int b = static_cast<int>(indices.at(i + 1));
        const int c = static_cast<int>(indices.at(i + 2));

        // The drafted triangle's sides, and what they became.
        const QPointF drafted_ab = rest_positions.at(b) - rest_positions.at(a);
        const QPointF drafted_ac = rest_positions.at(c) - rest_positions.at(a);
        const qreal doubled_area = drafted_ab.x() * drafted_ac.y() - drafted_ac.x() * drafted_ab.y();
        if (qFuzzyIsNull(doubled_area))
        {
            continue;
        }
        const QVector3D ab = positions.at(b) - positions.at(a);
        const QVector3D ac = positions.at(c) - positions.at(a);

        // Where the drafted x and y directions went: the columns of the deformation gradient.
        const QVector3D along_x = (ab * static_cast<float>(drafted_ac.y()) - ac * static_cast<float>(drafted_ab.y()))
                                  / static_cast<float>(doubled_area);
        const QVector3D along_y = (ac * static_cast<float>(drafted_ab.x()) - ab * static_cast<float>(drafted_ac.x()))
                                  / static_cast<float>(doubled_area);

        // The largest eigenvalue of the right Cauchy-Green tensor is the square of the largest principal stretch.
        const qreal xx = QVector3D::dotProduct(along_x, along_x);
        const qreal yy = QVector3D::dotProduct(along_y, along_y);
        const qreal xy = QVector3D::dotProduct(along_x, along_y);
        const qreal half_trace = (xx + yy) / 2.0;
        const qreal largest = half_trace + qSqrt(qMax(0.0, half_trace * half_trace - (xx * yy - xy * xy)));
        const qreal triangle_strain = qSqrt(largest) - 1.0;

        const qreal area = qAbs(doubled_area) / 2.0;
        for (const int vertex : {a, b, c})
        {
            strain[vertex] += triangle_strain * area;
            weight[vertex] += area;
        }
    }

    for (int i = 0; i < count; ++i)
    {
        strain[i] = weight.at(i) > 0 ? strain.at(i) / weight.at(i) : 0.0;
    }
    return strain;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the vertices of a changed version of the piece go, with this mesh's vertices at the given positions:
/// each where the point of this mesh at the same place in the flat piece is. Beyond this mesh, the triangle nearest
/// carries on, so a lengthened hem goes on hanging the way the cloth above it hangs. Lets an edited piece start out
/// where it was draped. Positions for another mesh carry nothing over.
QVector<QVector3D> GarmentMesh::carry(const QVector<QVector3D>& positions, const GarmentMesh& changed) const
{
    QVector<QVector3D> carried;
    if (positions.size() != vertexCount() || isEmpty())
    {
        return carried;
    }

    carried.reserve(changed.vertexCount());
    for (const QPointF& point : changed.rest_positions)
    {
        // The triangle the point lies in, or else the one it lies nearest, and the point's weights in it.
        qreal nearest = std::numeric_limits<qreal>::max();
        qreal weights[3] = {1, 0, 0};
        int corners[3] = {0, 0, 0};
        for (int i = 0; i + 2 < indices.size() && nearest > 0; i += 3)
        {
            const int a = static_cast<int>(indices.at(i));
            const int b = static_cast<int>(indices.at(i + 1));
            const int c = static_cast<int>(indices.at(i + 2));
            const QPointF ab = rest_positions.at(b) - rest_positions.at(a);
            const QPointF ac = rest_positions.at(c) - rest_positions.at(a);
            const QPointF ap = point - rest_positions.at(a);
            const qreal doubled_area = ab.x() * ac.y() - ac.x() * ab.y();
            if (qFuzzyIsNull(doubled_area))
            {
                continue;
            }
            const qreal along_b = (ap.x() * ac.y() - ac.x() * ap.y()) / doubled_area;
            const qreal along_c = (ab.x() * ap.y() - ap.x() * ab.y()) / doubled_area;
            const qreal along_a = 1.0 - along_b - along_c;

            // How far outside the triangle the point is, 0 inside.
            qreal outside = 0;
            if (along_a < 0 || along_b < 0 || along_c < 0)
            {
                outside = qMin(distanceToSide(point, rest_positions.at(a), rest_positions.at(b)),
                               qMin(distanceToSide(point, rest_positions.at(b), rest_positions.at(c)),
                                    distanceToSide(point, rest_positions.at(c), rest_positions.at(a))));
            }
            if (outside < nearest)
            {
                nearest = outside;
                weights[0] = along_a;
                weights[1] = along_b;
                weights[2] = along_c;
                corners[0] = a;
                corners[1] = b;
                corners[2] = c;
            }
        }

        QVector3D position;
        for (int k = 0; k < 3; ++k)
        {
            position += positions.at(corners[k]) * static_cast<float>(weights[k]);
        }
        carried.append(position);
    }
    return carried;
}
