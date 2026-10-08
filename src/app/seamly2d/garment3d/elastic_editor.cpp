//---------------------------------------------------------------------------------------------------------------------
//  @file   elastic_editor.cpp
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

#include "elastic_editor.h"

#include <QLineF>
#include <QLocale>

#include <limits>

namespace
{
// While sewing elastic, the elastics are drawn in this color, the internal paths without in this one.
const char* const elastic_color = "#1f9e89";
const char* const path_color = "#8c8c8c";

// An elastic is at least this share of its line long.
const qreal shortest_ratio = 0.1;

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
// A ratio as the percentage of the line's length it is.
QString ratioText(qreal ratio)
{
    return QLocale().toString(ratio * 100.0, 'g', 3) + QLatin1Char('%');
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool ElasticEditor::Line::isValid() const
{
    return piece_id != 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool ElasticEditor::Line::operator==(const Line& other) const
{
    return piece_id == other.piece_id && start_node == other.start_node && end_node == other.end_node
           && path_id == other.path_id;
}

//---------------------------------------------------------------------------------------------------------------------
ElasticEditor::ElasticEditor(QObject* parent)
    : QObject(parent)
    , m_pieces()
    , m_elastics()
    , m_symmetry()
    , m_highlight(Qt::white)
    , m_editing(false)
    , m_ratio(0.8)
    , m_hovered()
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces the scene shows, as drafted, with the meshes it shows them by, and the pattern's elastics. The
/// lines to draw aren't announced: the scene gets them with its new meshes.
void ElasticEditor::setPieces(const QVector<ShownPiece>& pieces, const QVector<VElastic>& elastics)
{
    m_pieces = pieces;
    m_elastics = elastics;
    m_symmetry = GarmentSymmetry();
    for (const ShownPiece& piece : m_pieces)
    {
        m_symmetry.setPiece(piece.id, piece.symmetry, piece.fold_start, piece.fold_end);
    }

    // A piece that was edited may have lost the line the mouse is over.
    const bool hovered_there = std::any_of(m_pieces.cbegin(), m_pieces.cend(), [this](const ShownPiece& piece)
    {
        return piece.id == m_hovered.piece_id
               && std::any_of(piece.shown.cbegin(), piece.shown.cend(), [this](const ShownMesh& shown)
                  {
                      return !meshLines(shown.mesh, m_hovered).isEmpty();
                  });
    });
    if (m_hovered.isValid() && !hovered_there)
    {
        m_hovered = Line();
    }
    emit hintChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern's elastics, as they are after an edit, an undo or a redo.
void ElasticEditor::setElastics(const QVector<VElastic>& elastics)
{
    if (!(elastics == m_elastics))
    {
        m_elastics = elastics;
        emit linesChanged();
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VElastic> ElasticEditor::elastics() const
{
    return m_elastics;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The color the line under the mouse is drawn in.
void ElasticEditor::setHighlightColor(const QColor& color)
{
    m_highlight = color;
}

//---------------------------------------------------------------------------------------------------------------------
bool ElasticEditor::isEditing() const
{
    return m_editing;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While sewing elastic, the elastics and the internal paths show, and clicks on pieces sew elastic along their
/// edges and paths instead of selecting them.
void ElasticEditor::setEditing(bool editing)
{
    if (editing != m_editing)
    {
        m_editing = editing;
        m_hovered = Line();
        emit editingChanged();
        emit hintChanged();
        emit linesChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How long a click makes an elastic, as the share of its line's length VElastic says.
qreal ElasticEditor::ratio() const
{
    return m_ratio;
}

//---------------------------------------------------------------------------------------------------------------------
void ElasticEditor::setRatio(qreal ratio)
{
    m_ratio = qBound(shortest_ratio, ratio, 1.0);
    emit hintChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief What to do next while sewing elastic; empty otherwise.
QString ElasticEditor::hint() const
{
    QString text;
    if (!m_editing)
    {
        return text;
    }

    if (!m_hovered.isValid())
    {
        text = tr("Click near an edge of a piece, or a line inside it, to sew elastic along it, %1 of its length, as "
                  "chosen in Elastic's menu, or to take the elastic out. Esc stops.").arg(ratioText(m_ratio));
    }
    else
    {
        const int elastic = elasticOf(m_hovered);
        if (elastic < 0)
        {
            text = tr("Click to sew elastic along this, %1 of its length. Esc stops.").arg(ratioText(m_ratio));
        }
        else if (qFuzzyCompare(1.0 + m_elastics.at(elastic).ratio, 1.0 + m_ratio))
        {
            text = tr("Click to take this elastic out. Esc stops.");
        }
        else
        {
            text = tr("Click to make this elastic %1 of its line's length instead of %2. Esc stops.")
                       .arg(ratioText(m_ratio), ratioText(m_elastics.at(elastic).ratio));
        }
    }
    return text;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Stops sewing elastic, as Esc does. Returns whether there was anything to stop.
bool ElasticEditor::cancel()
{
    const bool cancelled = m_editing;
    setEditing(false);
    return cancelled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While sewing elastic, the lines to draw on every mesh the scene shows, by the scene's id: the internal paths,
/// the edges with elastic, and the line under the mouse, the elastics and that line each in a color of their own. None
/// otherwise.
QHash<quint32, QVector<DrawnLine>> ElasticEditor::lines() const
{
    QHash<quint32, QVector<DrawnLine>> drawn;
    if (!m_editing)
    {
        return drawn;
    }

    auto color_of = [this](const Line& line)
    {
        return line == m_hovered ? m_highlight : elasticOf(line) >= 0 ? QColor(elastic_color) : QColor(path_color);
    };
    for (const ShownPiece& piece : m_pieces)
    {
        QVector<Line> edges;
        for (const VElastic& elastic : m_elastics)
        {
            if (elastic.piece_id == piece.id && elastic.path_id == NULL_ID)
            {
                edges.append(Line{piece.id, elastic.start_node, elastic.end_node, NULL_ID});
            }
        }
        if (m_hovered.piece_id == piece.id && m_hovered.path_id == NULL_ID && !edges.contains(m_hovered))
        {
            edges.append(m_hovered);
        }

        for (const ShownMesh& shown : piece.shown)
        {
            for (const MeshLine& mesh_line : shown.mesh.lines)
            {
                drawn[shown.id].append({mesh_line.vertices,
                                        color_of(Line{piece.id, NULL_ID, NULL_ID, pathOfLine(mesh_line.id)})});
            }
            for (const Line& edge : edges)
            {
                for (const QVector<quint32>& vertices : meshLines(shown.mesh, edge))
                {
                    drawn[shown.id].append({vertices, color_of(edge)});
                }
            }
        }
    }
    return drawn;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The elastics of a pattern piece, along the lines of one of the meshes it drapes as: the piece, unfolded or
/// not, or its mirrored copy; the vertices counting from the mesh's first, offset, over all pieces.
QVector<ClothElastic> ElasticEditor::clothElastics(quint32 piece_id, const GarmentMesh& mesh, quint32 offset) const
{
    QVector<ClothElastic> elastics;
    for (const VElastic& stored : m_elastics)
    {
        if (stored.piece_id != piece_id)
        {
            continue;
        }
        const Line line{stored.piece_id, stored.start_node, stored.end_node, stored.path_id};
        for (const QVector<quint32>& vertices : meshLines(mesh, line))
        {
            ClothElastic elastic;
            for (const quint32 vertex : vertices)
            {
                elastic.vertices.append(vertex + offset);
            }
            elastic.ratio = stored.ratio;
            elastics.append(elastic);
        }
    }
    return elastics;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The mouse moved over a piece the scene shows, at a point of its mesh's flat shape in cm. Only matters while
/// sewing elastic, to show the line a click would sew it along. Lines further than the tolerance, in cm, aren't under
/// it.
void ElasticEditor::hover(int id, qreal x, qreal y, qreal tolerance)
{
    if (m_editing)
    {
        setHovered(lineAt(static_cast<quint32>(id), QPointF(x, y), tolerance));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void ElasticEditor::leave()
{
    setHovered(Line());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A click on a piece the scene shows. While sewing elastic, a click near an edge or an internal path sews
/// elastic along it at the ratio chosen, makes its elastic that long instead if it is another, or takes it out if it is
/// that long. Returns whether the click was used.
bool ElasticEditor::click(int id, qreal x, qreal y, qreal tolerance)
{
    const Line line = m_editing ? lineAt(static_cast<quint32>(id), QPointF(x, y), tolerance) : Line();
    if (!line.isValid())
    {
        return false;
    }

    QVector<VElastic> after = m_elastics;
    const int elastic = elasticOf(line);
    QString text;
    if (elastic >= 0 && qFuzzyCompare(1.0 + after.at(elastic).ratio, 1.0 + m_ratio))
    {
        after.remove(elastic);
        text = tr("take elastic out");
    }
    else if (elastic >= 0)
    {
        after[elastic].ratio = m_ratio;
        text = tr("change elastic");
    }
    else
    {
        after.append({line.piece_id, line.start_node, line.end_node, line.path_id, m_ratio});
        text = tr("sew elastic");
    }

    emit elasticsEdited(after, text);
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Which of the pattern's elastics is along the line, or -1.
int ElasticEditor::elasticOf(const Line& line) const
{
    for (int i = 0; i < m_elastics.size(); ++i)
    {
        const VElastic& elastic = m_elastics.at(i);
        if (Line{elastic.piece_id, elastic.start_node, elastic.end_node, elastic.path_id} == line)
        {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
// The edge or internal path nearest to a point of the flat shape of the mesh the scene shows by this id, if no further
// than the tolerance. A fold line is no edge.
ElasticEditor::Line ElasticEditor::lineAt(quint32 id, const QPointF& point, qreal tolerance) const
{
    Line line;
    qreal nearest = tolerance;
    for (const ShownPiece& piece : m_pieces)
    {
        const ShownMesh* shown = piece.mesh(id);
        if (shown == nullptr)
        {
            continue;
        }
        for (const MeshLine& mesh_line : shown->mesh.lines)
        {
            const qreal distance = distanceToLine(shown->mesh, mesh_line, point);
            if (distance <= nearest)
            {
                nearest = distance;
                line = Line{piece.id, NULL_ID, NULL_ID, pathOfLine(mesh_line.id)};
            }
        }
        const OutlineHit hit = piece.outline.hit(piece.drafted(shown->layout, point));
        if (hit.segment >= 0 && hit.distance <= nearest && !piece.foldSegments().value(hit.segment))
        {
            nearest = hit.distance;
            line = Line{piece.id, piece.outline.segmentStart(hit.segment), piece.outline.segmentEnd(hit.segment),
                        NULL_ID};
        }
    }
    return line;
}

//---------------------------------------------------------------------------------------------------------------------
// Where a line runs on a mesh a piece shows or drapes as: the vertices along it, once for each time it is there. An
// edge of a piece cut on the fold runs on both halves of it unfolded.
QVector<QVector<quint32>> ElasticEditor::meshLines(const GarmentMesh& mesh, const Line& line) const
{
    QVector<QVector<quint32>> found;
    if (line.path_id != NULL_ID)
    {
        for (const MeshLine& mesh_line : mesh.lines)
        {
            if (pathOfLine(mesh_line.id) == line.path_id)
            {
                found.append(mesh_line.vertices);
            }
        }
        return found;
    }

    QVector<GarmentSeamSide> sides = {{line.piece_id, line.start_node, line.end_node, false}};
    GarmentSeamSide mirror;
    bool turned = false;
    if (m_symmetry.symmetry(line.piece_id) == PieceSymmetry::Fold && m_symmetry.mirrored(sides.first(), &mirror, &turned))
    {
        sides.append(mirror);
    }
    for (const GarmentSeamSide& side : sides)
    {
        const QVector<quint32> vertices = mesh.stretch(side.start_node, side.end_node).vertices();
        if (vertices.size() >= 2 && !found.contains(vertices))
        {
            found.append(vertices);
        }
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
void ElasticEditor::setHovered(const Line& line)
{
    if (!(line == m_hovered))
    {
        m_hovered = line;
        emit hintChanged();
        emit linesChanged();
    }
}
