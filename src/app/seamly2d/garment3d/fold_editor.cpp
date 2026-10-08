//---------------------------------------------------------------------------------------------------------------------
//  @file   fold_editor.cpp
//  @author Julius
//  @date   8 Oct, 2026
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

#include "fold_editor.h"

#include <QLineF>
#include <QLocale>

#include <limits>

namespace
{
// While folding, the folds are drawn in this color, the internal paths that aren't folds in this one.
const char* const fold_color = "#e0782b";
const char* const path_color = "#8c8c8c";

// The degree sign, after an angle.
const QChar degree_sign(0x00B0);

//---------------------------------------------------------------------------------------------------------------------
// The internal path a line of a mesh follows: the line's own, or on a piece unfolded the path it is the mirror image
// of.
quint32 pathOfLine(quint32 line_id)
{
    return PieceOutline::isMirrorId(line_id) ? PieceOutline::mirrorId(line_id) : line_id;
}

//---------------------------------------------------------------------------------------------------------------------
// How far a point of a mesh's flat shape is from one of its lines, in cm.
qreal distanceToLine(const GarmentMesh& mesh, const MeshLine& line, const QPointF& point)
{
    qreal nearest = std::numeric_limits<qreal>::infinity();
    for (int i = 0; i + 1 < line.vertices.size(); ++i)
    {
        const QPointF& a = mesh.rest_positions.at(static_cast<int>(line.vertices.at(i)));
        const QPointF& b = mesh.rest_positions.at(static_cast<int>(line.vertices.at(i + 1)));
        const QPointF along = b - a;
        const qreal length_squared = QPointF::dotProduct(along, along);
        const qreal t = length_squared > 0 ? qBound(0.0, QPointF::dotProduct(point - a, along) / length_squared, 1.0)
                                           : 0.0;
        nearest = qMin(nearest, QLineF(point, a + along * t).length());
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
QString angleText(qreal angle)
{
    return QLocale().toString(angle, 'g', 4) + degree_sign;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool FoldEditor::Path::isValid() const
{
    return piece_id != 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool FoldEditor::Path::operator==(const Path& other) const
{
    return piece_id == other.piece_id && path_id == other.path_id;
}

//---------------------------------------------------------------------------------------------------------------------
FoldEditor::FoldEditor(QObject* parent)
    : QObject(parent)
    , m_pieces()
    , m_folds()
    , m_highlight(Qt::white)
    , m_folding(false)
    , m_angle(360.0)
    , m_hovered()
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces the scene shows, as drafted, with the meshes it shows them by, and the pattern's folds. The lines
/// to draw aren't announced: the scene gets them with its new meshes.
void FoldEditor::setPieces(const QVector<ShownPiece>& pieces, const QVector<VFold>& folds)
{
    m_pieces = pieces;
    m_folds = folds;

    // A piece that was edited may have lost the path the mouse is over.
    bool hovered_there = false;
    for (const ShownPiece& piece : m_pieces)
    {
        for (const OutlineLine& line : piece.outline.lines())
        {
            hovered_there = hovered_there || (piece.id == m_hovered.piece_id && line.id == m_hovered.path_id);
        }
    }
    if (m_hovered.isValid() && !hovered_there)
    {
        m_hovered = Path();
    }
    emit hintChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern's folds, as they are after an edit, an undo or a redo.
void FoldEditor::setFolds(const QVector<VFold>& folds)
{
    if (!(folds == m_folds))
    {
        m_folds = folds;
        emit linesChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFold> FoldEditor::folds() const
{
    return m_folds;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The color the path under the mouse is drawn in.
void FoldEditor::setHighlightColor(const QColor& color)
{
    m_highlight = color;
}

//---------------------------------------------------------------------------------------------------------------------
bool FoldEditor::isFolding() const
{
    return m_folding;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While folding, the internal paths show, and clicks on pieces fold them along them instead of selecting them.
void FoldEditor::setFolding(bool folding)
{
    if (folding != m_folding)
    {
        m_folding = folding;
        m_hovered = Path();
        emit foldingChanged();
        emit hintChanged();
        emit linesChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The angle a click folds a path at, in degrees, as VFold says.
qreal FoldEditor::angle() const
{
    return m_angle;
}

//---------------------------------------------------------------------------------------------------------------------
void FoldEditor::setAngle(qreal angle)
{
    m_angle = qBound(0.0, angle, 360.0);
    emit hintChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief What to do next while folding; empty otherwise.
QString FoldEditor::hint() const
{
    QString text;
    if (!m_folding)
    {
        return text;
    }

    if (!hasPaths())
    {
        text = tr("The pieces have no internal paths to fold along: add some to them in Seamly2D. Esc stops folding.");
    }
    else if (!m_hovered.isValid())
    {
        text = tr("Click near a line inside a piece to fold the piece along it at %1, the angle chosen in Fold's menu, "
                  "or to unfold it. Esc stops folding.").arg(angleText(m_angle));
    }
    else
    {
        const int fold = foldOf(m_hovered);
        if (fold < 0)
        {
            text = tr("Click to fold along this line at %1. Esc stops folding.").arg(angleText(m_angle));
        }
        else if (qFuzzyCompare(1.0 + m_folds.at(fold).angle, 1.0 + m_angle))
        {
            text = tr("Click to unfold this line. Esc stops folding.");
        }
        else
        {
            text = tr("Click to fold along this line at %1 instead of %2. Esc stops folding.")
                       .arg(angleText(m_angle), angleText(m_folds.at(fold).angle));
        }
    }
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Stops folding, as Esc does. Returns whether there was anything to stop.
bool FoldEditor::cancel()
{
    const bool cancelled = m_folding;
    setFolding(false);
    return cancelled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While folding, the lines to draw on every mesh the scene shows, by the scene's id: its internal paths, the
/// folds and the path under the mouse each in a color of their own. None otherwise.
QHash<quint32, QVector<DrawnLine>> FoldEditor::lines() const
{
    QHash<quint32, QVector<DrawnLine>> drawn;
    if (!m_folding)
    {
        return drawn;
    }

    for (const ShownPiece& piece : m_pieces)
    {
        for (const ShownMesh& shown : piece.shown)
        {
            for (const MeshLine& line : shown.mesh.lines)
            {
                const Path path{piece.id, pathOfLine(line.id)};
                DrawnLine drawn_line;
                drawn_line.vertices = line.vertices;
                drawn_line.color = path == m_hovered ? m_highlight
                                   : foldOf(path) >= 0 ? QColor(fold_color) : QColor(path_color);
                drawn[shown.id].append(drawn_line);
            }
        }
    }
    return drawn;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The folds of a pattern piece, along the lines of one of the meshes it drapes as: the piece, unfolded or not,
/// or its mirrored copy, which folds the same way on its right side. Strength is how stiffly a fold holds its angle,
/// as ClothFold says.
QVector<ClothFold> FoldEditor::clothFolds(quint32 piece_id, const GarmentMesh& mesh, qreal strength) const
{
    QVector<ClothFold> folds;
    for (const VFold& fold : m_folds)
    {
        if (fold.piece_id != piece_id)
        {
            continue;
        }
        for (const MeshLine& line : mesh.lines)
        {
            if (pathOfLine(line.id) == fold.path_id)
            {
                ClothFold cloth_fold;
                cloth_fold.vertices = line.vertices;
                cloth_fold.angle = fold.angle;
                cloth_fold.strength = strength;
                folds.append(cloth_fold);
            }
        }
    }
    return folds;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mouse moved over a piece the scene shows, at a point of its mesh's flat shape in cm. Only matters while
/// folding, to show the path a click would fold. Paths further than the tolerance, in cm, aren't under it.
void FoldEditor::hover(int id, qreal x, qreal y, qreal tolerance)
{
    if (m_folding)
    {
        setHovered(pathAt(static_cast<quint32>(id), QPointF(x, y), tolerance));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void FoldEditor::leave()
{
    setHovered(Path());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on a piece the scene shows. While folding, a click near an internal path folds the piece along it at
/// the angle chosen, folds it at that angle instead if it is folded at another, or unfolds it if it is folded at that
/// angle. Returns whether the click was used.
bool FoldEditor::click(int id, qreal x, qreal y, qreal tolerance)
{
    const Path path = m_folding ? pathAt(static_cast<quint32>(id), QPointF(x, y), tolerance) : Path();
    if (!path.isValid())
    {
        return false;
    }

    QVector<VFold> after = m_folds;
    const int fold = foldOf(path);
    QString text;
    if (fold >= 0 && qFuzzyCompare(1.0 + after.at(fold).angle, 1.0 + m_angle))
    {
        after.remove(fold);
        text = tr("unfold piece");
    }
    else if (fold >= 0)
    {
        after[fold].angle = m_angle;
        text = tr("change fold");
    }
    else
    {
        after.append({path.piece_id, path.path_id, m_angle});
        text = tr("fold piece");
    }

    emit foldsEdited(after, text);
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Which of the pattern's folds is along the path, or -1.
int FoldEditor::foldOf(const Path& path) const
{
    for (int i = 0; i < m_folds.size(); ++i)
    {
        if (m_folds.at(i).piece_id == path.piece_id && m_folds.at(i).path_id == path.path_id)
        {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
// The internal path nearest to a point of the flat shape of the mesh the scene shows by this id, if no further than
// the tolerance.
FoldEditor::Path FoldEditor::pathAt(quint32 id, const QPointF& point, qreal tolerance) const
{
    Path path;
    qreal nearest = tolerance;
    for (const ShownPiece& piece : m_pieces)
    {
        const ShownMesh* shown = piece.mesh(id);
        if (shown == nullptr)
        {
            continue;
        }
        for (const MeshLine& line : shown->mesh.lines)
        {
            const qreal distance = distanceToLine(shown->mesh, line, point);
            if (distance <= nearest)
            {
                nearest = distance;
                path = Path{piece.id, pathOfLine(line.id)};
            }
        }
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether any piece shown has an internal path to fold along.
bool FoldEditor::hasPaths() const
{
    for (const ShownPiece& piece : m_pieces)
    {
        for (const ShownMesh& shown : piece.shown)
        {
            if (!shown.mesh.lines.isEmpty())
            {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
void FoldEditor::setHovered(const Path& path)
{
    if (!(path == m_hovered))
    {
        m_hovered = path;
        emit hintChanged();
        emit linesChanged();
    }
}
