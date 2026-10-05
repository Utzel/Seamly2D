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

namespace
{
// Band widths in cm: seams, the selected seam, and the segments being sewn.
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
// The first few cm of the stretch, to mark where a seam starts.
QVector<QPointF> startMark(const SeamStretch& stretch)
{
    const qreal length = qMin(start_mark_length, stretch.length() * start_mark_share);
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
SeamEditor::SeamEditor(QObject* parent)
    : QObject(parent)
    , m_pieces()
    , m_seams()
    , m_shown_seams()
    , m_highlight_color(Qt::black)
    , m_sewing(false)
    , m_selected_seam(-1)
    , m_started()
    , m_hovered()
    , m_seam_bands(new SeamGeometry())
    , m_seam_lines(new SeamGeometry())
    , m_preview_bands(new SeamGeometry())
    , m_preview_lines(new SeamGeometry())
{
    for (SeamGeometry* geometry : {m_seam_bands, m_seam_lines, m_preview_bands, m_preview_lines})
    {
        geometry->setParent(this);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces on the board, with their outlines in cm. Seams of other pieces aren't shown.
void SeamEditor::setPieces(const QVector<Piece>& pieces)
{
    m_pieces = pieces;

    // A piece that was edited may have lost the segment being sewn.
    auto still_there = [this](const Edge& edge)
    {
        const PieceOutline* piece_outline = outline(edge.piece_id);
        return edge.isValid() && piece_outline != nullptr && edge.segment < piece_outline->segmentCount();
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
/// @brief While sewing, clicks on the board sew instead of selecting.
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
/// @brief The mouse moved over the board. Only matters while sewing, to show the segment it would sew.
void SeamEditor::hover(qreal x, qreal y, qreal tolerance)
{
    if (m_sewing)
    {
        const Edge edge = edgeAt(QPointF(x, y), tolerance);
        if (!edge.sameSegment(m_hovered) || edge.from_start != m_hovered.from_start)
        {
            m_hovered = edge;
            updatePreview();
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void SeamEditor::leave()
{
    if (m_hovered.isValid())
    {
        m_hovered = Edge();
        updatePreview();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on the board. While sewing it starts or finishes a seam; otherwise it selects the seam under it.
/// Returns false if the click is left for the pieces.
bool SeamEditor::click(qreal x, qreal y, qreal tolerance)
{
    const QPointF point(x, y);
    bool handled = false;

    if (m_sewing)
    {
        const Edge edge = edgeAt(point, tolerance);
        if (!edge.isValid())
        {
            setStarted(Edge());
        }
        else if (!m_started.isValid())
        {
            setStarted(edge);
        }
        else if (!edge.sameSegment(m_started))
        {
            const VSeam seam = seamBetween(m_started, edge);
            setStarted(Edge());
            if (!knowsSeam(seam))
            {
                emit seamSewn(seam);
            }
        }
        handled = true;
    }
    else
    {
        const QPointF on_pieces = flip(point);
        int nearest = -1;
        qreal nearest_distance = tolerance;
        for (const ShownSeam& seam : m_shown_seams)
        {
            const qreal distance = qMin(distanceToPolyline(on_pieces, seam.first.points()),
                                        distanceToPolyline(on_pieces, seam.second.points()));
            if (distance <= nearest_distance)
            {
                nearest = seam.index;
                nearest_distance = distance;
            }
        }
        setSelectedSeam(nearest);
        handled = nearest >= 0;
    }
    return handled;
}

//---------------------------------------------------------------------------------------------------------------------
const PieceOutline* SeamEditor::outline(quint32 piece_id) const
{
    const PieceOutline* found = nullptr;
    for (const Piece& piece : m_pieces)
    {
        if (piece.id == piece_id)
        {
            found = &piece.outline;
        }
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
// The segment nearest to a point on the board, if one is within the tolerance. Where pieces overlap, the one drawn
// in front wins.
SeamEditor::Edge SeamEditor::edgeAt(const QPointF& point, qreal tolerance) const
{
    const QPointF on_pieces = flip(point);
    Edge nearest;
    qreal nearest_distance = tolerance;
    for (const Piece& piece : m_pieces)
    {
        const OutlineHit hit = piece.outline.hit(on_pieces);
        if (hit.segment >= 0 && hit.distance <= nearest_distance)
        {
            nearest.piece_id = piece.id;
            nearest.segment = hit.segment;
            nearest.from_start = hit.along < 0.5;
            nearest_distance = hit.distance;
        }
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
// The edge's segment, running from the end the seam starts at.
SeamStretch SeamEditor::edgeStretch(const Edge& edge) const
{
    SeamStretch stretch;
    const PieceOutline* piece_outline = outline(edge.piece_id);
    if (edge.isValid() && piece_outline != nullptr)
    {
        stretch = piece_outline->stretch(piece_outline->segmentStart(edge.segment),
                                         piece_outline->segmentEnd(edge.segment));
        if (!edge.from_start)
        {
            stretch = stretch.reversed();
        }
    }
    return stretch;
}

//---------------------------------------------------------------------------------------------------------------------
// The seam sewing two segments together, the ends picked on each meeting.
VSeam SeamEditor::seamBetween(const Edge& first, const Edge& second) const
{
    const PieceOutline* first_outline = outline(first.piece_id);
    const PieceOutline* second_outline = outline(second.piece_id);

    VSeam seam;
    seam.first.piece_id = first.piece_id;
    seam.first.start_node = first_outline->segmentStart(first.segment);
    seam.first.end_node = first_outline->segmentEnd(first.segment);
    seam.second.piece_id = second.piece_id;
    seam.second.start_node = second_outline->segmentStart(second.segment);
    seam.second.end_node = second_outline->segmentEnd(second.segment);
    seam.reverse = first.from_start != second.from_start;
    return seam;
}

//---------------------------------------------------------------------------------------------------------------------
// Both sides of a seam, the second running like the first. False if a piece or point of the seam isn't shown.
bool SeamEditor::seamStretches(const VSeam& seam, SeamStretch* first, SeamStretch* second) const
{
    const PieceOutline* first_outline = outline(seam.first.piece_id);
    const PieceOutline* second_outline = outline(seam.second.piece_id);
    if (first_outline != nullptr && second_outline != nullptr)
    {
        *first = first_outline->stretch(seam.first.start_node, seam.first.end_node);
        *second = second_outline->stretch(seam.second.start_node, seam.second.end_node);
        if (seam.reverse)
        {
            *second = second->reversed();
        }
    }
    return first_outline != nullptr && second_outline != nullptr && !first->isEmpty() && !second->isEmpty();
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

    if (selection_gone)
    {
        emit selectedSeamChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Each seam is a band of its color along both sides, with lines between the places that meet.
void SeamEditor::updateSeamGeometry()
{
    QVector<SeamGeometry::Band> bands;
    QVector<SeamGeometry::Line> lines;
    for (const ShownSeam& seam : m_shown_seams)
    {
        const QColor color = seamColor(seam.index);
        const qreal width = seam.index == m_selected_seam ? selected_seam_width : seam_width;
        bands.append({flip(seam.first.points()), color, width});
        bands.append({flip(seam.second.points()), color, width});

        for (const SeamMatch& match : SeamStretch::matches(seam.first, seam.second))
        {
            lines.append({flip(seam.first.pointAt(match.first)), flip(seam.second.pointAt(match.second)), color});
        }
    }
    m_seam_bands->setBands(bands);
    m_seam_lines->setLines(lines);
}

//---------------------------------------------------------------------------------------------------------------------
// While sewing, the segments being sewn, wider where the seam starts, and the lines the seam would get.
void SeamEditor::updatePreview()
{
    QVector<SeamGeometry::Band> bands;
    QVector<SeamGeometry::Line> lines;
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
            const SeamStretch stretch = edgeStretch(edge);
            bands.append({flip(stretch.points()), m_highlight_color, preview_width});
            bands.append({flip(startMark(stretch)), m_highlight_color, start_mark_width});
        }

        if (edges.size() == 2)
        {
            const SeamStretch first = edgeStretch(m_started);
            const SeamStretch second = edgeStretch(m_hovered);
            for (const SeamMatch& match : SeamStretch::matches(first, second))
            {
                lines.append({flip(first.pointAt(match.first)), flip(second.pointAt(match.second)),
                              m_highlight_color});
            }
        }
    }
    m_preview_bands->setBands(bands);
    m_preview_lines->setLines(lines);
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
