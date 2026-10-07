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
// Corners have a position and a normal.
const int floats_per_corner = 3 + 3;
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
    const ThreadMesh thread = Topstitching::threadMesh(stitches, placed, PieceGeometry::vertexNormals(mesh, placed),
                                                       scale);

    const int vertex_bytes = floats_per_corner * static_cast<int>(sizeof(float));
    QByteArray vertex_data(static_cast<int>(thread.positions.size()) * vertex_bytes, Qt::Uninitialized);
    float* vertex = reinterpret_cast<float*>(vertex_data.data());
    const float largest = std::numeric_limits<float>::max();
    QVector3D minimum(largest, largest, largest);
    QVector3D maximum = -minimum;
    for (int i = 0; i < thread.positions.size(); ++i)
    {
        const QVector3D& position = thread.positions.at(i);
        const QVector3D& normal = thread.normals.at(i);
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
    if (thread.positions.isEmpty())
    {
        minimum = QVector3D();
        maximum = QVector3D();
    }
    const QByteArray index_data(reinterpret_cast<const char*>(thread.indices.constData()),
                                static_cast<int>(thread.indices.size() * static_cast<int>(sizeof(quint32))));

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

    const int count = static_cast<int>(stitches.size());
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
