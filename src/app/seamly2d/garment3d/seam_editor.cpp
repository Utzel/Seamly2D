//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_editor.cpp
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

#include "seam_editor.h"

#include <QLineF>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
// Band widths in cm: seams, the selected seam, and the segments being sewn. On the avatar they are tubes as thick.
const qreal seam_width = 0.6;
const qreal selected_seam_width = 1.4;
const qreal preview_width = 0.8;

// The end a seam starts at is marked by a wider band this long, in cm, but no longer than this share of the segment.
const qreal start_mark_length = 3.0;
const qreal start_mark_share = 0.3;
const qreal start_mark_width = 2.0;
const int start_mark_samples = 6;

// Seams get these colors in turn, so neighbouring seams can be told apart.
const char* const seam_colors[] = {"#e6194b", "#3cb44b", "#4363d8", "#f58231", "#911eb4", "#42d4f4", "#f032e6",
                                   "#9a6324"};

//---------------------------------------------------------------------------------------------------------------------
// The board's y axis points up, the pieces' down. Flipping is its own inverse, so this goes both ways.
QPointF flip(const QPointF& point)
{
    return QPointF(point.x(), -point.y());
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QPointF> flip(const QVector<QPointF>& points)
{
    QVector<QPointF> flipped;
    flipped.reserve(points.size());
    for (const QPointF& point : points)
    {
        flipped.append(flip(point));
    }
    return flipped;
}

//---------------------------------------------------------------------------------------------------------------------
qreal distanceToPolyline(const QPointF& point, const QVector<QPointF>& polyline)
{
    qreal nearest = std::numeric_limits<qreal>::infinity();
    for (int i = 0; i + 1 < polyline.size(); ++i)
    {
        const QPointF& a = polyline.at(i);
        const QPointF ab = polyline.at(i + 1) - a;
        const qreal length_squared = QPointF::dotProduct(ab, ab);
        const qreal t = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - a, ab) / length_squared, 1.0)
                                           : 0.0;
        nearest = qMin(nearest, QLineF(point, a + ab * t).length());
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
// How far along a stretch its start is marked.
qreal startMarkLength(const SeamStretch& stretch)
{
    return qMin(start_mark_length, stretch.length() * start_mark_share);
}

//---------------------------------------------------------------------------------------------------------------------
// The first few cm of the stretch, to mark where a seam starts.
QVector<QPointF> startMark(const SeamStretch& stretch)
{
    const qreal length = startMarkLength(stretch);
    QVector<QPointF> points;
    for (int i = 0; i <= start_mark_samples; ++i)
    {
        points.append(stretch.pointAt(length * i / start_mark_samples));
    }
    return points;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::Edge::isValid() const
{
    return segment >= 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::Edge::sameSegment(const Edge& other) const
{
    return piece_id == other.piece_id && segment == other.segment;
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::PlacedStretch::isEmpty() const
{
    return stretch.isEmpty() || points.size() != stretch.points().size();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::PlacedStretch::reverse()
{
    stretch = stretch.reversed();
    std::reverse(points.begin(), points.end());
}

//---------------------------------------------------------------------------------------------------------------------
// Where the point this far along the stretch, in cm as drafted, is now.
QVector3D SeamEditor::PlacedStretch::pointAt(qreal distance) const
{
    const QVector<qreal>& distances = stretch.distances();
    const auto after = std::upper_bound(distances.cbegin(), distances.cend(), distance);
    const int next = qBound(1, static_cast<int>(after - distances.cbegin()), static_cast<int>(distances.size()) - 1);
    const qreal span = distances.at(next) - distances.at(next - 1);
    const float t = static_cast<float>(span > 0 ? qBound(0.0, (distance - distances.at(next - 1)) / span, 1.0) : 0.0);
    return points.at(next - 1) + (points.at(next) - points.at(next - 1)) * t;
}

//---------------------------------------------------------------------------------------------------------------------
// The first few cm of the stretch, as they are now, to mark where a seam starts.
QVector<QVector3D> SeamEditor::PlacedStretch::startMark() const
{
    const qreal length = startMarkLength(stretch);
    QVector<QVector3D> mark;
    for (int i = 0; i <= start_mark_samples; ++i)
    {
        mark.append(pointAt(length * i / start_mark_samples));
    }
    return mark;
}

//---------------------------------------------------------------------------------------------------------------------
SeamEditor::SeamEditor(QObject* parent)
    : QObject(parent)
    , m_pieces()
    , m_positions()
    , m_symmetry()
    , m_seams()
    , m_shown_seams()
    , m_highlight_color(Qt::black)
    , m_sewing(false)
    , m_selected_seam(-1)
    , m_garment_shown(true)
    , m_started()
    , m_hovered()
    , m_seam_bands(new SeamGeometry())
    , m_seam_lines(new SeamGeometry())
    , m_preview_bands(new SeamGeometry())
    , m_preview_lines(new SeamGeometry())
    , m_garment_seams(new SeamGeometry())
    , m_garment_lines(new SeamGeometry())
    , m_garment_preview(new SeamGeometry())
    , m_garment_preview_lines(new SeamGeometry())
{
    for (SeamGeometry* geometry : {m_seam_bands, m_seam_lines, m_preview_bands, m_preview_lines, m_garment_seams,
                                   m_garment_lines, m_garment_preview, m_garment_preview_lines})
    {
        geometry->setParent(this);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces the scene shows, as drafted, on the board or with the meshes it shows them by on the avatar.
/// Seams of other pieces aren't shown.
void SeamEditor::setPieces(const QVector<ShownPiece>& pieces)
{
    m_pieces = pieces;
    m_symmetry = GarmentSymmetry();
    for (const ShownPiece& shown_piece : m_pieces)
    {
        m_symmetry.setPiece(shown_piece.id, shown_piece.symmetry, shown_piece.fold_start, shown_piece.fold_end);
    }

    // A piece that was edited may have lost the segment being sewn.
    auto still_there = [this](const Edge& edge)
    {
        const ShownPiece* edge_piece = piece(edge.piece_id);
        return edge.isValid() && edge_piece != nullptr && edge.segment < edge_piece->outline.segmentCount();
    };
    if (!still_there(m_hovered))
    {
        m_hovered = Edge();
    }
    if (m_started.isValid() && !still_there(m_started))
    {
        setStarted(Edge());
    }

    updateShownSeams();
    updatePreview();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the vertices of the meshes on the avatar are now, by the scene's ids, as the drape goes on.
void SeamEditor::setPositions(const QHash<quint32, QVector<QVector3D>>& positions)
{
    m_positions = positions;
    updateGarmentGeometry();
    updatePreview();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief All of the pattern's seams; the selection stays on the same index.
void SeamEditor::setSeams(const QVector<VSeam>& seams)
{
    m_seams = seams;
    updateShownSeams();
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::setHighlightColor(const QColor& color)
{
    m_highlight_color = color;
    updatePreview();
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::isSewing() const
{
    return m_sewing;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While sewing, clicks on the pieces sew instead of selecting.
void SeamEditor::setSewing(bool sewing)
{
    if (sewing != m_sewing)
    {
        m_sewing = sewing;
        m_started = Edge();
        m_hovered = Edge();
        updatePreview();
        emit sewingChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Index of the selected seam among the pattern's seams, or -1.
int SeamEditor::selectedSeam() const
{
    return m_selected_seam;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Selects a seam by its index among the pattern's seams; -1, or a seam that isn't shown, selects none.
void SeamEditor::setSelectedSeam(int index)
{
    const bool shown = std::any_of(m_shown_seams.cbegin(), m_shown_seams.cend(), [index](const ShownSeam& seam)
    {
        return seam.index == index;
    });
    const int selected = shown ? index : -1;

    if (selected != m_selected_seam)
    {
        m_selected_seam = selected;
        updateSeamGeometry();
        updateGarmentGeometry();
        emit selectedSeamChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief What to do next, while sewing or with a seam selected; empty otherwise.
QString SeamEditor::hint() const
{
    QString text;
    if (m_sewing)
    {
        text = m_started.isValid()
               ? tr("Now click the edge to sew it to, near the end that meets the start. Esc starts over.")
               : tr("Click the edge to sew, near the end where the seam starts. Esc stops sewing.");
    }
    else if (m_selected_seam >= 0)
    {
        text = tr("Lines that cross mean the seam is twisted: flip it. Delete removes the seam.");
    }
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Steps back, as Esc does: drops the seam started, else stops sewing, else drops the selection. Returns
/// whether there was anything to step back from.
bool SeamEditor::cancel()
{
    bool cancelled = true;
    if (m_started.isValid())
    {
        setStarted(Edge());
    }
    else if (m_sewing)
    {
        setSewing(false);
    }
    else if (m_selected_seam >= 0)
    {
        setSelectedSeam(-1);
    }
    else
    {
        cancelled = false;
    }
    return cancelled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the seams show on the pieces on the avatar too; while sewing they always do.
bool SeamEditor::isGarmentSeamsShown() const
{
    return m_garment_shown;
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::setGarmentSeamsShown(bool shown)
{
    if (shown != m_garment_shown)
    {
        m_garment_shown = shown;
        emit garmentSeamsShownChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QObject* SeamEditor::seamBands() const
{
    return m_seam_bands;
}

//---------------------------------------------------------------------------------------------------------------------
QObject* SeamEditor::seamLines() const
{
    return m_seam_lines;
}

//---------------------------------------------------------------------------------------------------------------------
QObject* SeamEditor::previewBands() const
{
    return m_preview_bands;
}

//---------------------------------------------------------------------------------------------------------------------
QObject* SeamEditor::previewLines() const
{
    return m_preview_lines;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seams on the pieces on the avatar, as tubes along both their sides, in scene coordinates.
QObject* SeamEditor::garmentSeams() const
{
    return m_garment_seams;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Lines between the places that meet, of seams on the avatar.
QObject* SeamEditor::garmentLines() const
{
    return m_garment_lines;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While sewing, the segments being sewn on the avatar.
QObject* SeamEditor::garmentPreview() const
{
    return m_garment_preview;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While sewing, the lines the seam being sewn on the avatar would get.
QObject* SeamEditor::garmentPreviewLines() const
{
    return m_garment_preview_lines;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mouse moved over the board. Only matters while sewing, to show the segment it would sew.
void SeamEditor::hover(qreal x, qreal y, qreal tolerance)
{
    if (m_sewing)
    {
        hoverEdge(edgeAt(QPointF(x, y), tolerance));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mouse moved over a piece on the avatar, shown by this id, at a point of its mesh's flat shape in cm.
/// Only matters while sewing, to show the segment it would sew.
void SeamEditor::hoverPiece(int id, qreal x, qreal y, qreal tolerance)
{
    if (m_sewing)
    {
        hoverEdge(edgeOn(static_cast<quint32>(id), QPointF(x, y), tolerance));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::leave()
{
    hoverEdge(Edge());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on the board. While sewing it starts or finishes a seam; otherwise it selects the seam under it.
/// Returns false if the click is left for the pieces.
bool SeamEditor::click(qreal x, qreal y, qreal tolerance)
{
    bool handled = true;
    if (m_sewing)
    {
        sewEdge(edgeAt(QPointF(x, y), tolerance));
    }
    else
    {
        const int nearest = seamNear(flip(QPointF(x, y)), tolerance, 0);
        setSelectedSeam(nearest);
        handled = nearest >= 0;
    }
    return handled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on a piece on the avatar, shown by this id, at a point of its mesh's flat shape in cm. While sewing
/// it starts or finishes a seam; otherwise it selects the seam of that piece under it. Returns false if the click is
/// left for the pieces, as it is on a piece not on the avatar.
bool SeamEditor::clickPiece(int id, qreal x, qreal y, qreal tolerance)
{
    const ShownMesh* shown = nullptr;
    const ShownPiece* clicked = pieceShowing(static_cast<quint32>(id), &shown);
    if (clicked == nullptr || !shown->placed)
    {
        return false;
    }

    bool handled = true;
    if (m_sewing)
    {
        sewEdge(edgeOn(static_cast<quint32>(id), QPointF(x, y), tolerance));
    }
    else
    {
        const int nearest = seamNear(clicked->drafted(shown->layout, QPointF(x, y)), tolerance, clicked->id);
        setSelectedSeam(nearest);
        handled = nearest >= 0;
    }
    return handled;
}

//---------------------------------------------------------------------------------------------------------------------
const ShownPiece* SeamEditor::piece(quint32 piece_id) const
{
    const auto found = std::find_if(m_pieces.cbegin(), m_pieces.cend(), [piece_id](const ShownPiece& candidate)
    {
        return candidate.id == piece_id;
    });
    return found != m_pieces.cend() ? &*found : nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
// The piece the scene shows by this id, and the mesh it shows.
const ShownPiece* SeamEditor::pieceShowing(quint32 id, const ShownMesh** shown) const
{
    for (const ShownPiece& candidate : m_pieces)
    {
        *shown = candidate.mesh(id);
        if (*shown != nullptr)
        {
            return &candidate;
        }
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::isOnBoard(quint32 piece_id) const
{
    const ShownPiece* found = piece(piece_id);
    return found != nullptr && !found->isPlaced();
}

//---------------------------------------------------------------------------------------------------------------------
bool SeamEditor::isMirrored(quint32 piece_id) const
{
    const ShownPiece* found = piece(piece_id);
    return found != nullptr && found->symmetry == PieceSymmetry::Pair;
}

//---------------------------------------------------------------------------------------------------------------------
// The segment of a piece on the board nearest to a point on the board, if one is within the tolerance. Where pieces
// overlap, the one drawn in front wins.
SeamEditor::Edge SeamEditor::edgeAt(const QPointF& point, qreal tolerance) const
{
    const QPointF on_pieces = flip(point);
    Edge nearest;
    qreal nearest_distance = tolerance;
    for (const ShownPiece& candidate : m_pieces)
    {
        if (candidate.isPlaced())
        {
            continue;
        }
        const OutlineHit hit = candidate.outline.hit(on_pieces);
        if (hit.segment >= 0 && hit.distance <= nearest_distance)
        {
            nearest.piece_id = candidate.id;
            nearest.segment = hit.segment;
            nearest.from_start = hit.along < 0.5;
            nearest_distance = hit.distance;
        }
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
// The segment of the piece on the avatar shown by this id nearest to a point of its mesh's flat shape, as a segment
// of the piece as drafted, if one is within the tolerance. A fold line is no edge.
SeamEditor::Edge SeamEditor::edgeOn(quint32 id, const QPointF& point, qreal tolerance) const
{
    Edge edge;
    const ShownMesh* shown = nullptr;
    const ShownPiece* hovered = pieceShowing(id, &shown);
    if (hovered != nullptr && shown->placed)
    {
        const OutlineHit hit = hovered->outline.hit(hovered->drafted(shown->layout, point));
        if (hit.segment >= 0 && hit.distance <= tolerance && !hovered->foldSegments().value(hit.segment))
        {
            edge.piece_id = hovered->id;
            edge.segment = hit.segment;
            edge.from_start = hit.along < 0.5;
        }
    }
    return edge;
}

//---------------------------------------------------------------------------------------------------------------------
// The edge's segment as drafted, running from the end the seam starts at.
SeamStretch SeamEditor::edgeStretch(const Edge& edge) const
{
    SeamStretch stretch;
    const ShownPiece* edge_piece = piece(edge.piece_id);
    if (edge.isValid() && edge_piece != nullptr)
    {
        stretch = edge_piece->outline.stretch(edge_piece->outline.segmentStart(edge.segment),
                                              edge_piece->outline.segmentEnd(edge.segment));
        if (!edge.from_start)
        {
            stretch = stretch.reversed();
        }
    }
    return stretch;
}

//---------------------------------------------------------------------------------------------------------------------
// The seam sewing two segments together, the ends picked on each meeting. Sewn to its mirror image, a segment meets it
// end to end.
VSeam SeamEditor::seamBetween(const Edge& first, const Edge& second) const
{
    const PieceOutline& first_outline = piece(first.piece_id)->outline;
    const PieceOutline& second_outline = piece(second.piece_id)->outline;

    VSeam seam;
    seam.first.piece_id = first.piece_id;
    seam.first.start_node = first_outline.segmentStart(first.segment);
    seam.first.end_node = first_outline.segmentEnd(first.segment);
    seam.second.piece_id = second.piece_id;
    seam.second.start_node = second_outline.segmentStart(second.segment);
    seam.second.end_node = second_outline.segmentEnd(second.segment);
    seam.reverse = first.from_start != second.from_start && !first.sameSegment(second);
    return seam;
}

//---------------------------------------------------------------------------------------------------------------------
// Both sides of a seam as drafted, the second running like the first. False if a piece or point of the seam isn't
// shown.
bool SeamEditor::seamStretches(const VSeam& seam, SeamStretch* first, SeamStretch* second) const
{
    const ShownPiece* first_piece = piece(seam.first.piece_id);
    const ShownPiece* second_piece = piece(seam.second.piece_id);
    if (first_piece != nullptr && second_piece != nullptr)
    {
        *first = first_piece->outline.stretch(seam.first.start_node, seam.first.end_node);
        *second = second_piece->outline.stretch(seam.second.start_node, seam.second.end_node);
        if (seam.reverse)
        {
            *second = second->reversed();
        }
    }
    return first_piece != nullptr && second_piece != nullptr && !first->isEmpty() && !second->isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
// Whether the pattern already has this seam, either way round.
bool SeamEditor::knowsSeam(const VSeam& seam) const
{
    VSeam swapped = seam;
    swapped.first = seam.second;
    swapped.second = seam.first;
    return m_seams.contains(seam) || m_seams.contains(swapped);
}

//---------------------------------------------------------------------------------------------------------------------
// The seam with a side nearest to a point of a piece as drafted, if one is within the tolerance, or -1: among the
// sides on the board for piece 0, else among the sides on that piece.
int SeamEditor::seamNear(const QPointF& point, qreal tolerance, quint32 piece_id) const
{
    int nearest = -1;
    qreal nearest_distance = tolerance;
    for (const ShownSeam& seam : m_shown_seams)
    {
        const VSeam& stored = m_seams.at(seam.index);
        const std::pair<quint32, const SeamStretch*> sides[] = {{stored.first.piece_id, &seam.first},
                                                                {stored.second.piece_id, &seam.second}};
        for (const auto& side : sides)
        {
            const bool wanted = piece_id == 0 ? isOnBoard(side.first) : side.first == piece_id;
            const qreal distance = wanted ? distanceToPolyline(point, side.second->points())
                                          : std::numeric_limits<qreal>::infinity();
            if (distance <= nearest_distance)
            {
                nearest = seam.index;
                nearest_distance = distance;
            }
        }
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
// The seam in the garment: the pattern's seam, with its twin on the other side of the body, or a side sewn to its
// mirror image.
QVector<GarmentSeam> SeamEditor::garmentSeams(const VSeam& seam) const
{
    GarmentSeam garment_seam;
    garment_seam.first = {seam.first.piece_id, seam.first.start_node, seam.first.end_node};
    garment_seam.second = {seam.second.piece_id, seam.second.start_node, seam.second.end_node};
    garment_seam.reverse = seam.reverse;
    return m_symmetry.madeUp({garment_seam});
}

//---------------------------------------------------------------------------------------------------------------------
// Where a side of a garment seam runs now, on the mesh of a piece on the avatar; empty if the piece isn't there.
SeamEditor::PlacedStretch SeamEditor::placedStretch(const GarmentSeamSide& side) const
{
    PlacedStretch placed;
    const ShownMesh* shown = nullptr;
    const ShownPiece* side_piece = pieceShowing(side.piece, &shown);
    const auto positions = m_positions.constFind(side.piece);
    if (side_piece != nullptr && shown->placed && positions != m_positions.cend()
        && positions->size() == shown->mesh.vertexCount())
    {
        placed.stretch = shown->mesh.stretch(side.start_node, side.end_node);
        for (const quint32 vertex : placed.stretch.vertices())
        {
            placed.points.append(positions->at(static_cast<int>(vertex)));
        }
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
// While sewing, a click on an edge starts a seam there, or with one started finishes it; a click off the edges drops
// the seam started.
void SeamEditor::sewEdge(const Edge& edge)
{
    if (!edge.isValid())
    {
        setStarted(Edge());
    }
    else if (!m_started.isValid())
    {
        setStarted(edge);
    }
    else if (!edge.sameSegment(m_started) || isMirrored(edge.piece_id))
    {
        const VSeam seam = seamBetween(m_started, edge);
        setStarted(Edge());
        if (!knowsSeam(seam))
        {
            emit seamSewn(seam);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::hoverEdge(const Edge& edge)
{
    if (!edge.sameSegment(m_hovered) || edge.from_start != m_hovered.from_start)
    {
        m_hovered = edge;
        updatePreview();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::updateShownSeams()
{
    m_shown_seams.clear();
    for (int i = 0; i < m_seams.size(); ++i)
    {
        ShownSeam shown;
        shown.index = i;
        if (seamStretches(m_seams.at(i), &shown.first, &shown.second))
        {
            m_shown_seams.append(shown);
        }
    }

    const bool still_shown = std::any_of(m_shown_seams.cbegin(), m_shown_seams.cend(), [this](const ShownSeam& seam)
    {
        return seam.index == m_selected_seam;
    });
    const bool selection_gone = m_selected_seam >= 0 && !still_shown;
    if (selection_gone)
    {
        m_selected_seam = -1;
    }

    updateSeamGeometry();
    updateGarmentGeometry();

    if (selection_gone)
    {
        emit selectedSeamChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// On the board, each seam is a band of its color along its sides there, with lines between the places that meet when
// both are on the board.
void SeamEditor::updateSeamGeometry()
{
    QVector<SeamGeometry::Band> bands;
    QVector<SeamGeometry::Line> lines;
    for (const ShownSeam& seam : m_shown_seams)
    {
        const QColor color = seamColor(seam.index);
        const qreal width = seam.index == m_selected_seam ? selected_seam_width : seam_width;
        const bool first_on_board = isOnBoard(m_seams.at(seam.index).first.piece_id);
        const bool second_on_board = isOnBoard(m_seams.at(seam.index).second.piece_id);
        if (first_on_board)
        {
            bands.append({flip(seam.first.points()), color, width});
        }
        if (second_on_board)
        {
            bands.append({flip(seam.second.points()), color, width});
        }
        if (first_on_board && second_on_board)
        {
            for (const SeamMatch& match : SeamStretch::matches(seam.first, seam.second))
            {
                lines.append({flip(seam.first.pointAt(match.first)), flip(seam.second.pointAt(match.second)), color});
            }
        }
    }
    m_seam_bands->setBands(bands);
    m_seam_lines->setLines(lines);
}

//---------------------------------------------------------------------------------------------------------------------
// On the avatar, each seam and its twin are tubes of the seam's color along their sides there, with lines between the
// places that meet when both are there, which shrink away as the seam closes.
void SeamEditor::updateGarmentGeometry()
{
    QVector<SeamGeometry::Tube> tubes;
    QVector<SeamGeometry::Segment> segments;
    for (const ShownSeam& seam : m_shown_seams)
    {
        const QColor color = seamColor(seam.index);
        const qreal radius = (seam.index == m_selected_seam ? selected_seam_width : seam_width) / 2.0;
        for (const GarmentSeam& garment_seam : garmentSeams(m_seams.at(seam.index)))
        {
            const PlacedStretch first = placedStretch(garment_seam.first);
            PlacedStretch second = placedStretch(garment_seam.second);
            if (!second.isEmpty() && garment_seam.reverse)
            {
                second.reverse();
            }
            if (!first.isEmpty())
            {
                tubes.append({first.points, color, radius});
            }
            if (!second.isEmpty())
            {
                tubes.append({second.points, color, radius});
            }
            if (!first.isEmpty() && !second.isEmpty())
            {
                for (const SeamMatch& match : SeamStretch::matches(first.stretch, second.stretch))
                {
                    segments.append({first.pointAt(match.first), second.pointAt(match.second), color});
                }
            }
        }
    }
    m_garment_seams->setTubes(tubes);
    m_garment_lines->setSegments(segments);
}

//---------------------------------------------------------------------------------------------------------------------
// While sewing, the segments being sewn, wider where the seam starts, and the lines the seam would get: on the board
// for pieces on the board, on the avatar for pieces there, on both sides of the body.
void SeamEditor::updatePreview()
{
    QVector<SeamGeometry::Band> bands;
    QVector<SeamGeometry::Line> lines;
    QVector<SeamGeometry::Tube> tubes;
    QVector<SeamGeometry::Segment> segments;
    if (m_sewing)
    {
        QVector<Edge> edges;
        if (m_started.isValid())
        {
            edges.append(m_started);
        }
        if (m_hovered.isValid() && !m_hovered.sameSegment(m_started))
        {
            edges.append(m_hovered);
        }

        for (const Edge& edge : edges)
        {
            if (isOnBoard(edge.piece_id))
            {
                const SeamStretch stretch = edgeStretch(edge);
                bands.append({flip(stretch.points()), m_highlight_color, preview_width});
                bands.append({flip(startMark(stretch)), m_highlight_color, start_mark_width});
                continue;
            }

            // On the avatar, the segment and its mirror image, which on an unfolded piece's mirrored half runs the
            // other way.
            const PieceOutline& outline = piece(edge.piece_id)->outline;
            const GarmentSeamSide side = {edge.piece_id, outline.segmentStart(edge.segment),
                                          outline.segmentEnd(edge.segment)};
            QVector<std::pair<GarmentSeamSide, bool>> sides = {{side, false}};
            GarmentSeamSide mirror;
            bool turned = false;
            if (m_symmetry.mirrored(side, &mirror, &turned))
            {
                sides.append({mirror, turned});
            }
            for (const auto& garment_side : sides)
            {
                PlacedStretch placed = placedStretch(garment_side.first);
                if (!placed.isEmpty())
                {
                    if (edge.from_start == garment_side.second)
                    {
                        placed.reverse();
                    }
                    tubes.append({placed.points, m_highlight_color, preview_width / 2.0});
                    tubes.append({placed.startMark(), m_highlight_color, start_mark_width / 2.0});
                }
            }
        }

        if (edges.size() == 2)
        {
            if (isOnBoard(m_started.piece_id) && isOnBoard(m_hovered.piece_id))
            {
                const SeamStretch first = edgeStretch(m_started);
                const SeamStretch second = edgeStretch(m_hovered);
                for (const SeamMatch& match : SeamStretch::matches(first, second))
                {
                    lines.append({flip(first.pointAt(match.first)), flip(second.pointAt(match.second)),
                                  m_highlight_color});
                }
            }
            for (const GarmentSeam& garment_seam : garmentSeams(seamBetween(m_started, m_hovered)))
            {
                const PlacedStretch first = placedStretch(garment_seam.first);
                PlacedStretch second = placedStretch(garment_seam.second);
                if (first.isEmpty() || second.isEmpty())
                {
                    continue;
                }
                if (garment_seam.reverse)
                {
                    second.reverse();
                }
                for (const SeamMatch& match : SeamStretch::matches(first.stretch, second.stretch))
                {
                    segments.append({first.pointAt(match.first), second.pointAt(match.second), m_highlight_color});
                }
            }
        }
    }
    m_preview_bands->setBands(bands);
    m_preview_lines->setLines(lines);
    m_garment_preview->setTubes(tubes);
    m_garment_preview_lines->setSegments(segments);
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::setStarted(const Edge& edge)
{
    const bool was_started = m_started.isValid();
    m_started = edge;
    updatePreview();
    if (was_started != m_started.isValid())
    {
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QColor SeamEditor::seamColor(int index)
{
    const int color_count = static_cast<int>(sizeof(seam_colors) / sizeof(seam_colors[0]));
    return QColor(seam_colors[index % color_count]);
}
