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

#include <QPolygonF>
#include <QRectF>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

#include "body_measurer.h"

namespace
{
// How far a placed piece starts out from the body, in cm.
const qreal clearance = 2.0;

// Skin this close to an arm's bones, from the shoulder down to the finger tips, in cm, belongs to the arm, which
// pieces aren't wrapped around.
const float arm_reach = 7.0f;

// A part with no skin at a piece's height still gets a cylinder of this radius, in cm.
const qreal fallback_radius = 10.0;

// No piece wraps around its part further than this share of the way, so its sides don't overlap.
const qreal widest_wrap = 0.9;

// An arm's middle line bends in an arc from this many cm before to this many after the elbow.
const qreal elbow_bend = 10.0;

// An arm's middle line goes on this many cm above the shoulder joint, and below the wrist, past the finger tips.
const qreal above_shoulder = 10.0;
const qreal below_wrist = 25.0;

// How far around a place on an arm, in cm, the arm's thickness and a piece's width count for a tube there.
const int arm_window = 3;

// A tube around an arm narrows by at most this many cm per cm along it: the faster it narrows, the more the sides of
// a piece around it are sheared.
const qreal arm_narrowing = 0.1;

// Where an arm parts from the body is searched for down to this share of the upper arm, to within this many cm.
const qreal armpit_reach = 1.0;
const qreal armpit_precision = 0.25;

// Skin closer than this share of the shoulder joint's distance to the middle of the body is the torso's.
const float torso_middle = 0.5f;

// Arrangement points around the body or a leg sit where a tape around it at their height would: on the outline of
// the skin up to this many cm above and below them, bridging its hollows.
const qreal point_band = 2.0;

// The calf's arrangement points are this share of the way down from the knee to the ankle.
const qreal calf_share = 0.3;

// An arm's arrangement points sit as far from its middle line as its skin reaches up to this many cm up and down the
// arm from them, and up to this many degrees around it either way.
const qreal arm_point_reach = 2.0;
const qreal arm_point_spread = 20.0;

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

//---------------------------------------------------------------------------------------------------------------------
// The radius of a cylinder a piece this wide wraps around without its sides overlapping.
qreal widestWrapRadius(qreal width)
{
    return width / (2.0 * M_PI * widest_wrap);
}

//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF& origin, const QPointF& a, const QPointF& b)
{
    return (a.x() - origin.x()) * (b.y() - origin.y()) - (a.y() - origin.y()) * (b.x() - origin.x());
}

//---------------------------------------------------------------------------------------------------------------------
// The convex hull of the points, anticlockwise (Andrew's monotone chain).
QVector<QPointF> convexHull(QVector<QPointF> points)
{
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b)
    {
        return a.x() < b.x() || (!(b.x() < a.x()) && a.y() < b.y());
    });
    if (points.size() < 3)
    {
        return points;
    }

    QVector<QPointF> hull(2 * points.size());
    int count = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        while (count >= 2 && cross(hull.at(count - 2), hull.at(count - 1), points.at(i)) <= 0)
        {
            --count;
        }
        hull[count++] = points.at(i);
    }
    const int lower = count + 1;
    for (int i = points.size() - 2; i >= 0; --i)
    {
        while (count >= lower && cross(hull.at(count - 2), hull.at(count - 1), points.at(i)) <= 0)
        {
            --count;
        }
        hull[count++] = points.at(i);
    }
    hull.resize(count - 1);
    return hull;
}

//---------------------------------------------------------------------------------------------------------------------
// How far a ray from a point inside a convex outline runs before it leaves it; 0 if it doesn't meet it.
qreal exitDistance(const QVector<QPointF>& outline, const QPointF& from, const QPointF& direction)
{
    qreal farthest = 0;
    for (int i = 0; i < outline.size(); ++i)
    {
        const QPointF to_edge = outline.at(i) - from;
        const QPointF edge = outline.at((i + 1) % outline.size()) - outline.at(i);
        const qreal across = direction.x() * edge.y() - direction.y() * edge.x();
        if (qAbs(across) > 1e-12)
        {
            const qreal along_ray = (to_edge.x() * edge.y() - to_edge.y() * edge.x()) / across;
            const qreal along_edge = (to_edge.x() * direction.y() - to_edge.y() * direction.x()) / across;
            if (along_ray > 0 && along_edge >= 0 && along_edge <= 1)
            {
                farthest = qMax(farthest, along_ray);
            }
        }
    }
    return farthest;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar as fitted: the model and its vertex positions.
BodyWrap::BodyWrap(const BodyModel& model, const QVector<QVector3D>& positions)
    : m_skin(positions.mid(0, model.skinVertexCount()))
    , m_pelvis(model.joint(positions, QStringLiteral("pelvis")))
    , m_crotch(0)
    , m_armpits{0, 0}
    , m_shoulder_tips{0, 0}
    , m_skin_arms(m_skin.size(), -1)
    , m_skin_on_arm()
    , m_points()
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
        m_arm_lines[side] = LimbLine({m_arms[side][0], m_arms[side][1], m_arms[side][2]}, elbow_bend, above_shoulder,
                                     below_wrist);
    }
    const QVector3D shoulder_tip = BodyMeasurer(model).shoulderTip(positions);
    m_shoulder_tips[0] = m_arm_lines[0].alongNearest(shoulder_tip);
    m_shoulder_tips[1] = m_arm_lines[1].alongNearest(mirrored(shoulder_tip));

    m_crotch = model.crotch(positions).y();

    QVector<QVector<int>> neighbours(m_skin.size());
    const QVector<quint32>& triangles = model.triangles();
    for (int t = 0; t + 2 < triangles.size(); t += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const int a = static_cast<int>(triangles.at(t + k));
            const int b = static_cast<int>(triangles.at(t + (k + 1) % 3));
            if (a < m_skin.size() && b < m_skin.size())
            {
                neighbours[a].append(b);
                neighbours[b].append(a);
            }
        }
    }
    findArmSkin(0, neighbours);
    findArmSkin(1, neighbours);

    m_skin_on_arm.reserve(m_skin.size());
    for (const QVector3D& point : m_skin)
    {
        m_skin_on_arm.append(onArm(point));
    }
    findPoints(model, positions);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes if it is put at this point of the body: around the arm if the point is on an arm below
/// the armpit, around the leg below the crotch if the point is nearer to it, around the body otherwise.
PieceArrangement BodyWrap::arrangementAt(const QVector3D& point) const
{
    const int arm = armAt(point);
    return arrangementOn(arm == 0 ? BodyPart::LeftArm : (arm == 1 ? BodyPart::RightArm : nearestPart(point)), point);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes on a given part of the body if it is put at this point, wherever the point is: around an
/// arm by where the point is along and around the arm's middle line, around the body or a leg by its height and its
/// angle around the part's axis.
PieceArrangement BodyWrap::arrangementOn(BodyPart part, const QVector3D& point) const
{
    PieceArrangement arrangement;
    arrangement.part = part;
    const int arm = armSide(part);
    if (arm >= 0)
    {
        const LimbLine& line = m_arm_lines[arm];
        const qreal along = line.alongNearest(point);
        const QVector3D centre = line.pointAt(along);
        const QVector3D out = point - centre;
        const QVector3D front = line.frontAt(along);
        const QVector3D quarter = QVector3D::crossProduct(front, line.directionAt(along));
        arrangement.height = centre.y();
        arrangement.angle = qRadiansToDegrees(qAtan2(QVector3D::dotProduct(out, quarter),
                                                     QVector3D::dotProduct(out, front)));
    }
    else
    {
        arrangement.height = point.y();

        const QVector3D axis = axisAt(part, point.y());
        arrangement.angle = qRadiansToDegrees(qAtan2(point.x() - axis.x(), point.z() - axis.z()));
    }
    return arrangement;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where an arrangement puts a piece on this avatar: if it was put at an arrangement point, where that point
/// is on this avatar, otherwise where it says.
PieceArrangement BodyWrap::resolved(const PieceArrangement& arrangement) const
{
    PieceArrangement resolved = arrangement;
    const int index = pointNamed(arrangement.point);
    if (index >= 0)
    {
        const PieceArrangement& at = m_points.at(index).arrangement;
        resolved.part = at.part;
        resolved.angle = at.angle;
        resolved.height = at.height;
    }
    return resolved;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the mesh's vertices start out: the piece, turned over and rotated as the arrangement says, wrapped
/// around the part at the arrangement's height, its middle at the arrangement's angle. Its horizontal lines run around
/// the part, its vertical lines down it, and distances along it are kept. Put further out by `out` cm, it shows in
/// front of pieces already put there, as a preview does.
QVector<QVector3D> BodyWrap::place(const GarmentMesh& mesh, const PieceArrangement& arrangement, qreal out) const
{
    const QVector<QPointF> flat = arranged(mesh, arrangement);
    return armSide(arrangement.part) >= 0 ? placeOnArm(flat, arrangement, out)
                                          : placeUpright(flat, arrangement, out);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The point mirrored to the other side of the body, across the upright plane through the middle of the
/// pelvis; where a piece's mirrored copy goes.
QVector3D BodyWrap::mirrored(const QVector3D& point) const
{
    return QVector3D(2.0f * m_pelvis.x() - point.x(), point.y(), point.z());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The arrangement points: around the body, then around the left and the right leg, then around the left and
/// the right arm.
const QVector<ArrangementPoint>& BodyWrap::points() const
{
    return m_points;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Which of the arrangement points has that name; -1 for none.
int BodyWrap::pointNamed(const QString& name) const
{
    int found = -1;
    for (int i = 0; i < m_points.size() && found < 0 && !name.isEmpty(); ++i)
    {
        found = m_points.at(i).name == name ? i : -1;
    }
    return found;
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
    else if (part == BodyPart::LeftArm)
    {
        name = QStringLiteral("leftArm");
    }
    else if (part == BodyPart::RightArm)
    {
        name = QStringLiteral("rightArm");
    }
    return name;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The body part written in the pattern file; anything unknown is the body.
BodyPart BodyWrap::partFromName(const QString& name)
{
    BodyPart part = BodyPart::Body;
    for (const BodyPart named : {BodyPart::LeftLeg, BodyPart::RightLeg, BodyPart::LeftArm, BodyPart::RightArm})
    {
        if (name == partName(named))
        {
            part = named;
        }
    }
    return part;
}

//---------------------------------------------------------------------------------------------------------------------
// Wraps the piece's flat points around an upright cylinder on the body's or the leg's axis, its vertical lines straight
// down. The cylinder is wide enough to clear the part over the piece's height, even where a leg leans away from its
// axis.
QVector<QVector3D> BodyWrap::placeUpright(const QVector<QPointF>& flat, const PieceArrangement& arrangement,
                                          qreal out) const
{
    const QRectF bounds = QPolygonF(flat).boundingRect();
    const QPointF middle = bounds.center();
    const qreal half_height = bounds.height() / 2.0;
    const QVector3D axis = axisAt(arrangement.part, arrangement.height);
    const qreal radius = qMax(radiusAround(arrangement.part, axis, arrangement.height - half_height,
                                           arrangement.height + half_height) + clearance,
                              widestWrapRadius(bounds.width())) + out;
    const qreal start = qDegreesToRadians(arrangement.angle);

    QVector<QVector3D> placed;
    placed.reserve(flat.size());
    for (const QPointF& point : flat)
    {
        const qreal height = arrangement.height - (point.y() - middle.y());
        const qreal around = start + (point.x() - middle.x()) / radius;
        placed.append(QVector3D(static_cast<float>(axis.x() + radius * qSin(around)), static_cast<float>(height),
                                static_cast<float>(axis.z() + radius * qCos(around))));
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
// The point of the part's axis at a height: the body's runs straight up through the pelvis, a leg's from the hip
// through the knee to the ankle.
QVector3D BodyWrap::axisAt(BodyPart part, qreal height) const
{
    QVector3D axis(m_pelvis.x(), static_cast<float>(height), m_pelvis.z());
    if (part == BodyPart::LeftLeg || part == BodyPart::RightLeg)
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
    qreal radius = 0;
    for (int i = 0; i < m_skin.size(); ++i)
    {
        const QVector3D& point = m_skin.at(i);
        if (point.y() >= from && point.y() <= to && !m_skin_on_arm.at(i) && onPartsSide(part, point))
        {
            radius = qMax(radius, horizontalDistance(point, axis));
        }
    }
    return radius > 0 ? radius : fallback_radius;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether skin is the part's to wrap around: all skin for the body, the skin on its side of the body for a leg.
bool BodyWrap::onPartsSide(BodyPart part, const QVector3D& point) const
{
    const float side = part == BodyPart::Body ? 0.0f
                                              : m_legs[part == BodyPart::LeftLeg ? 0 : 1][0].x() - m_pelvis.x();
    return part == BodyPart::Body || (point.x() - m_pelvis.x()) * side >= 0;
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

//---------------------------------------------------------------------------------------------------------------------
// Finds the skin of an arm below the armpit: all skin connected to the elbow that lies further down the arm than
// where the arm parts from the body. That is found by moving a cut down the arm until the skin below it no longer
// reaches the middle of the body.
void BodyWrap::findArmSkin(int side, const QVector<QVector<int>>& neighbours)
{
    const LimbLine& line = m_arm_lines[side];
    QVector<ArmSkin> skin(m_skin.size());
    int elbow = -1;
    float elbow_distance = std::numeric_limits<float>::infinity();
    for (int i = 0; i < m_skin.size(); ++i)
    {
        qreal distance = 0;
        skin[i].vertex = i;
        skin[i].along = static_cast<float>(line.alongNearest(m_skin.at(i), &distance));
        skin[i].distance = static_cast<float>(distance);
        const float to_elbow = (m_skin.at(i) - m_arms[side][1]).lengthSquared();
        if (to_elbow < elbow_distance)
        {
            elbow = i;
            elbow_distance = to_elbow;
        }
    }

    const float middle = torso_middle * qAbs(m_arms[side][0].x() - m_pelvis.x());
    auto below = [&](qreal cut, bool* reaches_body)
    {
        QVector<int> found;
        QVector<bool> seen(m_skin.size(), false);
        std::queue<int> next;
        *reaches_body = false;
        if (elbow >= 0 && skin.at(elbow).along > cut)
        {
            next.push(elbow);
            seen[elbow] = true;
        }
        while (!next.empty())
        {
            const int vertex = next.front();
            next.pop();
            found.append(vertex);
            *reaches_body = *reaches_body || qAbs(m_skin.at(vertex).x() - m_pelvis.x()) < middle;
            for (const int neighbour : neighbours.at(vertex))
            {
                if (!seen.at(neighbour) && skin.at(neighbour).along > cut)
                {
                    seen[neighbour] = true;
                    next.push(neighbour);
                }
            }
        }
        return found;
    };

    // Moving the cut down only ever takes skin away, so the armpit is where the skin below stops reaching the body.
    qreal attached = 0;
    qreal parted = armpit_reach * (m_arms[side][1] - m_arms[side][0]).length();
    bool reaches_body = false;
    below(parted, &reaches_body);
    if (!reaches_body)
    {
        while (parted - attached > armpit_precision)
        {
            const qreal cut = (attached + parted) / 2.0;
            below(cut, &reaches_body);
            if (reaches_body)
            {
                attached = cut;
            }
            else
            {
                parted = cut;
            }
        }
        m_armpits[side] = parted;
        for (const int vertex : below(parted, &reaches_body))
        {
            m_arm_skin[side].append(skin.at(vertex));
            m_skin_arms[vertex] = static_cast<qint8>(side);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The arm whose skin below the armpit the point is nearest to, if no other skin is nearer; -1 otherwise.
int BodyWrap::armAt(const QVector3D& point) const
{
    int nearest = -1;
    float nearest_distance = std::numeric_limits<float>::infinity();
    for (int i = 0; i < m_skin.size(); ++i)
    {
        const float distance = (m_skin.at(i) - point).lengthSquared();
        if (distance < nearest_distance)
        {
            nearest = i;
            nearest_distance = distance;
        }
    }
    return nearest >= 0 ? m_skin_arms.at(nearest) : -1;
}

//---------------------------------------------------------------------------------------------------------------------
// How far an arm's skin reaches out from its middle line between two places along it. Above the armpit, where the
// arm and the body are one, the arm is taken to be as thick as just below the armpit. The hand is left out: a sleeve
// long enough to reach over it starts out with the hand through it, as when it is put on, rather than ballooning
// out around it.
qreal BodyWrap::armRadius(int side, qreal from, qreal to) const
{
    const qreal armpit = m_armpits[side];
    const qreal wrist = qMax(m_arm_lines[side].length(), armpit + clearance);
    from = qBound(armpit, from, wrist - clearance);
    to = qBound(from + clearance, to, wrist);

    qreal radius = 0;
    for (const ArmSkin& skin : m_arm_skin[side])
    {
        if (skin.along >= from && skin.along <= to)
        {
            radius = qMax(radius, static_cast<qreal>(skin.distance));
        }
    }
    return radius > 0 ? radius : fallback_radius;
}

//---------------------------------------------------------------------------------------------------------------------
// Wraps the piece's flat points around a tube along the arm's middle line, its middle at the arrangement's place on the
// line and angle around it, its top no further up the arm than the shoulder tip.
//
// The tube narrows down the arm as the arm does, though slowly, staying clear of the arm near each place along it
// and wide enough there for the piece to go around without its sides overlapping. So a sleeve narrowing to the wrist
// wraps most of the way around the arm all the way down, and its underarm seam closes under the arm, not across it.
QVector<QVector3D> BodyWrap::placeOnArm(const QVector<QPointF>& flat, const PieceArrangement& arrangement,
                                        qreal out) const
{
    const int side = armSide(arrangement.part);
    const LimbLine& line = m_arm_lines[side];
    const QRectF bounds = QPolygonF(flat).boundingRect();
    const QPointF middle = bounds.center();
    const qreal top = qMax(line.alongAtHeight(arrangement.height) - bounds.height() / 2.0, m_shoulder_tips[side]);

    // The radius every cm down the piece.
    const int rows = qCeil(bounds.height()) + 2;
    QVector<qreal> widths(rows, 0);
    for (const QPointF& point : flat)
    {
        const int row = qBound(0, qRound(point.y() - bounds.top()), rows - 1);
        widths[row] = qMax(widths.at(row), 2.0 * qAbs(point.x() - middle.x()));
    }
    QVector<qreal> radii(rows, 0);
    for (int row = 0; row < rows; ++row)
    {
        qreal width = 0;
        for (int nearby = qMax(0, row - arm_window); nearby <= qMin(rows - 1, row + arm_window); ++nearby)
        {
            width = qMax(width, widths.at(nearby));
        }
        radii[row] = qMax(armRadius(side, top + row - arm_window, top + row + arm_window) + clearance,
                          widestWrapRadius(width));
    }
    for (int row = 1; row < rows; ++row)
    {
        radii[row] = qMax(radii.at(row), radii.at(row - 1) - arm_narrowing);
    }
    for (int row = rows - 2; row >= 0; --row)
    {
        radii[row] = qMax(radii.at(row), radii.at(row + 1) - arm_narrowing);
    }

    QVector<QVector3D> placed;
    placed.reserve(flat.size());
    for (const QPointF& point : flat)
    {
        const qreal down = point.y() - bounds.top();
        const int row = qBound(0, qFloor(down), rows - 2);
        const qreal t = qBound(0.0, down - row, 1.0);
        const qreal radius = radii.at(row) * (1.0 - t) + radii.at(row + 1) * t + out;
        const qreal along = top + down;
        const qreal angle = arrangement.angle + qRadiansToDegrees((point.x() - middle.x()) / radius);
        placed.append(line.pointAt(along) + line.aroundAt(along, angle) * static_cast<float>(radius));
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
// Finds the arrangement points: around the body at the neck, the bust, the waist and the hip, where they are measured,
// and halfway down the thighs; around each leg halfway down the thigh, at the knee and on the calf; around each arm
// halfway down the upper arm, at the elbow and at the wrist.
void BodyWrap::findPoints(const BodyModel& model, const QVector<QVector3D>& positions)
{
    const BodyMeasurer measurer(model);

    // The avatar's left, the way its left leg is.
    const qreal left = m_legs[0][0].x() > m_pelvis.x() ? 90.0 : -90.0;
    const QVector<QPair<QString, qreal>> around_body = {
        {QStringLiteral("front"), 0.0},
        {QStringLiteral("frontLeft"), left / 2.0},
        {QStringLiteral("left"), left},
        {QStringLiteral("backLeft"), std::remainder(180.0 - left / 2.0, 360.0)},
        {QStringLiteral("back"), 180.0},
        {QStringLiteral("backRight"), std::remainder(180.0 + left / 2.0, 360.0)},
        {QStringLiteral("right"), -left},
        {QStringLiteral("frontRight"), -left / 2.0}};
    const qreal knees = (m_legs[0][1].y() + m_legs[1][1].y()) / 2.0;
    addUprightPoints(BodyPart::Body, QStringLiteral("neck"), measurer.neckLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("bust"), measurer.bustLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("waist"), measurer.waistLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("hip"), measurer.hipLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("thigh"), (m_crotch + knees) / 2.0, around_body);

    for (int side = 0; side < 2; ++side)
    {
        const BodyPart leg = side == 0 ? BodyPart::LeftLeg : BodyPart::RightLeg;
        const qreal outside = side == 0 ? left : -left;
        const QVector<QPair<QString, qreal>> around_leg = {
            {QStringLiteral("front"), 0.0},
            {QStringLiteral("outside"), outside},
            {QStringLiteral("back"), 180.0},
            {QStringLiteral("inside"), -outside}};
        const qreal knee = m_legs[side][1].y();
        addUprightPoints(leg, QStringLiteral("thigh"), (m_crotch + knee) / 2.0, around_leg);
        addUprightPoints(leg, QStringLiteral("knee"), knee, around_leg);
        addUprightPoints(leg, QStringLiteral("calf"), knee - calf_share * (knee - m_legs[side][2].y()), around_leg);
    }

    // An arm's outside faces up at the upper arm, and goes on down the arm at the same angle around it, as the middle
    // of a sleeve does.
    for (int side = 0; side < 2; ++side)
    {
        const qreal elbow = m_arm_lines[side].alongNearest(m_arms[side][1]);
        const qreal upper_arm = (m_shoulder_tips[side] + elbow) / 2.0;
        const qreal outside = armAngle(side, upper_arm, QVector3D(0, 1, 0));
        addArmPoints(side, QStringLiteral("upperArm"), upper_arm, outside);
        addArmPoints(side, QStringLiteral("elbow"), elbow, outside);
        addArmPoints(side, QStringLiteral("wrist"), m_arm_lines[side].alongNearest(m_arms[side][2]), outside);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Adds arrangement points around the body or a leg at a height, at the angles of its sides, where a tape around the
// part there would lie.
void BodyWrap::addUprightPoints(BodyPart part, const QString& level, qreal height,
                                const QVector<QPair<QString, qreal>>& sides)
{
    const QVector3D axis = axisAt(part, height);
    QVector<QPointF> slice;
    for (int i = 0; i < m_skin.size(); ++i)
    {
        const QVector3D& point = m_skin.at(i);
        if (qAbs(point.y() - height) <= point_band && !m_skin_on_arm.at(i) && onPartsSide(part, point))
        {
            slice.append(QPointF(point.x() - axis.x(), point.z() - axis.z()));
        }
    }
    const QVector<QPointF> outline = convexHull(slice);

    for (const QPair<QString, qreal>& side : sides)
    {
        const qreal radians = qDegreesToRadians(side.second);
        const QPointF direction(qSin(radians), qCos(radians));
        const qreal reach = outline.size() >= 3 ? exitDistance(outline, QPointF(), direction) : 0;

        ArrangementPoint point;
        point.name = partName(part) + QLatin1Char('-') + level + QLatin1Char('-') + side.first;
        point.level = level;
        point.side = side.first;
        point.arrangement.part = part;
        point.arrangement.angle = side.second;
        point.arrangement.height = height;
        point.arrangement.point = point.name;
        point.normal = QVector3D(static_cast<float>(direction.x()), 0.0f, static_cast<float>(direction.y()));
        point.position = QVector3D(axis.x(), static_cast<float>(height), axis.z())
                         + point.normal * static_cast<float>(reach > 0 ? reach : fallback_radius);
        m_points.append(point);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Adds arrangement points around an arm at a place along it: in front, on its outside, behind and on its inside, each
// as far from the arm's middle line as the arm reaches there.
void BodyWrap::addArmPoints(int side, const QString& level, qreal along, qreal outside)
{
    const LimbLine& line = m_arm_lines[side];
    const QVector<QPair<QString, qreal>> around_arm = {
        {QStringLiteral("front"), 0.0},
        {QStringLiteral("outside"), outside},
        {QStringLiteral("back"), 180.0},
        {QStringLiteral("inside"), std::remainder(outside + 180.0, 360.0)}};
    const BodyPart part = side == 0 ? BodyPart::LeftArm : BodyPart::RightArm;

    for (const QPair<QString, qreal>& around : around_arm)
    {
        const float reach = static_cast<float>(armReach(side, along, around.second));
        ArrangementPoint point;
        point.name = partName(part) + QLatin1Char('-') + level + QLatin1Char('-') + around.first;
        point.level = level;
        point.side = around.first;
        point.arrangement.part = part;
        point.arrangement.angle = around.second;
        point.arrangement.height = line.pointAt(along).y();
        point.arrangement.point = point.name;
        point.normal = line.aroundAt(along, around.second);
        point.position = line.pointAt(along) + point.normal * reach;
        m_points.append(point);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The angle around an arm, at a place along it, that faces the most towards a direction.
qreal BodyWrap::armAngle(int side, qreal along, const QVector3D& towards) const
{
    const LimbLine& line = m_arm_lines[side];
    const QVector3D front = line.frontAt(along);
    const QVector3D quarter = QVector3D::crossProduct(front, line.directionAt(along));
    return qRadiansToDegrees(qAtan2(QVector3D::dotProduct(towards, quarter), QVector3D::dotProduct(towards, front)));
}

//---------------------------------------------------------------------------------------------------------------------
// How far an arm's skin reaches out from its middle line around a place along it, towards an angle around it; the
// hand is left out.
qreal BodyWrap::armReach(int side, qreal along, qreal angle) const
{
    const qreal from = along - arm_point_reach;
    const qreal to = qMin(along + arm_point_reach, m_arm_lines[side].length());
    qreal reach = 0;
    for (const ArmSkin& skin : m_arm_skin[side])
    {
        if (skin.along >= from && skin.along <= to && skin.distance > reach)
        {
            const qreal towards = armAngle(side, skin.along, m_skin.at(skin.vertex)
                                                             - m_arm_lines[side].pointAt(skin.along));
            if (qAbs(std::remainder(towards - angle, 360.0)) <= arm_point_spread)
            {
                reach = skin.distance;
            }
        }
    }
    return reach > 0 ? reach : armRadius(side, from, to);
}

//---------------------------------------------------------------------------------------------------------------------
// 0 for the left arm, 1 for the right, -1 for other parts.
int BodyWrap::armSide(BodyPart part)
{
    int side = -1;
    if (part == BodyPart::LeftArm)
    {
        side = 0;
    }
    else if (part == BodyPart::RightArm)
    {
        side = 1;
    }
    return side;
}

//---------------------------------------------------------------------------------------------------------------------
// The piece's flat points the way round it is put on the avatar: turned over and rotated about its middle, as seen
// from outside.
QVector<QPointF> BodyWrap::arranged(const GarmentMesh& mesh, const PieceArrangement& arrangement)
{
    const QPointF middle = mesh.bounds().center();
    const qreal radians = qDegreesToRadians(arrangement.rotation);
    const qreal cosine = qCos(radians);
    const qreal sine = qSin(radians);

    QVector<QPointF> flat;
    flat.reserve(mesh.rest_positions.size());
    for (const QPointF& rest : mesh.rest_positions)
    {
        const qreal x = arrangement.turned_over ? middle.x() - rest.x() : rest.x() - middle.x();
        const qreal y = rest.y() - middle.y();

        // The piece scene's y axis points down, so this turns the piece clockwise as it is seen.
        flat.append(QPointF(middle.x() + x * cosine - y * sine, middle.y() + x * sine + y * cosine));
    }
    return flat;
}
