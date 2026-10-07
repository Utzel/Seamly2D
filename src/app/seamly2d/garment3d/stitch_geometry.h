//---------------------------------------------------------------------------------------------------------------------
//  @file   stitch_geometry.h
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

#ifndef STITCH_GEOMETRY_H
#define STITCH_GEOMETRY_H

#include <QVector3D>
#include <QVector>
#include <QtQuick3D/QQuick3DGeometry>

#include "../vgarment/topstitch.h"

struct GarmentMesh;

/// @brief The thread of a piece's topstitching, stitch by stitch, on both faces of the cloth.
///
/// Each stitch is a short spindle of thread from where it comes out of the cloth to where it goes back in, raised a
/// little off the cloth and lit like it, so a row reads as stitching close up and as a fine line from further away.
/// The stitches lie on the piece's mesh and move with it: where the piece is put, or flat on the board without
/// positions, as PieceGeometry shows it.
class StitchGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    Q_PROPERTY(int stitchCount READ stitchCount NOTIFY stitchCountChanged)

public:
    explicit           StitchGeometry(QQuick3DObject* parent = nullptr);

    void               setStitches(const GarmentMesh& mesh, const QVector<ThreadStitch>& stitches,
                                   const QVector<QVector3D>& positions = QVector<QVector3D>(), qreal scale = 1.0,
                                   qreal thickness = 0.0);
    int                stitchCount() const;

signals:
    void               stitchCountChanged();

private:
    Q_DISABLE_COPY(StitchGeometry)

    int                m_stitch_count;
};

#endif // STITCH_GEOMETRY_H
