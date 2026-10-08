//---------------------------------------------------------------------------------------------------------------------
//  @file   piece_mesher.h
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

#ifndef PIECE_MESHER_H
#define PIECE_MESHER_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include "garment_mesh.h"
#include "piece_outline.h"

class VContainer;
class VPiece;

/// @brief Turns pattern pieces into triangle meshes with evenly sized triangles.
///
/// The seam line is resampled at the edge length, keeping its corners and the piece's path points, and so are the
/// lines inside the piece, ending on the seam line where they come close to it. The rest of the inside is filled with
/// a triangular lattice of the same spacing. Everything is Delaunay triangulated; seam line and line segments the
/// triangulation leaves out are split until it keeps them all, so no triangle crosses the seam line or a line, and the
/// cloth can fold along the lines.
///
/// The edge length plays the role of CLO's particle distance: coarse while editing, fine for the final drape.
class PieceMesher
{
public:
    explicit           PieceMesher(qreal edge_length = defaultEdgeLength());

    static qreal       defaultEdgeLength();

    qreal              edgeLength() const;
    void               setEdgeLength(qreal edge_length);

    GarmentMesh        meshPolygon(const QVector<QPointF>& outline) const;
    GarmentMesh        meshOutline(const PieceOutline& outline) const;
    GarmentMesh        meshPiece(quint32 piece_id, const VPiece& piece, const VContainer* data) const;

    static GarmentMesh meshPoints(const QVector<QPointF>& points);

private:
    qreal              m_edge_length;
};

#endif // PIECE_MESHER_H
