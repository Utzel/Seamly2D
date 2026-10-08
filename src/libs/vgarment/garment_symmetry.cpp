//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_symmetry.cpp
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

#include "garment_symmetry.h"

#include "piece_outline.h"

//---------------------------------------------------------------------------------------------------------------------
bool GarmentSeamSide::operator==(const GarmentSeamSide& other) const
{
    return piece == other.piece && start_node == other.start_node && end_node == other.end_node;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How a piece is made up; a piece cut on the fold also needs the path points its fold line runs between.
void GarmentSymmetry::setPiece(quint32 piece, PieceSymmetry symmetry, quint32 fold_start, quint32 fold_end)
{
    Piece made_up;
    made_up.symmetry = symmetry;
    made_up.fold_start = fold_start;
    made_up.fold_end = fold_end;
    m_pieces.insert(piece, made_up);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How a piece is made up; pieces not set are cut once.
PieceSymmetry GarmentSymmetry::symmetry(quint32 piece) const
{
    return m_pieces.value(piece).symmetry;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seams of the whole garment: the pattern's seams, each one's twin on the other side, and seams sewing a
/// piece to its mirror image.
QVector<GarmentSeam> GarmentSymmetry::madeUp(const QVector<GarmentSeam>& seams) const
{
    QVector<GarmentSeam> made_up;
    for (const GarmentSeam& seam : seams)
    {
        if (seam.first == seam.second)
        {
            // A side sewn to itself is sewn to its mirror image, which only a piece cut twice has.
            if (symmetry(seam.first.piece) == PieceSymmetry::Pair)
            {
                GarmentSeam to_mirror = seam;
                to_mirror.second.piece = PieceOutline::mirrorId(seam.second.piece);
                made_up.append(to_mirror);
            }
        }
        else
        {
            made_up.append(seam);

            GarmentSeam twin;
            bool first_turned = false;
            bool second_turned = false;
            if (mirrored(seam.first, &twin.first, &first_turned) && mirrored(seam.second, &twin.second, &second_turned))
            {
                twin.reverse = seam.reverse != (first_turned != second_turned);
                twin.angle = seam.angle;
                made_up.append(twin);
            }
        }
    }
    return made_up;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The side's mirror image on the other side of the garment; false for a piece cut once, which has none. On the
/// mirrored half of an unfolded piece the seam line runs the other way, so the side's ends swap: turned says so.
bool GarmentSymmetry::mirrored(const GarmentSeamSide& side, GarmentSeamSide* mirror, bool* turned) const
{
    const Piece piece = m_pieces.value(side.piece);
    bool has_mirror = true;
    if (piece.symmetry == PieceSymmetry::Pair)
    {
        *mirror = side;
        mirror->piece = PieceOutline::mirrorId(side.piece);
        *turned = false;
    }
    else if (piece.symmetry == PieceSymmetry::Fold)
    {
        // The fold line's ends are on both halves.
        auto mirror_node = [&piece](quint32 node)
        {
            return node == piece.fold_start || node == piece.fold_end ? node : PieceOutline::mirrorId(node);
        };
        mirror->piece = side.piece;
        mirror->start_node = mirror_node(side.end_node);
        mirror->end_node = mirror_node(side.start_node);
        *turned = true;
    }
    else
    {
        has_mirror = false;
    }
    return has_mirror;
}
