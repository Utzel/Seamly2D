//---------------------------------------------------------------------------------------------------------------------
//  @file   shown_piece.h
//  @author Julius
//  @date   7 Oct, 2026
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

#ifndef SHOWN_PIECE_H
#define SHOWN_PIECE_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>

#include "../vgarment/garment_mesh.h"
#include "../vgarment/garment_symmetry.h"
#include "../vgarment/piece_outline.h"

/// @brief How a mesh the 3D scene shows lies over the piece as drafted.
enum class PieceLayout : quint8
{
    Drafted,   ///< as drafted: the piece on the board, or on the avatar a piece that isn't unfolded
    Unfolded,  ///< the drafted half and its mirror image across the fold line
    Mirrored   ///< the mirrored copy of a piece cut twice
};

/// @brief A mesh the 3D scene shows of a piece, by the scene's id: the piece's, or its mirrored copy's.
struct ShownMesh
{
    quint32     id = 0;
    GarmentMesh mesh;
    PieceLayout layout = PieceLayout::Drafted;
    bool        placed = false;  ///< on the avatar, rather than on the board
};

/// @brief A pattern piece as the 3D scene shows it: as drafted, how it is made up into the garment, and the meshes the
/// scene shows of it. A point of a shown mesh's flat shape maps back onto the piece as drafted, and lines drawn on the
/// piece as drafted map onto each mesh.
struct ShownPiece
{
    quint32                   id = 0;
    PieceOutline              outline;                            ///< as drafted
    PieceSymmetry             symmetry = PieceSymmetry::Single;
    quint32                   fold_start = 0;                     ///< the fold line's ends when cut on the fold
    quint32                   fold_end = 0;
    QVector<QVector<QPointF>> paths;                              ///< internal paths drawn as stitching, in cm
    QVector<ShownMesh>        shown;

    const ShownMesh*          mesh(quint32 id) const;
    bool                      isPlaced() const;
    QVector<bool>             foldSegments() const;
    QPointF                   drafted(PieceLayout layout, const QPointF& point) const;
    QVector<QVector<QPointF>> laidOut(const QVector<QVector<QPointF>>& lines, PieceLayout layout) const;

private:
    bool                      foldLine(QPointF* start, QPointF* end) const;
    int                       nodePosition(quint32 id) const;
};

#endif // SHOWN_PIECE_H
