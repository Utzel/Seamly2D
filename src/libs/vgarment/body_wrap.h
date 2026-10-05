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

/// @brief The parts of the body a piece can be wrapped around.
enum class BodyPart : quint8
{
    Body,
    LeftLeg,
    RightLeg
};

/// @brief Where a piece starts out on the avatar.
struct PieceArrangement
{
    BodyPart part = BodyPart::Body;
    qreal    angle = 0;   ///< degrees around the part, 0 in front, 90 towards +x
    qreal    height = 0;  ///< of the piece's middle above the floor, in cm
};

/// @brief Puts flat pieces around a fitted avatar, wrapped around its body or a leg, the way they are held up to a
/// dress form before sewing.
///
/// A piece is bent around an upright cylinder just outside the part it goes on, measured over the piece's height
/// with the arms left out, so it starts clear of the body and the seams can pull it in. Bending around a cylinder
/// keeps the piece's lengths, so it starts out unstretched. Seen from outside, a placed piece looks as it does in
/// the piece scene.
class BodyWrap
{
public:
                       BodyWrap(const BodyModel& model, const QVector<QVector3D>& positions);

    PieceArrangement   arrangementAt(const QVector3D& point) const;
    QVector<QVector3D> place(const GarmentMesh& mesh, const PieceArrangement& arrangement) const;

    static QString     partName(BodyPart part);
    static BodyPart    partFromName(const QString& name);

private:
    QVector<QVector3D> m_skin;
    QVector3D          m_pelvis;
    qreal              m_crotch;      // height where the legs part
    QVector3D          m_legs[2][3];  // hip, knee and ankle of the left and the right leg
    QVector3D          m_arms[2][3];  // shoulder, elbow and hand of the left and the right arm

    QVector3D          axisAt(BodyPart part, qreal height) const;
    qreal              radiusAround(BodyPart part, const QVector3D& axis, qreal from, qreal to) const;
    bool               onArm(const QVector3D& point) const;
    BodyPart           nearestPart(const QVector3D& point) const;
};

#endif // BODY_WRAP_H
