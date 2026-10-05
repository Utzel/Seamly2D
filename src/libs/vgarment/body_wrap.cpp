//---------------------------------------------------------------------------------------------------------------------
//  @file   body_wrap.cpp
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

#include "body_wrap.h"

#include <QRectF>
#include <QtMath>

#include <limits>

namespace
{
// How far a placed piece starts out from the body, in cm.
const qreal clearance = 2.0;

// Skin this close to an arm's bones, from the shoulder down to the finger tips, in cm, belongs to the arm, which
// pieces aren't wrapped around.
const float arm_reach = 7.0f;

// Skin this close to the middle of the body, in cm, is where the legs part.
const float crotch_width = 1.5f;

// A part with no skin at a piece's height still gets a cylinder of this radius, in cm.
const qreal fallback_radius = 10.0;

//---------------------------------------------------------------------------------------------------------------------
float distanceToSegment(const QVector3D& point, const QVector3D& a, const QVector3D& b)
{
    const QVector3D ab = b - a;
    const float length_squared = ab.lengthSquared();
    const float t = length_squared > 0 ? qBound(0.0f, QVector3D::dotProduct(point - a, ab) / length_squared, 1.0f)
                                       : 0.0f;
    return (point - (a + ab * t)).length();
}

//---------------------------------------------------------------------------------------------------------------------
qreal horizontalDistance(const QVector3D& a, const QVector3D& b)
{
    return qSqrt(qPow(a.x() - b.x(), 2) + qPow(a.z() - b.z(), 2));
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar as fitted: the model and its vertex positions.
BodyWrap::BodyWrap(const BodyModel& model, const QVector<QVector3D>& positions)
    : m_skin(positions.mid(0, model.skinVertexCount()))
    , m_pelvis(model.joint(positions, QStringLiteral("pelvis")))
    , m_crotch(0)
{
    const QString sides[2] = {QStringLiteral("l-"), QStringLiteral("r-")};
    for (int side = 0; side < 2; ++side)
    {
        m_legs[side][0] = model.joint(positions, sides[side] + QStringLiteral("upper-leg"));
        m_legs[side][1] = model.joint(positions, sides[side] + QStringLiteral("knee"));
        m_legs[side][2] = model.joint(positions, sides[side] + QStringLiteral("ankle"));
        m_arms[side][0] = model.joint(positions, sides[side] + QStringLiteral("shoulder"));
        m_arms[side][1] = model.joint(positions, sides[side] + QStringLiteral("elbow"));
        m_arms[side][2] = model.joint(positions, sides[side] + QStringLiteral("hand"));
        m_arms[side][3] = model.joint(positions, sides[side] + QStringLiteral("finger-3-4"));
        m_arms[side][4] = model.joint(positions, sides[side] + QStringLiteral("finger-1-4"));
        m_arms[side][5] = model.joint(positions, sides[side] + QStringLiteral("finger-5-4"));
    }

    // The legs part at the lowest skin in the middle of the body below the pelvis.
    m_crotch = m_pelvis.y();
    for (const QVector3D& point : m_skin)
    {
        if (qAbs(point.x() - m_pelvis.x()) < crotch_width && point.y() < m_pelvis.y() && point.y() > m_legs[0][1].y())
        {
            m_crotch = qMin(m_crotch, static_cast<qreal>(point.y()));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes if it is put at this point of the body: around the leg below the crotch if the point
/// is nearer to it, around the body otherwise.
PieceArrangement BodyWrap::arrangementAt(const QVector3D& point) const
{
    PieceArrangement arrangement;
    arrangement.part = nearestPart(point);
    arrangement.height = point.y();

    const QVector3D axis = axisAt(arrangement.part, point.y());
    arrangement.angle = qRadiansToDegrees(qAtan2(point.x() - axis.x(), point.z() - axis.z()));
    return arrangement;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the mesh's vertices start out: the piece wrapped around an upright cylinder on its part's axis at the
/// arrangement's height, its middle at the arrangement's angle. Its horizontal lines run around the part, its
/// vertical lines straight down, and distances along it are kept. The cylinder is wide enough to clear the part over
/// the piece's height, even where a leg leans away from its axis.
QVector<QVector3D> BodyWrap::place(const GarmentMesh& mesh, const PieceArrangement& arrangement) const
{
    const QRectF bounds = mesh.bounds();
    const QPointF middle = bounds.center();
    const qreal half_height = bounds.height() / 2.0;
    const QVector3D axis = axisAt(arrangement.part, arrangement.height);
    const qreal radius = radiusAround(arrangement.part, axis, arrangement.height - half_height,
                                      arrangement.height + half_height) + clearance;
    const qreal start = qDegreesToRadians(arrangement.angle);

    QVector<QVector3D> placed;
    placed.reserve(mesh.vertexCount());
    for (const QPointF& point : mesh.rest_positions)
    {
        const qreal height = arrangement.height - (point.y() - middle.y());
        const qreal around = start + (point.x() - middle.x()) / radius;
        placed.append(QVector3D(static_cast<float>(axis.x() + radius * qSin(around)), static_cast<float>(height),
                                static_cast<float>(axis.z() + radius * qCos(around))));
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How a body part is written in the pattern file.
QString BodyWrap::partName(BodyPart part)
{
    QString name = QStringLiteral("body");
    if (part == BodyPart::LeftLeg)
    {
        name = QStringLiteral("leftLeg");
    }
    else if (part == BodyPart::RightLeg)
    {
        name = QStringLiteral("rightLeg");
    }
    return name;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The body part written in the pattern file; anything unknown is the body.
BodyPart BodyWrap::partFromName(const QString& name)
{
    BodyPart part = BodyPart::Body;
    if (name == partName(BodyPart::LeftLeg))
    {
        part = BodyPart::LeftLeg;
    }
    else if (name == partName(BodyPart::RightLeg))
    {
        part = BodyPart::RightLeg;
    }
    return part;
}

//---------------------------------------------------------------------------------------------------------------------
// The point of the part's axis at a height: the body's runs straight up through the pelvis, a leg's from the hip
// through the knee to the ankle.
QVector3D BodyWrap::axisAt(BodyPart part, qreal height) const
{
    QVector3D axis(m_pelvis.x(), static_cast<float>(height), m_pelvis.z());
    if (part != BodyPart::Body)
    {
        const QVector3D* leg = m_legs[part == BodyPart::LeftLeg ? 0 : 1];
        QVector3D on_leg = height >= leg[0].y() ? leg[0] : leg[2];
        for (int k = 0; k < 2; ++k)
        {
            if (height < leg[k].y() && height >= leg[k + 1].y())
            {
                const float t = static_cast<float>((leg[k].y() - height) / (leg[k].y() - leg[k + 1].y()));
                on_leg = leg[k] + (leg[k + 1] - leg[k]) * t;
            }
        }
        axis = QVector3D(on_leg.x(), static_cast<float>(height), on_leg.z());
    }
    return axis;
}

//---------------------------------------------------------------------------------------------------------------------
// How far the skin reaches out from an upright axis between two heights, the arms left out. For a leg that is all
// skin on its side of the body, so a piece reaching up to the hips or the crotch clears them as well.
qreal BodyWrap::radiusAround(BodyPart part, const QVector3D& axis, qreal from, qreal to) const
{
    const float side = part == BodyPart::Body ? 0.0f
                                              : m_legs[part == BodyPart::LeftLeg ? 0 : 1][0].x() - m_pelvis.x();
    qreal radius = 0;
    for (const QVector3D& point : m_skin)
    {
        if (point.y() >= from && point.y() <= to && !onArm(point))
        {
            const bool belongs = part == BodyPart::Body || (point.x() - m_pelvis.x()) * side >= 0;
            if (belongs)
            {
                radius = qMax(radius, horizontalDistance(point, axis));
            }
        }
    }
    return radius > 0 ? radius : fallback_radius;
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyWrap::onArm(const QVector3D& point) const
{
    // Upper arm, forearm, and from the wrist to the tips of the middle finger, the thumb and the little finger.
    const int bones[5][2] = {{0, 1}, {1, 2}, {2, 3}, {2, 4}, {2, 5}};

    bool on_arm = false;
    for (int side = 0; side < 2 && !on_arm; ++side)
    {
        const QVector3D* arm = m_arms[side];
        for (int bone = 0; bone < 5 && !on_arm; ++bone)
        {
            on_arm = distanceToSegment(point, arm[bones[bone][0]], arm[bones[bone][1]]) < arm_reach;
        }
    }
    return on_arm;
}

//---------------------------------------------------------------------------------------------------------------------
// The body, or below the crotch the leg, whose axis is nearest to the point.
BodyPart BodyWrap::nearestPart(const QVector3D& point) const
{
    BodyPart nearest = BodyPart::Body;
    qreal nearest_distance = horizontalDistance(point, axisAt(BodyPart::Body, point.y()));
    if (point.y() < m_crotch)
    {
        for (const BodyPart leg : {BodyPart::LeftLeg, BodyPart::RightLeg})
        {
            const qreal distance = horizontalDistance(point, axisAt(leg, point.y()));
            if (distance < nearest_distance)
            {
                nearest = leg;
                nearest_distance = distance;
            }
        }
    }
    return nearest;
}
