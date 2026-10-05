//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_symmetry.h
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

#ifndef GARMENT_SYMMETRY_H
#define GARMENT_SYMMETRY_H

#include <QHash>
#include <QVector>
#include <QtGlobal>

/// @brief How a pattern piece is made up into the garment.
enum class PieceSymmetry : quint8
{
    Single,  ///< cut once, as drafted
    Pair,    ///< cut twice, the second a mirror image, as from doubled fabric
    Fold     ///< cut on the fold, unfolded into a whole piece across its fold line
};

/// @brief One side of a seam in the garment: a stretch of a piece's seam line from one path point forward to another.
/// Mirrored copies and mirrored halves have mirrored ids (PieceOutline::mirrorId()).
struct GarmentSeamSide
{
    quint32 piece = 0;
    quint32 start_node = 0;
    quint32 end_node = 0;

    bool    operator==(const GarmentSeamSide& other) const;
};

/// @brief Two sides sewn together, their starts meeting, or with reverse the first side's start the second's end.
struct GarmentSeam
{
    GarmentSeamSide first;
    GarmentSeamSide second;
    bool            reverse = false;
};

/// @brief Makes up the seams of a garment that is the same on the left and the right.
///
/// A pattern only has the pieces of one side, and only the seams between them. Each seam between pieces that are
/// mirrored, cut twice or cut on the fold, has a twin on the other side. A side of a piece cut twice sewn to itself
/// means sewn to its mirror image, as a centre back seam.
class GarmentSymmetry
{
public:
    void               setPiece(quint32 piece, PieceSymmetry symmetry, quint32 fold_start = 0, quint32 fold_end = 0);
    PieceSymmetry      symmetry(quint32 piece) const;

    QVector<GarmentSeam> madeUp(const QVector<GarmentSeam>& seams) const;

private:
    struct Piece
    {
        PieceSymmetry symmetry = PieceSymmetry::Single;
        quint32       fold_start = 0;
        quint32       fold_end = 0;
    };

    QHash<quint32, Piece> m_pieces;

    bool               mirrored(const GarmentSeamSide& side, GarmentSeamSide* mirror, bool* turned) const;
};

#endif // GARMENT_SYMMETRY_H
