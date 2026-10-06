//---------------------------------------------------------------------------------------------------------------------
//  @file   stitch_geometry.cpp
//  @author Julius
//  @date   6 Oct, 2026
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

#include "stitch_geometry.h"

#include <QByteArray>

#include <limits>

#include "../vgarment/garment_mesh.h"
#include "piece_geometry.h"

namespace
{
// The thread's size in cm: how wide and how high a stitch is at its middle, and how far off the cloth it lies, so
// the cloth doesn't show through it.
const float thread_width = 0.08f;
const float thread_height = 0.035f;
const float thread_lift = 0.02f;

// How far the normals at a stitch's ends and sides lean along it and across it, so it lights up round.
const float end_lean = 0.5f;
const float side_lean = 2.0f;

// A stitch has five corners on each face of the cloth: its two ends, its two sides and its ridge, and four
// triangles between them. Corners have a position and a normal.
const int corners_per_face = 5;
const int floats_per_corner = 3 + 3;
const quint32 face_triangles[] = {0, 4, 2, 0, 3, 4, 1, 2, 4, 1, 4, 3};

//---------------------------------------------------------------------------------------------------------------------
void appendCorner(float*& vertex, const QVector3D& position, const QVector3D& normal)
{
    *vertex++ = position.x();
    *vertex++ = position.y();
    *vertex++ = position.z();
    *vertex++ = normal.x();
    *vertex++ = normal.y();
    *vertex++ = normal.z();
}

//---------------------------------------------------------------------------------------------------------------------
// Any direction square to the given one, for a stitch whose cloth normal runs along it.
QVector3D squareTo(const QVector3D& direction)
{
    const QVector3D other = qAbs(direction.x()) < 0.9f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
    return QVector3D::crossProduct(direction, other).normalized();
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
StitchGeometry::StitchGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
    , m_stitch_count(0)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the geometry with the stitches on the mesh, at the given positions in cm, or flat on the board
/// without them. Scale makes the thread thicker and lifts it further, as for stitches shown over others.
void StitchGeometry::setStitches(const GarmentMesh& mesh, const QVector<ThreadStitch>& stitches,
                                 const QVector<QVector3D>& positions, qreal scale)
{
    const QVector<QVector3D> placed = PieceGeometry::placedPositions(mesh, positions);
    const QVector<QVector3D> normals = PieceGeometry::vertexNormals(mesh, placed);
    const float size = static_cast<float>(scale);

    const int count = static_cast<int>(stitches.size());
    const int vertex_bytes = floats_per_corner * static_cast<int>(sizeof(float));
    QByteArray vertex_data(count * 2 * corners_per_face * vertex_bytes, Qt::Uninitialized);
    QByteArray index_data(count * 2 * static_cast<int>(sizeof(face_triangles)), Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    quint32* index = reinterpret_cast<quint32*>(index_data.data());

    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum = -minimum;
    quint32 first = 0;
    for (const ThreadStitch& stitch : stitches)
    {
        const QVector3D start = stitch.start.position(placed);
        const QVector3D middle = stitch.middle.position(placed);
        const QVector3D end = stitch.end.position(placed);
        QVector3D normal = stitch.middle.position(normals);
        normal = normal.isNull() ? QVector3D(0, 0, 1) : normal.normalized();
        QVector3D along = end - start;
        along = along.isNull() ? squareTo(normal) : along.normalized();
        QVector3D side = QVector3D::crossProduct(normal, along);
        side = side.isNull() ? squareTo(along) : side.normalized();

        // The same spindle on each face of the cloth, turned over for the back, so both face outwards.
        for (const float face : {1.0f, -1.0f})
        {
            const QVector3D up = normal * face;
            const QVector3D left = side * face;
            const QVector3D lift = up * (thread_lift * size);
            const QVector3D corners[corners_per_face] = {
                start + lift, end + lift, middle + lift + left * (thread_width * size / 2),
                middle + lift - left * (thread_width * size / 2), middle + lift + up * (thread_height * size)};
            const QVector3D corner_normals[corners_per_face] = {
                (up - along * end_lean).normalized(), (up + along * end_lean).normalized(),
                (up + left * side_lean).normalized(), (up - left * side_lean).normalized(), up};
            for (int k = 0; k < corners_per_face; ++k)
            {
                appendCorner(vertex, corners[k], corner_normals[k]);
                minimum = QVector3D(qMin(minimum.x(), corners[k].x()), qMin(minimum.y(), corners[k].y()),
                                    qMin(minimum.z(), corners[k].z()));
                maximum = QVector3D(qMax(maximum.x(), corners[k].x()), qMax(maximum.y(), corners[k].y()),
                                    qMax(maximum.z(), corners[k].z()));
            }
            for (const quint32 corner : face_triangles)
            {
                *index++ = first + corner;
            }
            first += corners_per_face;
        }
    }
    if (count == 0)
    {
        minimum = QVector3D();
        maximum = QVector3D();
    }

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

    if (count != m_stitch_count)
    {
        m_stitch_count = count;
        emit stitchCountChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How many stitches there are; the geometry is empty without any.
int StitchGeometry::stitchCount() const
{
    return m_stitch_count;
}
