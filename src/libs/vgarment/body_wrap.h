//---------------------------------------------------------------------------------------------------------------------
//  @file   body_wrap.h
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

#ifndef BODY_WRAP_H
#define BODY_WRAP_H

#include <QString>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "body_model.h"
#include "garment_mesh.h"
#include "limb_line.h"

/// @brief The parts of the body a piece can be wrapped around.
enum class BodyPart : quint8
{
    Body,
    LeftLeg,
    RightLeg,
    LeftArm,
    RightArm
};

/// @brief Where a piece starts out on the avatar.
struct PieceArrangement
{
    BodyPart part = BodyPart::Body;
    qreal    angle = 0;   ///< degrees around the part, 0 in front, 90 towards +x (on an arm see LimbLine)
    qreal    height = 0;  ///< of the piece's middle above the floor, in cm; on an arm, of the arm's middle line there
};

/// @brief Puts flat pieces around a fitted avatar, wrapped around its body, a leg or an arm, the way they are held
/// up to a dress form before sewing.
///
/// A piece for the body or a leg is bent around an upright cylinder just outside the part it goes on, measured over
/// the piece's height with the arms left out, so it starts clear of the body and the seams can pull it in. A piece
/// for an arm is bent around a tube along the arm's middle line, which follows the arm out from the shoulder, bends
/// with it at the elbow and narrows as the arm does; the piece doesn't reach further up the arm than the shoulder tip,
/// where arm lengths are measured from, so a sleeve's cap starts on top of the arm where the armhole is. Bending
/// around a cylinder keeps the piece's lengths, so it starts out unstretched,
/// except where the tube bends or narrows, most of all around the elbow, where it starts out stretched on the outside
/// of the bend and squeezed on the inside. No piece wraps all the way around, so its sides don't overlap. Seen from
/// outside, a placed piece looks as it does in the piece scene, its top towards the shoulder on an arm.
class BodyWrap
{
public:
                       BodyWrap(const BodyModel& model, const QVector<QVector3D>& positions);

    PieceArrangement   arrangementAt(const QVector3D& point) const;
    QVector<QVector3D> place(const GarmentMesh& mesh, const PieceArrangement& arrangement) const;
    QVector3D          mirrored(const QVector3D& point) const;

    static QString     partName(BodyPart part);
    static BodyPart    partFromName(const QString& name);

private:
    // A skin vertex of an arm below the armpit: where along the arm's middle line it is, and how far from it.
    struct ArmSkin
    {
        float along = 0;
        float distance = 0;
    };

    QVector<QVector3D> m_skin;
    QVector3D          m_pelvis;
    qreal              m_crotch;            // height where the legs part
    QVector3D          m_legs[2][3];        // hip, knee and ankle of the left and the right leg
    QVector3D          m_arms[2][6];        // shoulder, elbow, wrist, and middle finger, thumb and little finger tips
    LimbLine           m_arm_lines[2];      // from the shoulder joint through the elbow and the wrist
    qreal              m_armpits[2];        // how far along its line each arm parts from the body
    qreal              m_shoulder_tips[2];  // how far along its line each arm's shoulder tip is
    QVector<ArmSkin>   m_arm_skin[2];
    QVector<qint8>     m_skin_arms;         // for each skin vertex the arm below the armpit it is on, or -1

    QVector<QVector3D> placeUpright(const GarmentMesh& mesh, const PieceArrangement& arrangement) const;
    QVector3D          axisAt(BodyPart part, qreal height) const;
    qreal              radiusAround(BodyPart part, const QVector3D& axis, qreal from, qreal to) const;
    bool               onArm(const QVector3D& point) const;
    BodyPart           nearestPart(const QVector3D& point) const;

    void               findArmSkin(int side, const QVector<QVector<int>>& neighbours);
    int                armAt(const QVector3D& point) const;
    qreal              armRadius(int side, qreal from, qreal to) const;
    QVector<QVector3D> placeOnArm(const GarmentMesh& mesh, const PieceArrangement& arrangement) const;

    static int         armSide(BodyPart part);
};

#endif // BODY_WRAP_H
