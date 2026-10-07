//---------------------------------------------------------------------------------------------------------------------
//  @file   stitch_editor.cpp
//  @author Julius
//  @date   6 Oct, 2026
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

#include "stitch_editor.h"

#include <QLineF>

#include <algorithm>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
qreal cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

//---------------------------------------------------------------------------------------------------------------------
// The point mirrored across the line through a and b.
QPointF reflect(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF along = b - a;
    const QPointF offset = point - a;
    const QPointF onto = along * (QPointF::dotProduct(offset, along) / QPointF::dotProduct(along, along));
    return a + onto * 2.0 - offset;
}

//---------------------------------------------------------------------------------------------------------------------
// Which of the outline's path points the node is, or -1.
int nodePosition(const PieceOutline& outline, quint32 id)
{
    const QVector<OutlineNode>& nodes = outline.nodes();
    for (int k = 0; k < nodes.size(); ++k)
    {
        if (nodes.at(k).id == id)
        {
            return k;
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
// The ends of the fold line of a piece cut on the fold.
bool foldLine(const StitchEditor::Piece& piece, QPointF* start, QPointF* end)
{
    const int first = piece.fold_start != 0 ? nodePosition(piece.outline, piece.fold_start) : -1;
    const int last = piece.fold_end != 0 ? nodePosition(piece.outline, piece.fold_end) : -1;
    if (first < 0 || last < 0)
    {
        return false;
    }
    *start = piece.outline.points().at(piece.outline.nodes().at(first).index);
    *end = piece.outline.points().at(piece.outline.nodes().at(last).index);
    return QLineF(*start, *end).length() > 0;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool StitchEditor::Edge::isValid() const
{
    return segment >= 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool StitchEditor::Edge::operator==(const Edge& other) const
{
    return piece_id == other.piece_id && segment == other.segment;
}

//---------------------------------------------------------------------------------------------------------------------
StitchEditor::StitchEditor(QObject* parent)
    : QObject(parent)
    , m_pieces()
    , m_topstitches()
    , m_stitching(false)
    , m_hovered()
    , m_stitches()
    , m_preview()
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces the scene shows, as drafted, with the meshes it shows them by, and the pattern's topstitching.
/// Their stitches are worked out right away, but not announced: the scene gets them with its new meshes.
void StitchEditor::setPieces(const QVector<Piece>& pieces, const VTopstitches& topstitches)
{
    m_pieces = pieces;
    m_topstitches = topstitches;

    // A piece that was edited may have lost the segment the mouse is over.
    const auto hovered = std::find_if(m_pieces.cbegin(), m_pieces.cend(), [this](const Piece& piece)
    {
        return piece.id == m_hovered.piece_id;
    });
    if (m_hovered.isValid() && (hovered == m_pieces.cend() || m_hovered.segment >= hovered->outline.segmentCount()))
    {
        m_hovered = Edge();
        emit hintChanged();
    }

    workOutStitches();
    workOutPreview();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern's topstitching. Its style is the one edges clicked are stitched in.
void StitchEditor::setTopstitches(const VTopstitches& topstitches)
{
    if (!(topstitches == m_topstitches))
    {
        m_topstitches = topstitches;
        workOutStitches();
        workOutPreview();
        emit stitchesChanged();
        emit previewChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool StitchEditor::isStitching() const
{
    return m_stitching;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While stitching, clicks on pieces stitch their edges instead of selecting them.
void StitchEditor::setStitching(bool stitching)
{
    if (stitching != m_stitching)
    {
        m_stitching = stitching;
        setHovered(Edge());
        emit stitchingChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief What to do next while stitching; empty otherwise.
QString StitchEditor::hint() const
{
    QString text;
    if (m_stitching && m_hovered.isValid())
    {
        const auto piece = std::find_if(m_pieces.cbegin(), m_pieces.cend(), [this](const Piece& candidate)
        {
            return candidate.id == m_hovered.piece_id;
        });
        const QString style = piece != m_pieces.cend() ? segmentStyles(*piece).value(m_hovered.segment) : QString();
        if (style.isEmpty())
        {
            text = tr("Click to topstitch along this edge. Esc stops topstitching.");
        }
        else if (style == chosenStyle().name)
        {
            text = tr("Click to take the stitches out of this edge. Esc stops topstitching.");
        }
        else
        {
            text = tr("Click to topstitch this edge in the chosen style instead. Esc stops topstitching.");
        }
    }
    else if (m_stitching)
    {
        text = tr("Click near an edge of a piece to topstitch along it in the style chosen in Topstitch's menu, or to "
                  "take its stitches out. Esc stops topstitching.");
    }
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Stops stitching, as Esc does. Returns whether there was anything to stop.
bool StitchEditor::cancel()
{
    const bool cancelled = m_stitching;
    setStitching(false);
    return cancelled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stitches of every mesh the scene shows, by the scene's id.
QHash<quint32, QVector<ThreadStitch>> StitchEditor::stitches() const
{
    return m_stitches;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stitches the edge under the mouse would get, by the scene's id; none when the mouse isn't near one.
QHash<quint32, QVector<ThreadStitch>> StitchEditor::preview() const
{
    return m_preview;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mouse moved over a piece the scene shows, at a point of its mesh's flat shape in cm. Only matters while
/// stitching, to show the edge a click would stitch. Edges further than the tolerance, in cm, aren't under it.
void StitchEditor::hover(int id, qreal x, qreal y, qreal tolerance)
{
    if (m_stitching)
    {
        setHovered(edgeAt(static_cast<quint32>(id), QPointF(x, y), tolerance));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void StitchEditor::leave()
{
    setHovered(Edge());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on a piece the scene shows. While stitching, a click near an edge stitches it in the chosen style,
/// restitches it in the chosen style if it is stitched in another, or takes its stitches out if it is stitched in the
/// chosen one. Returns whether the click was used.
bool StitchEditor::click(int id, qreal x, qreal y, qreal tolerance)
{
    const Edge edge = m_stitching ? edgeAt(static_cast<quint32>(id), QPointF(x, y), tolerance) : Edge();
    const auto piece = std::find_if(m_pieces.cbegin(), m_pieces.cend(), [&edge](const Piece& candidate)
    {
        return candidate.id == edge.piece_id;
    });
    if (!edge.isValid() || piece == m_pieces.cend())
    {
        return false;
    }

    const quint32 start = piece->outline.segmentStart(edge.segment);
    const quint32 end = piece->outline.segmentEnd(edge.segment);
    const QString style = segmentStyles(*piece).value(edge.segment);
    const QString chosen = chosenStyle().name;

    // An edge stitched by the garment keeps no entry of its own, nor one left out where the garment isn't stitched.
    // Edges stitched one by one keep the style they were stitched in.
    VTopstitches after = m_topstitches;
    after.segments.erase(std::remove_if(after.segments.begin(), after.segments.end(),
                                        [piece, start, end](const VTopstitch& segment)
    {
        return segment.piece_id == piece->id && segment.start_node == start && segment.end_node == end;
    }), after.segments.end());

    QString text;
    if (style == chosen)
    {
        if (after.all)
        {
            after.segments.append({piece->id, start, end, false, QString()});
        }
        text = tr("take out topstitching");
    }
    else
    {
        after.segments.append({piece->id, start, end, true, chosen});
        text = style.isEmpty() ? tr("topstitch edge") : tr("change topstitching");
    }

    emit topstitchesEdited(after, text);
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// The piece the scene shows by this id, and how the mesh it shows lies over the piece as drafted.
const StitchEditor::Piece* StitchEditor::pieceShowing(quint32 id, Layout* layout) const
{
    for (const Piece& piece : m_pieces)
    {
        for (const Shown& shown : piece.shown)
        {
            if (shown.id == id)
            {
                *layout = shown.layout;
                return &piece;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
// The segments along the fold line of a piece cut on the fold, from the fold's start to its end; the garment has no
// edge there.
QVector<bool> StitchEditor::foldSegments(const Piece& piece) const
{
    const int count = piece.outline.segmentCount();
    QVector<bool> fold(count, false);
    const int start = piece.fold_start != 0 ? nodePosition(piece.outline, piece.fold_start) : -1;
    const int end = piece.fold_end != 0 ? nodePosition(piece.outline, piece.fold_end) : -1;
    if (start >= 0 && end >= 0 && start != end)
    {
        for (int segment = start; segment != end; segment = (segment + 1) % count)
        {
            fold[segment] = true;
        }
    }
    return fold;
}

//---------------------------------------------------------------------------------------------------------------------
// The style each of the piece's segments is topstitched in, one of the presets' names; empty where it isn't stitched.
QVector<QString> StitchEditor::segmentStyles(const Piece& piece) const
{
    const QVector<bool> fold = foldSegments(piece);
    QVector<QString> styles(fold.size());
    for (int segment = 0; segment < fold.size(); ++segment)
    {
        const quint32 start = piece.outline.segmentStart(segment);
        const quint32 end = piece.outline.segmentEnd(segment);
        if (!fold.at(segment) && m_topstitches.isStitched(piece.id, start, end))
        {
            styles[segment] = TopstitchStyle::preset(m_topstitches.styleOf(piece.id, start, end)).name;
        }
    }
    return styles;
}

//---------------------------------------------------------------------------------------------------------------------
// The style edges clicked are stitched in: the garment's.
TopstitchStyle StitchEditor::chosenStyle() const
{
    return TopstitchStyle::preset(m_topstitches.style);
}

//---------------------------------------------------------------------------------------------------------------------
// Where a point of a shown mesh's flat shape is on the piece as drafted: on an unfolded piece's mirrored half, it is
// mirrored back across the fold line.
QPointF StitchEditor::drafted(const Piece& piece, Layout layout, const QPointF& point) const
{
    QPointF on_piece = point;
    QPointF start;
    QPointF end;
    if (layout == Layout::Mirrored)
    {
        on_piece.setX(-point.x());
    }
    else if (layout == Layout::Unfolded && foldLine(piece, &start, &end))
    {
        // The drafted half lies on the side of the fold line its points are furthest to.
        qreal drafted_side = 0;
        for (const QPointF& outline_point : piece.outline.points())
        {
            const qreal side = cross(end - start, outline_point - start);
            drafted_side = qAbs(side) > qAbs(drafted_side) ? side : drafted_side;
        }
        if (cross(end - start, point - start) * drafted_side < 0)
        {
            on_piece = reflect(point, start, end);
        }
    }
    return on_piece;
}

//---------------------------------------------------------------------------------------------------------------------
// The edge of the piece shown by this id nearest to a point of its mesh's flat shape, if no further than the
// tolerance; a fold line is no edge.
StitchEditor::Edge StitchEditor::edgeAt(quint32 id, const QPointF& point, qreal tolerance) const
{
    Edge edge;
    Layout layout = Layout::Drafted;
    const Piece* piece = pieceShowing(id, &layout);
    if (piece != nullptr)
    {
        const OutlineHit hit = piece->outline.hit(drafted(*piece, layout, point));
        if (hit.segment >= 0 && hit.distance <= tolerance && !foldSegments(*piece).value(hit.segment))
        {
            edge.piece_id = piece->id;
            edge.segment = hit.segment;
        }
    }
    return edge;
}

//---------------------------------------------------------------------------------------------------------------------
void StitchEditor::setHovered(const Edge& edge)
{
    if (!(edge == m_hovered))
    {
        m_hovered = edge;
        workOutPreview();
        emit previewChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Each style's rows go along the segments stitched in it, one row per distance; internal paths take the garment's
// style's stitches.
void StitchEditor::workOutStitches()
{
    m_stitches.clear();
    const TopstitchStyle chosen = chosenStyle();
    for (const Piece& piece : m_pieces)
    {
        const QVector<QString> styles = segmentStyles(piece);
        for (const TopstitchStyle& style : TopstitchStyle::presets())
        {
            if (!styles.contains(style.name))
            {
                continue;
            }
            QVector<bool> stitched(styles.size());
            for (int segment = 0; segment < styles.size(); ++segment)
            {
                stitched[segment] = styles.at(segment) == style.name;
            }
            QVector<QVector<QPointF>> rows;
            for (const qreal distance : style.distances)
            {
                rows += Topstitching::rows(piece.outline, stitched, distance);
            }
            for (const Shown& shown : piece.shown)
            {
                m_stitches[shown.id] += Topstitching::stitches(shown.mesh, laidOut(rows, piece, shown.layout),
                                                               style.stitch_length, style.thread_width);
            }
        }
        for (const Shown& shown : piece.shown)
        {
            m_stitches[shown.id] += Topstitching::stitches(shown.mesh, laidOut(piece.paths, piece, shown.layout),
                                                           chosen.stitch_length, chosen.thread_width);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void StitchEditor::workOutPreview()
{
    m_preview.clear();
    for (const Piece& piece : m_pieces)
    {
        if (m_hovered.isValid() && piece.id == m_hovered.piece_id)
        {
            QVector<bool> stitched(piece.outline.segmentCount(), false);
            stitched[m_hovered.segment] = true;
            const TopstitchStyle style = chosenStyle();
            QVector<QVector<QPointF>> rows;
            for (const qreal distance : style.distances)
            {
                rows += Topstitching::rows(piece.outline, stitched, distance);
            }
            for (const Shown& shown : piece.shown)
            {
                m_preview.insert(shown.id, Topstitching::stitches(shown.mesh, laidOut(rows, piece, shown.layout),
                                                                  style.stitch_length, style.thread_width));
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The rows drawn on the piece as drafted, laid out as a mesh the scene shows lies over it: on an unfolded piece also
// mirrored across the fold line, on a mirrored copy mirrored.
QVector<QVector<QPointF>> StitchEditor::laidOut(const QVector<QVector<QPointF>>& rows, const Piece& piece,
                                                Layout layout)
{
    QVector<QVector<QPointF>> laid = rows;
    QPointF start;
    QPointF end;
    if (layout == Layout::Mirrored)
    {
        for (QVector<QPointF>& row : laid)
        {
            for (QPointF& point : row)
            {
                point.setX(-point.x());
            }
        }
    }
    else if (layout == Layout::Unfolded && foldLine(piece, &start, &end))
    {
        for (QVector<QPointF> row : rows)
        {
            for (QPointF& point : row)
            {
                point = reflect(point, start, end);
            }
            laid.append(row);
        }
    }
    return laid;
}
