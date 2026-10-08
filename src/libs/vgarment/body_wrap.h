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

#include <QPair>
#include <QPointF>
#include <QQuaternion>
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

/// @brief Where a piece starts out on the avatar, and which way round.
struct PieceArrangement
{
    BodyPart part = BodyPart::Body;
    qreal    angle = 0;            ///< degrees around the part, 0 in front, 90 towards +x (on an arm see LimbLine)
    qreal    height = 0;           ///< of the piece's middle above the floor, in cm; on an arm, of the arm's middle
                                   ///< line there
    qreal    rotation = 0;         ///< degrees the piece is turned clockwise about its middle, as seen from outside
    bool     turned_over = false;  ///< the piece's other side out, as if cut from the cloth turned over
    QString  point;                ///< the arrangement point it was put at, if any; it goes there on any avatar
    qreal    distance = 0;         ///< cm further out from the body than pieces start out
    qreal    lean = 0;             ///< degrees its top leans out, about the line across its middle
    qreal    swing = 0;            ///< degrees its side towards larger angles swings out, about the line up its middle
};

/// @brief Which way a piece put on the avatar faces at its middle, as wrapped before it leans or swings: across it
/// towards larger angles around its part, up it towards its top, and out from the body.
struct PieceFrame
{
    QVector3D middle;
    QVector3D across;
    QVector3D up;
    QVector3D out;
};

/// @brief How a piece is laid on a piece it is sewn to, as CLO's Superimpose: over it, as a pocket or a collar lies,
/// under it, as a facing or a lining does, or beside it, edge to edge, as the next panel.
enum class Superimpose : quint8
{
    Over,
    Under,
    Side
};

/// @brief The two sides of a seam between a piece and the piece it is sewn to, as their vertices, in the order in
/// which they meet: the first of each at the same end.
struct SewnSides
{
    QVector<quint32> piece;
    QVector<quint32> partner;
};

/// @brief A place on the avatar to put pieces at, as CLO's arrangement points. Left and right are the avatar's own,
/// as for its legs and arms.
struct ArrangementPoint
{
    QString          name;         ///< as the pattern file keeps it, the part, level and side: "body-waist-front"
    QString          level;        ///< neck, bust, waist, hip or thigh on the body, thigh, knee or calf on a leg,
                                   ///< upperArm, elbow or wrist on an arm
    QString          side;         ///< front, frontLeft, left, backLeft, back, backRight, right or frontRight on the
                                   ///< body, front, outside, back or inside on a leg or an arm
    PieceArrangement arrangement;  ///< where a piece put there goes
    QVector3D        position;     ///< on the body, where it shows
    QVector3D        normal;       ///< outwards from the body there
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
/// outside, a placed piece looks as it does in the piece scene, its top towards the shoulder on an arm, unless it is
/// rotated or turned over. It can also be put further out, and lean or swing out as it is, as CLO's gizmo turns it.
///
/// Pieces can be put anywhere on the avatar, or at its arrangement points: in front, at the sides and behind the body
/// at the neck, the bust, the waist, the hip and halfway down the thighs, and around each leg and arm where its
/// middles and joints are. A point is where its level is on this avatar, so a piece put there goes to the same place
/// on any avatar.
class BodyWrap
{
public:
                       BodyWrap(const BodyModel& model, const QVector<QVector3D>& positions);

    PieceArrangement   arrangementAt(const QVector3D& point) const;
    PieceArrangement   arrangementOn(BodyPart part, const QVector3D& point) const;
    PieceArrangement   resolved(const PieceArrangement& arrangement) const;
    QVector<QVector3D> place(const GarmentMesh& mesh, const PieceArrangement& arrangement, qreal out = 0) const;
    PieceFrame         frameOf(const GarmentMesh& mesh, const PieceArrangement& arrangement) const;
    PieceArrangement   superimposed(const GarmentMesh& piece, const GarmentMesh& partner,
                                    const PieceArrangement& partner_arrangement, const QVector<SewnSides>& seams,
                                    Superimpose how) const;
    QVector3D          mirrored(const QVector3D& point) const;

    const QVector<ArrangementPoint>& points() const;
    int                pointNamed(const QString& name) const;

    static QString     partName(BodyPart part);
    static BodyPart    partFromName(const QString& name);

private:
    // A skin vertex of an arm below the armpit: which it is, where along the arm's middle line it is, and how far from
    // it.
    struct ArmSkin
    {
        int   vertex = 0;
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
    QVector<bool>      m_skin_on_arm;       // for each skin vertex whether it is close to an arm's bones
    QVector<ArrangementPoint> m_points;

    QVector<QVector3D> placeFlat(QVector<QPointF> flat, const PieceArrangement& arrangement, qreal out,
                                 PieceFrame* frame) const;
    QVector<QVector3D> placeUpright(const QVector<QPointF>& flat, const PieceArrangement& arrangement,
                                    qreal out) const;
    QVector3D          axisAt(BodyPart part, qreal height) const;
    qreal              radiusAround(BodyPart part, const QVector3D& axis, qreal from, qreal to) const;
    bool               onArm(const QVector3D& point) const;
    bool               onPartsSide(BodyPart part, const QVector3D& point) const;
    BodyPart           nearestPart(const QVector3D& point) const;

    void               findArmSkin(int side, const QVector<QVector<int>>& neighbours);
    int                armAt(const QVector3D& point) const;
    qreal              armRadius(int side, qreal from, qreal to) const;
    QVector<QVector3D> placeOnArm(const QVector<QPointF>& flat, const PieceArrangement& arrangement, qreal out) const;

    void               findPoints(const BodyModel& model, const QVector<QVector3D>& positions);
    void               addUprightPoints(BodyPart part, const QString& level, qreal height,
                                        const QVector<QPair<QString, qreal>>& sides);
    void               addArmPoints(int side, const QString& level, qreal along, qreal outside);
    qreal              armAngle(int side, qreal along, const QVector3D& towards) const;
    qreal              armReach(int side, qreal along, qreal angle) const;
    qreal              radiusAt(BodyPart part, const QVector3D& point) const;
    QVector3D          outAt(BodyPart part, const QVector3D& point) const;

    static int         armSide(BodyPart part);
    static QVector<QPointF> arranged(const GarmentMesh& mesh, const PieceArrangement& arrangement);
};

#endif // BODY_WRAP_H
