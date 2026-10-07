//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_export.h
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

#ifndef GARMENT_EXPORT_H
#define GARMENT_EXPORT_H

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

/// @brief A mesh to export: a draped piece, or the avatar it drapes on.
struct ExportMesh
{
    QString            name;
    QColor             color;
    QVector<QVector3D> positions;  ///< in cm
    QVector<QPointF>   flat;       ///< where each vertex is in the flat piece, in cm, y down; none for the avatar
    QVector<quint32>   indices;    ///< three per triangle, anticlockwise seen from outside
    QByteArray         image;      ///< an image of its fabric, an image file's bytes; empty for none
    QVector<QPointF>   image_uv;   ///< where each vertex is in the image, in its widths across and heights down
};

/// @brief Writes a draped garment, and the avatar it drapes on, to files other 3D programs open: Wavefront OBJ, with
/// its materials in an MTL file of the same name next to it, and binary glTF (.glb).
///
/// Lengths are in metres, with y up and the avatar facing +z. Each mesh keeps its own vertices, with normals smoothed
/// over its triangles, and its color as its material. A piece's flat shape gives its texture coordinates, in metres
/// too, so a fabric texture keeps its scale on every piece and lies on it as the piece lies in the piece scene.
///
/// A mesh with an image of its fabric has the image as its material's color instead, repeating as its image_uv say:
/// in glTF embedded in the file, on the mesh's second texture coordinates; in OBJ as a file next to it, named after the
/// OBJ file, on the mesh's only texture coordinates. Images other than PNG and JPG are written as PNG.
class GarmentExport
{
public:
    static bool               writeObj(const QString& path, const QVector<ExportMesh>& meshes,
                                       QString* error = nullptr);
    static bool               writeGlb(const QString& path, const QVector<ExportMesh>& meshes,
                                       QString* error = nullptr);
    static QVector<QVector3D> normals(const QVector<QVector3D>& positions, const QVector<quint32>& indices);
};

#endif // GARMENT_EXPORT_H
