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

#include <QLineF>
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

// How far around a place on an arm or the neck, in cm, its thickness and a piece's width count for a tube there.
const int line_window = 3;

// A tube around an arm or the neck narrows by at most this many cm per cm along it: the faster it narrows, the more
// the sides of a piece around it are sheared.
const qreal line_narrowing = 0.1;

// The neck's middle line goes on this many cm above the head joint, and below the neck joint into the body, so a
// collar reaching down past the base of the neck still has a tube to go around.
const qreal above_head = 10.0;
const qreal below_neck = 15.0;

// Skin further from the neck's middle line than this many times as far as the neck's skin around its middle is from it
// belongs to the jaw or the shoulders, not to the neck.
const qreal neck_skin_reach = 1.5;

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

// The frame at a piece's middle is found from points this many cm to either side of the middle and above and below it.
const qreal frame_step = 0.5;

// Superimposed, a piece starts this many cm out from or in from the piece it is laid on, clear of it. Each side of a
// seam is matched at this many places along it, spread by length, and the piece is moved this many times to put its
// side of the seams where the other's is, and turned this many times to lay it along the other's.
const qreal layer_gap = 0.5;
const int seam_samples = 12;
const int superimpose_steps = 6;
const int superimpose_turns = 3;

// A piece put closer in than usual still starts this many cm off the body.
const qreal closest_clearance = 0.5;

// An arm's or the neck's arrangement points sit as far from its middle line as its skin reaches up to this many cm
// along it from them, and up to this many degrees around it either way.
const qreal line_point_reach = 2.0;
const qreal line_point_spread = 20.0;

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
// Where a place along a line through points is: the segment it is on and how far along that segment.
struct LinePlace
{
    int   segment = 0;
    qreal t = 0;
};

//---------------------------------------------------------------------------------------------------------------------
// Places at even shares of a line's length, from its start to its end.
QVector<LinePlace> evenPlaces(const QVector<QPointF>& line, int count)
{
    QVector<qreal> reached(line.size(), 0);
    for (int i = 1; i < line.size(); ++i)
    {
        reached[i] = reached.at(i - 1) + QLineF(line.at(i - 1), line.at(i)).length();
    }
    QVector<LinePlace> places;
    for (int k = 0; k < count && line.size() >= 2; ++k)
    {
        const qreal wanted = reached.last() * k / (count - 1);
        LinePlace place;
        while (place.segment < line.size() - 2 && reached.at(place.segment + 1) < wanted)
        {
            ++place.segment;
        }
        const qreal length = reached.at(place.segment + 1) - reached.at(place.segment);
        place.t = length > 0 ? qBound(0.0, (wanted - reached.at(place.segment)) / length, 1.0) : 0.0;
        places.append(place);
    }
    return places;
}

//---------------------------------------------------------------------------------------------------------------------
template <typename Point>
Point pointAt(const QVector<Point>& line, const LinePlace& place)
{
    return line.at(place.segment) * static_cast<float>(1.0 - place.t)
           + line.at(place.segment + 1) * static_cast<float>(place.t);
}

//---------------------------------------------------------------------------------------------------------------------
// The turn, in degrees as PieceArrangement::rotation takes it, that best lays points, first mirrored left to right if
// asked, on others about their middles, and how far off they then are, squared and summed.
qreal bestTurn(const QVector<QPointF>& from, const QVector<QPointF>& onto, bool mirrored, qreal* error)
{
    QPointF from_middle;
    QPointF onto_middle;
    for (int i = 0; i < from.size(); ++i)
    {
        from_middle += from.at(i) / from.size();
        onto_middle += onto.at(i) / onto.size();
    }
    auto relative = [&from_middle, mirrored](const QPointF& point)
    {
        const QPointF away = point - from_middle;
        return mirrored ? QPointF(-away.x(), away.y()) : away;
    };

    qreal along = 0;
    qreal across = 0;
    for (int i = 0; i < from.size(); ++i)
    {
        const QPointF p = relative(from.at(i));
        const QPointF q = onto.at(i) - onto_middle;
        along += p.x() * q.x() + p.y() * q.y();
        across += p.x() * q.y() - p.y() * q.x();
    }
    const qreal turn = qAtan2(across, along);
    *error = 0;
    for (int i = 0; i < from.size(); ++i)
    {
        const QPointF p = relative(from.at(i));
        const QPointF turned(p.x() * qCos(turn) - p.y() * qSin(turn), p.x() * qSin(turn) + p.y() * qCos(turn));
        const QPointF off = turned - (onto.at(i) - onto_middle);
        *error += off.x() * off.x() + off.y() * off.y();
    }
    return qRadiansToDegrees(turn);
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
    m_neck_line = LimbLine({model.joint(positions, QStringLiteral("head")),
                            model.joint(positions, QStringLiteral("neck"))}, 0, above_head, below_neck);
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
    findNeckSkin();
    findPoints(model, positions);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes if it is put at this point of the body: around the arm if the point is on an arm below
/// the armpit, around the neck if it is at the neck, around the leg below the crotch if the point is nearer to it,
/// around the body otherwise.
PieceArrangement BodyWrap::arrangementAt(const QVector3D& point) const
{
    const int arm = armAt(point);
    BodyPart part = arm == 0 ? BodyPart::LeftArm : (arm == 1 ? BodyPart::RightArm : nearestPart(point));
    if (arm < 0 && onNeck(point))
    {
        part = BodyPart::Neck;
    }
    return arrangementOn(part, point);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes on a given part of the body if it is put at this point, wherever the point is: around an
/// arm or the neck by where the point is along and around its middle line, around the body or a leg by its height and
/// its angle around the part's axis.
PieceArrangement BodyWrap::arrangementOn(BodyPart part, const QVector3D& point) const
{
    PieceArrangement arrangement;
    arrangement.part = part;
    if (const LimbLine* line = lineOf(part))
    {
        const qreal along = line->alongNearest(point);
        const QVector3D centre = line->pointAt(along);
        arrangement.height = centre.y();
        arrangement.angle = lineAngle(part, along, point - centre);
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
/// around the part at the arrangement's height and distance, its middle at the arrangement's angle, then leaning and
/// swinging out about its middle as it is. Its horizontal lines run around the part, its vertical lines down it, and
/// distances along it are kept. Put further out by `out` cm, it shows in front of pieces already put there, as a
/// preview does.
QVector<QVector3D> BodyWrap::place(const GarmentMesh& mesh, const PieceArrangement& arrangement, qreal out) const
{
    PieceFrame frame;
    QVector<QVector3D> placed = placeFlat(arranged(mesh, arrangement), arrangement, out, &frame);
    if (!qFuzzyIsNull(arrangement.lean) || !qFuzzyIsNull(arrangement.swing))
    {
        // Leaning turns its top towards the outside, swinging its side towards larger angles.
        const QQuaternion turn = QQuaternion::fromAxisAndAngle(-frame.up, static_cast<float>(arrangement.swing))
                                 * QQuaternion::fromAxisAndAngle(frame.across, static_cast<float>(arrangement.lean));
        for (QVector3D& point : placed)
        {
            point = frame.middle + turn.rotatedVector(point - frame.middle);
        }
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where a piece goes laid on the piece it is sewn to, as CLO's Superimpose does it: on the same part of the
/// body, its sides of the seams on the partner's, its body over or under the partner's, or beside it, edge to edge, a
/// little further out, in, or as far out.
///
/// The piece is turned in the flat, and mirrored if it has to be, which turns it over, so its sides of the seams lie
/// along the partner's as the partner is put on the avatar; over or under, its body lies on the partner's side of the
/// seams, beside, along the longest seam only, on the other side. Then it is moved around the part, up or down and out
/// or in until its sides of the seams are where the partner's are.
PieceArrangement BodyWrap::superimposed(const GarmentMesh& piece, const GarmentMesh& partner,
                                        const PieceArrangement& partner_arrangement, const QVector<SewnSides>& seams,
                                        Superimpose how) const
{
    const QVector<QPointF> partner_flat = arranged(partner, partner_arrangement);
    const QVector<QVector3D> partner_placed = place(partner, partner_arrangement);

    // Where the seams are matched, on each piece, the longest seam's side first.
    QVector<QVector<quint32>> piece_sides;
    QVector<QVector<quint32>> partner_sides;
    QVector<QVector<LinePlace>> piece_places;
    QVector<QVector<LinePlace>> partner_places;
    QVector<QPointF> piece_points;
    QVector<QPointF> partner_points;
    QPointF chord_start;
    QPointF chord_end;
    qreal longest = -1;

    // Beside its partner, a piece meets it edge to edge along one seam, the longest.
    auto partnerLength = [&partner_flat](const SewnSides& seam)
    {
        qreal length = 0;
        for (int i = 1; i < seam.partner.size(); ++i)
        {
            length += QLineF(partner_flat.value(static_cast<int>(seam.partner.at(i - 1))),
                             partner_flat.value(static_cast<int>(seam.partner.at(i)))).length();
        }
        return length;
    };
    QVector<SewnSides> used = seams;
    if (how == Superimpose::Side && !seams.isEmpty())
    {
        used = {*std::max_element(seams.cbegin(), seams.cend(), [&partnerLength](const SewnSides& a, const SewnSides& b)
        {
            return partnerLength(a) < partnerLength(b);
        })};
    }

    for (const SewnSides& seam : used)
    {
        if (seam.piece.size() < 2 || seam.partner.size() < 2)
        {
            continue;
        }
        QVector<QPointF> own_line;
        QVector<QPointF> partner_line;
        for (const quint32 vertex : seam.piece)
        {
            own_line.append(piece.rest_positions.value(static_cast<int>(vertex)));
        }
        for (const quint32 vertex : seam.partner)
        {
            partner_line.append(partner_flat.value(static_cast<int>(vertex)));
        }
        const QVector<LinePlace> own_places = evenPlaces(own_line, seam_samples);
        const QVector<LinePlace> other_places = evenPlaces(partner_line, seam_samples);
        for (int k = 0; k < seam_samples; ++k)
        {
            piece_points.append(pointAt(own_line, own_places.at(k)));
            partner_points.append(pointAt(partner_line, other_places.at(k)));
        }
        piece_sides.append(seam.piece);
        partner_sides.append(seam.partner);
        piece_places.append(own_places);
        partner_places.append(other_places);

        const qreal length = QLineF(partner_line.first(), partner_line.last()).length();
        if (length > longest)
        {
            longest = length;
            chord_start = partner_line.first();
            chord_end = partner_line.last();
        }
    }

    PieceArrangement arrangement = partner_arrangement;
    arrangement.point.clear();
    if (piece_points.isEmpty())
    {
        return arrangement;
    }

    // Which side of the seams a body lies on, as the partner is put on the avatar.
    const QPointF chord = chord_end - chord_start;
    auto sideOf = [&chord, &chord_start](const QPointF& point)
    {
        return chord.x() * (point.y() - chord_start.y()) - chord.y() * (point.x() - chord_start.x()) >= 0;
    };
    QPointF partner_middle;
    for (const QPointF& point : partner_flat)
    {
        partner_middle += point / partner_flat.size();
    }
    QPointF piece_middle;
    QPointF seam_middle;
    for (int i = 0; i < piece_points.size(); ++i)
    {
        seam_middle += piece_points.at(i) / piece_points.size();
    }
    for (const QPointF& point : piece.rest_positions)
    {
        piece_middle += point / piece.rest_positions.size();
    }
    QPointF partner_seam_middle;
    for (const QPointF& point : partner_points)
    {
        partner_seam_middle += point / partner_points.size();
    }

    // Turned as drafted or mirrored, whichever lays its body on the wanted side; if both or neither do, whichever
    // fits better.
    bool mirrored = false;
    qreal turn = 0;
    qreal best_error = std::numeric_limits<qreal>::infinity();
    bool best_side = false;
    for (const bool mirror : {false, true})
    {
        qreal error = 0;
        const qreal degrees = bestTurn(piece_points, partner_points, mirror, &error);
        const qreal radians = qDegreesToRadians(degrees);
        QPointF away = piece_middle - seam_middle;
        away.setX(mirror ? -away.x() : away.x());
        const QPointF body(partner_seam_middle.x() + away.x() * qCos(radians) - away.y() * qSin(radians),
                           partner_seam_middle.y() + away.x() * qSin(radians) + away.y() * qCos(radians));
        const bool same_side = sideOf(body) == sideOf(partner_middle);
        const bool wanted_side = (how == Superimpose::Side) != same_side;
        if ((wanted_side && !best_side) || (wanted_side == best_side && error < best_error))
        {
            mirrored = mirror;
            turn = degrees;
            best_error = error;
            best_side = wanted_side;
        }
    }
    arrangement.turned_over = mirrored;
    arrangement.rotation = std::fmod(turn + 360.0, 360.0);

    // Moved until its sides of the seams are on the partner's, a little out or in, or as far out.
    auto seamPoints = [](const QVector<QVector3D>& placed, const QVector<QVector<quint32>>& sides,
                         const QVector<QVector<LinePlace>>& places)
    {
        QVector<QVector3D> points;
        for (int s = 0; s < sides.size(); ++s)
        {
            QVector<QVector3D> line;
            for (const quint32 vertex : sides.at(s))
            {
                line.append(placed.value(static_cast<int>(vertex)));
            }
            for (const LinePlace& place : places.at(s))
            {
                points.append(pointAt(line, place));
            }
        }
        return points;
    };
    auto middleOf = [](const QVector<QVector3D>& points)
    {
        QVector3D middle;
        for (const QVector3D& point : points)
        {
            middle += point / static_cast<float>(points.size());
        }
        return middle;
    };
    const QVector<QVector3D> partner_seams = seamPoints(partner_placed, partner_sides, partner_places);
    const QVector3D target = middleOf(partner_seams);
    const qreal layer = how == Superimpose::Over ? layer_gap : (how == Superimpose::Under ? -layer_gap : 0.0);
    const qreal target_radius = radiusAt(arrangement.part, target) + layer;
    const PieceArrangement at_target = arrangementOn(arrangement.part, target);

    // Each piece is wrapped around a cylinder of its own, around its own height's axis, so laid along the partner's
    // seams in the flat, it can come out a little turned on the avatar. Seen from outside there, it is turned back by
    // as much as best lays its sides of the seams along the partner's, as in the flat, and moved again.
    const QVector3D out = outAt(arrangement.part, target);
    const QVector3D level = qAbs(out.y()) < 0.9f ? QVector3D(0, 1, 0) : QVector3D(1, 0, 0);
    const QVector3D across = QVector3D::crossProduct(level, out).normalized();
    const QVector3D up = QVector3D::crossProduct(out, across);
    auto seenFromOutside = [&across, &up](const QVector<QVector3D>& points)
    {
        QVector<QPointF> seen;
        for (const QVector3D& point : points)
        {
            seen.append(QPointF(QVector3D::dotProduct(point, across), QVector3D::dotProduct(point, up)));
        }
        return seen;
    };
    for (int turning = 0; turning < superimpose_turns; ++turning)
    {
        QVector<QVector3D> placed;
        for (int step = 0; step < superimpose_steps; ++step)
        {
            placed = place(piece, arrangement);
            const QVector3D current = middleOf(seamPoints(placed, piece_sides, piece_places));
            const PieceArrangement at_current = arrangementOn(arrangement.part, current);
            arrangement.angle = std::remainder(arrangement.angle + at_target.angle - at_current.angle, 360.0);
            arrangement.height += at_target.height - at_current.height;
            arrangement.distance = qMax(arrangement.distance + target_radius - radiusAt(arrangement.part, current),
                                        closest_clearance - clearance);
        }
        if (turning + 1 < superimpose_turns)
        {
            // How far to turn it anticlockwise, seen from outside with up as up; the rotation turns clockwise.
            qreal error = 0;
            const QVector<QVector3D> own_seams = seamPoints(place(piece, arrangement), piece_sides, piece_places);
            const qreal off = bestTurn(seenFromOutside(own_seams), seenFromOutside(partner_seams), false, &error);
            arrangement.rotation = std::fmod(arrangement.rotation - off + 720.0, 360.0);
        }
    }
    return arrangement;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Which way a piece put on the avatar as the arrangement says faces at its middle, before it leans or swings.
PieceFrame BodyWrap::frameOf(const GarmentMesh& mesh, const PieceArrangement& arrangement) const
{
    PieceFrame frame;
    placeFlat(arranged(mesh, arrangement), arrangement, 0, &frame);
    return frame;
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
    else if (part == BodyPart::Neck)
    {
        name = QStringLiteral("neck");
    }
    return name;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The body part written in the pattern file; anything unknown is the body.
BodyPart BodyWrap::partFromName(const QString& name)
{
    BodyPart part = BodyPart::Body;
    for (const BodyPart named : {BodyPart::LeftLeg, BodyPart::RightLeg, BodyPart::LeftArm, BodyPart::RightArm,
                                 BodyPart::Neck})
    {
        if (name == partName(named))
        {
            part = named;
        }
    }
    return part;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the part is a leg or an arm, one of a pair, rather than the body or the neck in its middle.
bool BodyWrap::isLimb(BodyPart part)
{
    return part != BodyPart::Body && part != BodyPart::Neck;
}

//---------------------------------------------------------------------------------------------------------------------
// Wraps the piece's flat points around its part, at the arrangement's distance and out cm further out, and finds the
// frame at its middle from points of its own put to either side of the middle and above and below it.
QVector<QVector3D> BodyWrap::placeFlat(QVector<QPointF> flat, const PieceArrangement& arrangement, qreal out,
                                      PieceFrame* frame) const
{
    const int count = flat.size();
    const QPointF middle = QPolygonF(flat).boundingRect().center();
    flat << middle << middle + QPointF(frame_step, 0) << middle - QPointF(frame_step, 0)
         << middle - QPointF(0, frame_step) << middle + QPointF(0, frame_step);
    const qreal further = out + arrangement.distance;
    QVector<QVector3D> placed = lineOf(arrangement.part) != nullptr ? placeAlongLine(flat, arrangement, further)
                                                                    : placeUpright(flat, arrangement, further);

    // The piece scene's y axis points down, so the point above the middle is the one with the smaller y.
    frame->middle = placed.at(count);
    frame->across = (placed.at(count + 1) - placed.at(count + 2)).normalized();
    const QVector3D up = placed.at(count + 3) - placed.at(count + 4);
    frame->up = (up - frame->across * QVector3D::dotProduct(up, frame->across)).normalized();
    frame->out = QVector3D::crossProduct(frame->across, frame->up);
    placed.resize(count);
    return placed;
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
    QVector<LineSkin> skin(m_skin.size());
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
    for (const LineSkin& skin : m_arm_skin[side])
    {
        if (skin.along >= from && skin.along <= to)
        {
            radius = qMax(radius, static_cast<qreal>(skin.distance));
        }
    }
    return radius > 0 ? radius : fallback_radius;
}

//---------------------------------------------------------------------------------------------------------------------
// Finds the skin of the neck: between the head joint and the neck joint along the neck's middle line, and not much
// further from the line than the neck's skin around its middle is, which leaves out the jaw and the shoulders.
void BodyWrap::findNeckSkin()
{
    const qreal length = m_neck_line.length();
    QVector<LineSkin> along_neck;
    QVector<float> around_middle;
    for (int i = 0; i < m_skin.size(); ++i)
    {
        qreal distance = 0;
        const qreal along = m_neck_line.alongNearest(m_skin.at(i), &distance);
        if (!m_skin_on_arm.at(i) && along >= 0 && along <= length)
        {
            along_neck.append({i, static_cast<float>(along), static_cast<float>(distance)});
            if (along >= length / 4.0 && along <= 3.0 * length / 4.0)
            {
                around_middle.append(static_cast<float>(distance));
            }
        }
    }
    if (around_middle.isEmpty())
    {
        return;
    }

    const auto median = around_middle.begin() + around_middle.size() / 2;
    std::nth_element(around_middle.begin(), median, around_middle.end());
    const float reach = static_cast<float>(neck_skin_reach) * *median;
    for (const LineSkin& skin : along_neck)
    {
        if (skin.distance <= reach)
        {
            m_neck_skin.append(skin);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Whether a point is at the neck: along it between the head joint and the neck joint, and no further from its middle
// line than its skin there, with a piece's clearance.
bool BodyWrap::onNeck(const QVector3D& point) const
{
    if (m_neck_skin.isEmpty())
    {
        return false;
    }
    qreal distance = 0;
    const qreal along = m_neck_line.alongNearest(point, &distance);
    return along >= 0 && along <= m_neck_line.length()
           && distance <= neckRadius(along - point_band, along + point_band) + clearance;
}

//---------------------------------------------------------------------------------------------------------------------
// How far the neck's skin reaches out from its middle line between two places along it. Below the neck joint, where
// the neck and the body are one, the neck is taken to be as thick as just above it.
qreal BodyWrap::neckRadius(qreal from, qreal to) const
{
    const qreal length = m_neck_line.length();
    from = qBound(0.0, from, qMax(0.0, length - clearance));
    to = qBound(qMin(from + clearance, length), to, length);

    qreal radius = 0;
    for (const LineSkin& skin : m_neck_skin)
    {
        if (skin.along >= from && skin.along <= to)
        {
            radius = qMax(radius, static_cast<qreal>(skin.distance));
        }
    }
    return radius > 0 ? radius : fallback_radius;
}

//---------------------------------------------------------------------------------------------------------------------
// How far an arm's or the neck's skin reaches out from its middle line between two places along it.
qreal BodyWrap::lineRadius(BodyPart part, qreal from, qreal to) const
{
    const int side = armSide(part);
    return side >= 0 ? armRadius(side, from, to) : neckRadius(from, to);
}

//---------------------------------------------------------------------------------------------------------------------
// The middle line a piece on the part is wrapped around: an arm's or the neck's; none for the body or a leg.
const LimbLine* BodyWrap::lineOf(BodyPart part) const
{
    const int side = armSide(part);
    return side >= 0 ? &m_arm_lines[side] : (part == BodyPart::Neck ? &m_neck_line : nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
// Wraps the piece's flat points around a tube along an arm's or the neck's middle line, its middle at the arrangement's
// place on the line and angle around it; on an arm, its top no further up the arm than the shoulder tip.
//
// The tube narrows along the line as the arm or the neck does, though slowly, staying clear of it near each place
// along it and wide enough there for the piece to go around without its sides overlapping. So a sleeve narrowing to
// the wrist wraps most of the way around the arm all the way down, and its underarm seam closes under the arm, not
// across it; a collar goes around the neck, close to it, with its ends at the back if it is put there in front.
QVector<QVector3D> BodyWrap::placeAlongLine(const QVector<QPointF>& flat, const PieceArrangement& arrangement,
                                            qreal out) const
{
    const int side = armSide(arrangement.part);
    const LimbLine& line = *lineOf(arrangement.part);
    const QRectF bounds = QPolygonF(flat).boundingRect();
    const QPointF middle = bounds.center();
    qreal top = line.alongAtHeight(arrangement.height) - bounds.height() / 2.0;
    if (side >= 0)
    {
        top = qMax(top, m_shoulder_tips[side]);
    }

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
        for (int nearby = qMax(0, row - line_window); nearby <= qMin(rows - 1, row + line_window); ++nearby)
        {
            width = qMax(width, widths.at(nearby));
        }
        radii[row] = qMax(lineRadius(arrangement.part, top + row - line_window, top + row + line_window) + clearance,
                          widestWrapRadius(width));
    }
    for (int row = 1; row < rows; ++row)
    {
        radii[row] = qMax(radii.at(row), radii.at(row - 1) - line_narrowing);
    }
    for (int row = rows - 2; row >= 0; --row)
    {
        radii[row] = qMax(radii.at(row), radii.at(row + 1) - line_narrowing);
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
// Finds the arrangement points: around the body at the bust, the waist and the hip, where they are measured, and
// halfway down the thighs; around the neck at its base and halfway up it; around each leg halfway down the
// thigh, at the knee and on the calf; around each arm halfway down the upper arm, at the elbow and at the wrist.
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
    addUprightPoints(BodyPart::Body, QStringLiteral("bust"), measurer.bustLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("waist"), measurer.waistLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("hip"), measurer.hipLevel(positions), around_body);
    addUprightPoints(BodyPart::Body, QStringLiteral("thigh"), (m_crotch + knees) / 2.0, around_body);

    const QVector<QPair<QString, qreal>> around_neck = {
        {QStringLiteral("front"), 0.0},
        {QStringLiteral("left"), left},
        {QStringLiteral("back"), 180.0},
        {QStringLiteral("right"), -left}};
    addLinePoints(BodyPart::Neck, QStringLiteral("middle"), m_neck_line.length() / 2.0, around_neck);
    addLinePoints(BodyPart::Neck, QStringLiteral("base"), m_neck_line.length(), around_neck);

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
        const BodyPart arm = side == 0 ? BodyPart::LeftArm : BodyPart::RightArm;
        const qreal elbow = m_arm_lines[side].alongNearest(m_arms[side][1]);
        const qreal upper_arm = (m_shoulder_tips[side] + elbow) / 2.0;
        const qreal outside = lineAngle(arm, upper_arm, QVector3D(0, 1, 0));
        const QVector<QPair<QString, qreal>> around_arm = {
            {QStringLiteral("front"), 0.0},
            {QStringLiteral("outside"), outside},
            {QStringLiteral("back"), 180.0},
            {QStringLiteral("inside"), std::remainder(outside + 180.0, 360.0)}};
        addLinePoints(arm, QStringLiteral("upperArm"), upper_arm, around_arm);
        addLinePoints(arm, QStringLiteral("elbow"), elbow, around_arm);
        addLinePoints(arm, QStringLiteral("wrist"), m_arm_lines[side].alongNearest(m_arms[side][2]), around_arm);
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
// Adds arrangement points around an arm or the neck at a place along it, at the angles of its sides, each as far from
// its middle line as the arm or the neck reaches there.
void BodyWrap::addLinePoints(BodyPart part, const QString& level, qreal along,
                             const QVector<QPair<QString, qreal>>& sides)
{
    const LimbLine& line = *lineOf(part);
    for (const QPair<QString, qreal>& around : sides)
    {
        const float reach = static_cast<float>(lineReach(part, along, around.second));
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
// The angle around an arm or the neck, at a place along it, that faces the most towards a direction.
qreal BodyWrap::lineAngle(BodyPart part, qreal along, const QVector3D& towards) const
{
    const LimbLine& line = *lineOf(part);
    const QVector3D front = line.frontAt(along);
    const QVector3D quarter = QVector3D::crossProduct(front, line.directionAt(along));
    return qRadiansToDegrees(qAtan2(QVector3D::dotProduct(towards, quarter), QVector3D::dotProduct(towards, front)));
}

//---------------------------------------------------------------------------------------------------------------------
// How far an arm's or the neck's skin reaches out from its middle line around a place along it, towards an angle
// around it; an arm's hand is left out.
qreal BodyWrap::lineReach(BodyPart part, qreal along, qreal angle) const
{
    const int side = armSide(part);
    const LimbLine& line = *lineOf(part);
    const QVector<LineSkin>& skins = side >= 0 ? m_arm_skin[side] : m_neck_skin;
    const qreal from = along - line_point_reach;
    const qreal to = qMin(along + line_point_reach, line.length());
    qreal reach = 0;
    for (const LineSkin& skin : skins)
    {
        if (skin.along >= from && skin.along <= to && skin.distance > reach)
        {
            const qreal towards = lineAngle(part, skin.along, m_skin.at(skin.vertex) - line.pointAt(skin.along));
            if (qAbs(std::remainder(towards - angle, 360.0)) <= line_point_spread)
            {
                reach = skin.distance;
            }
        }
    }
    return reach > 0 ? reach : lineRadius(part, from, to);
}

//---------------------------------------------------------------------------------------------------------------------
// Straight out from a part's axis, or from an arm's or the neck's middle line, through a point.
QVector3D BodyWrap::outAt(BodyPart part, const QVector3D& point) const
{
    QVector3D out;
    if (const LimbLine* line = lineOf(part))
    {
        out = point - line->pointAt(line->alongNearest(point));
    }
    else
    {
        const QVector3D axis = axisAt(part, point.y());
        out = QVector3D(point.x() - axis.x(), 0.0f, point.z() - axis.z());
    }
    return out.normalized();
}

//---------------------------------------------------------------------------------------------------------------------
// How far a point is from a part's axis, or from an arm's or the neck's middle line.
qreal BodyWrap::radiusAt(BodyPart part, const QVector3D& point) const
{
    qreal distance = 0;
    if (const LimbLine* line = lineOf(part))
    {
        line->alongNearest(point, &distance);
    }
    else
    {
        distance = horizontalDistance(point, axisAt(part, point.y()));
    }
    return distance;
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
