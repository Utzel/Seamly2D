//---------------------------------------------------------------------------------------------------------------------
//  @file   topstitch.h
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

#ifndef TOPSTITCH_H
#define TOPSTITCH_H

#include <QPointF>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "garment_mesh.h"
#include "piece_outline.h"

/// @brief A point on a mesh's surface: the corners of the triangle it lies in, and how much of each corner it takes.
/// Wherever the mesh is moved, the point moves with it.
struct SurfacePoint
{
    quint32 corners[3] = {0, 0, 0};
    float   weights[3] = {1, 0, 0};  ///< add up to 1

    QPointF   restPosition(const GarmentMesh& mesh) const;
    QVector3D position(const QVector<QVector3D>& positions) const;

    bool      operator==(const SurfacePoint& other) const;
    bool      operator!=(const SurfacePoint& other) const;
};

/// @brief One stitch of thread on the cloth: where it comes out of the cloth, its middle, and where it goes back in.
struct ThreadStitch
{
    SurfacePoint start;
    SurfacePoint middle;
    SurfacePoint end;
    float        width = 0.08f;  ///< how thick the thread is, in cm, as Topstitching::defaultThreadWidth()

    bool         operator==(const ThreadStitch& other) const;
    bool         operator!=(const ThreadStitch& other) const;
};

/// @brief The thread of stitches as triangles, to draw or to export: positions, normals of unit length, and three
/// corners per triangle, anticlockwise seen from outside.
struct ThreadMesh
{
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<quint32>   indices;
};

/// @brief Topstitching: rows of stitches a little inside a piece's edges, or along lines drawn on the piece.
///
/// A row along the edges runs at the same distance from them all the way, turning where the edges turn, and ends
/// where it meets the piece's edge; a row along every edge goes all the way round. Rows are lines in the piece's
/// flat coordinates, in cm; stitches() lays thread along them onto a mesh of the piece.
class Topstitching
{
public:
    static qreal                     defaultDistance();
    static qreal                     defaultStitchLength();
    static qreal                     defaultThreadWidth();

    static QVector<QVector<QPointF>> rows(const PieceOutline& outline, const QVector<bool>& stitched,
                                          qreal distance = defaultDistance());
    static QVector<ThreadStitch>     stitches(const GarmentMesh& mesh, const QVector<QVector<QPointF>>& rows,
                                              qreal stitch_length = defaultStitchLength(),
                                              qreal thread_width = defaultThreadWidth());
    static QVector<SurfacePoint>     locate(const GarmentMesh& mesh, const QVector<QPointF>& points);
    static ThreadMesh                threadMesh(const QVector<ThreadStitch>& stitches,
                                                const QVector<QVector3D>& positions,
                                                const QVector<QVector3D>& normals, qreal scale = 1.0,
                                                qreal offset = 0.0);
};

/// @brief A way of topstitching, as CLO's topstitch styles describe one: rows of stitches at given distances inside
/// the edge, how long the stitches are and how thick the thread. The presets are what sewing usually does; their names
/// are what patterns store, the 3D View shows them translated.
struct TopstitchStyle
{
    QString        name;  ///< what patterns store, as the presets are named
    QVector<qreal> distances = {Topstitching::defaultDistance()};  ///< how far inside the edge each row runs, in cm
    qreal          stitch_length = Topstitching::defaultStitchLength();  ///< in cm
    qreal          thread_width = Topstitching::defaultThreadWidth();    ///< how thick the thread is, in cm

    static QVector<TopstitchStyle> presets();
    static TopstitchStyle          preset(const QString& name);
    static QString                 defaultName();
};

#endif // TOPSTITCH_H
