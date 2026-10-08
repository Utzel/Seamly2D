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
/// Mirrored copies and mirrored halves have mirrored ids (PieceOutline::mirrorId()). As a stretch of a side that goes
/// on over several, it can run backward along the path instead.
struct GarmentSeamSide
{
    quint32 piece = 0;
    quint32 start_node = 0;
    quint32 end_node = 0;
    bool    backward = false;

    bool    operator==(const GarmentSeamSide& other) const;
};

/// @brief Two sides sewn together, their starts meeting, or with reverse the first side's start the second's end, and
/// the angle the seam holds the pieces at, on the first side's right side, as ClothFold says; less than 0 for none,
/// the pieces bending across it as they will. As CLO's M:N sewing, either side can go on over more stretches, of the
/// same piece or others, one after the other; the other side is eased onto all of them evenly.
struct GarmentSeam
{
    GarmentSeamSide          first;
    GarmentSeamSide          second;
    bool                     reverse = false;
    qreal                    angle = -1.0;
    QVector<GarmentSeamSide> first_more;
    QVector<GarmentSeamSide> second_more;

    QVector<GarmentSeamSide> firstSide() const;
    QVector<GarmentSeamSide> secondSide() const;
};

QVector<GarmentSeamSide> reversedSide(const QVector<GarmentSeamSide>& side);

/// @brief Makes up the seams of a garment that is the same on the left and the right.
///
/// A pattern only has the pieces of one side, and only the seams between them. Each seam between pieces that are
/// mirrored, cut twice or cut on the fold, has a twin on the other side. A side of a piece cut twice sewn to itself
/// means sewn to its mirror image, as a centre back seam. A seam going on over several stretches has a twin if all of
/// them have one, on mirror images or mirrored halves, but not if the twin would sew any of the same stretches: a
/// waistband sewn all around the body is whole already.
class GarmentSymmetry
{
public:
    void               setPiece(quint32 piece, PieceSymmetry symmetry, quint32 fold_start = 0, quint32 fold_end = 0);
    PieceSymmetry      symmetry(quint32 piece) const;

    QVector<GarmentSeam> madeUp(const QVector<GarmentSeam>& seams) const;
    bool               mirrored(const GarmentSeamSide& side, GarmentSeamSide* mirror, bool* turned) const;

private:
    struct Piece
    {
        PieceSymmetry symmetry = PieceSymmetry::Single;
        quint32       fold_start = 0;
        quint32       fold_end = 0;
    };

    QHash<quint32, Piece> m_pieces;

    bool               mirroredSide(const QVector<GarmentSeamSide>& side, QVector<GarmentSeamSide>* mirror) const;
};

#endif // GARMENT_SYMMETRY_H
