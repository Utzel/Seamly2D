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

#include <algorithm>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// Whether two sides are the same stretch of seam line, whichever way they run.
bool sameStretch(const GarmentSeamSide& one, const GarmentSeamSide& other)
{
    return one.piece == other.piece
           && ((one.start_node == other.start_node && one.end_node == other.end_node)
               || (one.start_node == other.end_node && one.end_node == other.start_node));
}

//---------------------------------------------------------------------------------------------------------------------
// Whether any stretch of one seam's sides is also one of another's.
bool sharesStretches(const GarmentSeam& one, const GarmentSeam& other)
{
    const QVector<GarmentSeamSide> others = other.firstSide() + other.secondSide();
    const QVector<GarmentSeamSide> ones = one.firstSide() + one.secondSide();
    return std::any_of(ones.cbegin(), ones.cend(), [&others](const GarmentSeamSide& side)
    {
        return std::any_of(others.cbegin(), others.cend(), [&side](const GarmentSeamSide& another)
        {
            return sameStretch(side, another);
        });
    });
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool GarmentSeamSide::operator==(const GarmentSeamSide& other) const
{
    return piece == other.piece && start_node == other.start_node && end_node == other.end_node
           && backward == other.backward;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stretches of the first side, in the order it goes on over them.
QVector<GarmentSeamSide> GarmentSeam::firstSide() const
{
    return QVector<GarmentSeamSide>{first} + first_more;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stretches of the second side, in the order it goes on over them.
QVector<GarmentSeamSide> GarmentSeam::secondSide() const
{
    return QVector<GarmentSeamSide>{second} + second_more;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A side going on over several stretches, sewn the other way round: the stretches in the opposite order, each
/// running the other way.
QVector<GarmentSeamSide> reversedSide(const QVector<GarmentSeamSide>& side)
{
    QVector<GarmentSeamSide> reversed;
    for (auto stretch = side.crbegin(); stretch != side.crend(); ++stretch)
    {
        GarmentSeamSide turned = *stretch;
        turned.backward = !turned.backward;
        reversed.append(turned);
    }
    return reversed;
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
        if (seam.first == seam.second && seam.first_more.isEmpty() && seam.second_more.isEmpty())
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

            // A side of one stretch runs along its path, as the pattern's do: the seam is turned for each that
            // doesn't. A side of several goes on over their mirror images in the same order.
            QVector<GarmentSeamSide> first_side;
            QVector<GarmentSeamSide> second_side;
            if (mirroredSide(seam.firstSide(), &first_side) && mirroredSide(seam.secondSide(), &second_side))
            {
                GarmentSeam twin;
                twin.reverse = seam.reverse;
                for (QVector<GarmentSeamSide>* side : {&first_side, &second_side})
                {
                    if (side->size() == 1 && side->first().backward)
                    {
                        *side = reversedSide(*side);
                        twin.reverse = !twin.reverse;
                    }
                }
                twin.first = first_side.first();
                twin.first_more = first_side.mid(1);
                twin.second = second_side.first();
                twin.second_more = second_side.mid(1);
                twin.angle = seam.angle;
                if (!sharesStretches(twin, seam))
                {
                    made_up.append(twin);
                }
            }
        }
    }
    return made_up;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The side's mirror image on the other side of the garment; false for a piece cut once, which has none. The
/// mirror image of a side on a piece cut twice is on its mirror image, and the other way round. On the mirrored half
/// of an unfolded piece the seam line runs the other way, so the side's ends swap: turned says so.
bool GarmentSymmetry::mirrored(const GarmentSeamSide& side, GarmentSeamSide* mirror, bool* turned) const
{
    const bool on_mirror_image = PieceOutline::isMirrorId(side.piece);
    const Piece piece = m_pieces.value(on_mirror_image ? PieceOutline::mirrorId(side.piece) : side.piece);
    bool has_mirror = true;
    if (piece.symmetry == PieceSymmetry::Pair)
    {
        *mirror = side;
        mirror->piece = PieceOutline::mirrorId(side.piece);
        *turned = false;
    }
    else if (piece.symmetry == PieceSymmetry::Fold && !on_mirror_image)
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

//---------------------------------------------------------------------------------------------------------------------
// The mirror image of a side going on over several stretches, each stretch's, running the same way as the side's own
// does; false if any stretch has none.
bool GarmentSymmetry::mirroredSide(const QVector<GarmentSeamSide>& side, QVector<GarmentSeamSide>* mirror) const
{
    mirror->clear();
    for (const GarmentSeamSide& stretch : side)
    {
        GarmentSeamSide mirrored_stretch;
        bool turned = false;
        if (!mirrored(stretch, &mirrored_stretch, &turned))
        {
            return false;
        }
        mirrored_stretch.backward = stretch.backward != turned;
        mirror->append(mirrored_stretch);
    }
    return !mirror->isEmpty();
}
