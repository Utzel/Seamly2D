//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_scene_model.cpp
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

#include "garment_scene_model.h"

#include <QRectF>
#include <QtMath>

#include <algorithm>
#include <limits>

#include "../vgarment/piece_outline.h"
#include "avatar_geometry.h"
#include "piece_geometry.h"
#include "stitch_geometry.h"

namespace
{
// Gap in cm between the back of the avatar and the board of pieces behind it.
const float board_gap = 40.0f;

// The checks pieces can be shown in repeat every so many cm, across the grain and along it.
const qreal check_repeat = 4.0;

// Stitches an edge under the mouse would get are drawn this much thicker than stitches, so they show over them.
const qreal preview_scale = 1.8;

// Thread matching the cloth is this much of the way from the cloth's color to black on light cloth, or to white on
// dark cloth, so the stitching still shows.
const qreal matching_thread_share = 0.55;

//---------------------------------------------------------------------------------------------------------------------
// The pattern piece a row shows, or shows the mirrored copy of.
quint32 patternPiece(quint32 id)
{
    return PieceOutline::isMirrorId(id) ? PieceOutline::mirrorId(id) : id;
}

//---------------------------------------------------------------------------------------------------------------------
// The point of the triangle nearest to the given point, as how much of each corner it takes.
QVector3D nearestWeights(const QVector3D& point, const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D ap = point - a;
    const float d1 = QVector3D::dotProduct(ab, ap);
    const float d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0 && d2 <= 0)
    {
        return QVector3D(1, 0, 0);
    }

    const QVector3D bp = point - b;
    const float d3 = QVector3D::dotProduct(ab, bp);
    const float d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0 && d4 <= d3)
    {
        return QVector3D(0, 1, 0);
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0)
    {
        const float v = d1 / (d1 - d3);
        return QVector3D(1 - v, v, 0);
    }

    const QVector3D cp = point - c;
    const float d5 = QVector3D::dotProduct(ab, cp);
    const float d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0 && d5 <= d6)
    {
        return QVector3D(0, 0, 1);
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0)
    {
        const float w = d2 / (d2 - d6);
        return QVector3D(1 - w, 0, w);
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
    {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return QVector3D(0, 1 - w, w);
    }

    const float area = va + vb + vc;
    const float v = vb / area;
    const float w = vc / area;
    return QVector3D(1 - v - w, v, w);
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D lowerCorner(const QVector3D& a, const QVector3D& b)
{
    return QVector3D(qMin(a.x(), b.x()), qMin(a.y(), b.y()), qMin(a.z(), b.z()));
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D upperCorner(const QVector3D& a, const QVector3D& b)
{
    return QVector3D(qMax(a.x(), b.x()), qMax(a.y(), b.y()), qMax(a.z(), b.z()));
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
GarmentSceneModel::GarmentSceneModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_rows()
    , m_selected_piece(0)
    , m_scene_center()
    , m_scene_radius(0)
    , m_board_offset()
    , m_avatar(nullptr)
    , m_has_avatar(false)
    , m_avatar_minimum()
    , m_avatar_maximum()
    , m_avatar_note()
    , m_arranging(false)
    , m_hint()
    , m_strain_shown(false)
    , m_checks_shown(false)
    , m_thread_color()
{}

//---------------------------------------------------------------------------------------------------------------------
int GarmentSceneModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

//---------------------------------------------------------------------------------------------------------------------
QVariant GarmentSceneModel::data(const QModelIndex& index, int role) const
{
    QVariant value;
    if (index.isValid() && index.row() < m_rows.size())
    {
        const Row& row = m_rows.at(index.row());
        switch (role)
        {
            case PieceIdRole:
                value = static_cast<int>(row.id);
                break;
            case PieceNameRole:
                value = row.name;
                break;
            case PieceColorRole:
                value = row.color;
                break;
            case PieceGeometryRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.geometry));
                break;
            case PieceOutlineRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.outline));
                break;
            case PieceStitchesRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.stitch_geometry));
                break;
            case PieceStitchPreviewRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.preview_geometry));
                break;
            case PieceThreadColorRole:
                value = threadColor(row.color);
                break;
            case SelectedRole:
                value = patternPiece(row.id) == m_selected_piece;
                break;
            case PlacedRole:
                value = row.placed;
                break;
            default:
                break;
        }
    }
    return value;
}

//---------------------------------------------------------------------------------------------------------------------
QHash<int, QByteArray> GarmentSceneModel::roleNames() const
{
    return {{PieceIdRole, QByteArrayLiteral("pieceId")},
            {PieceNameRole, QByteArrayLiteral("pieceName")},
            {PieceColorRole, QByteArrayLiteral("pieceColor")},
            {PieceGeometryRole, QByteArrayLiteral("pieceGeometry")},
            {PieceOutlineRole, QByteArrayLiteral("pieceOutline")},
            {PieceStitchesRole, QByteArrayLiteral("pieceStitches")},
            {PieceStitchPreviewRole, QByteArrayLiteral("pieceStitchPreview")},
            {PieceThreadColorRole, QByteArrayLiteral("pieceThreadColor")},
            {SelectedRole, QByteArrayLiteral("selected")},
            {PlacedRole, QByteArrayLiteral("placed")}};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Shows these pieces. If they are the same pieces as before, in the same order, only their meshes, names and
/// colors are updated, so the scene doesn't flicker while the pattern is edited.
void GarmentSceneModel::setPieces(const QVector<Piece>& pieces)
{
    const bool was_empty = m_rows.isEmpty();

    bool same_pieces = pieces.size() == m_rows.size();
    for (int i = 0; i < pieces.size() && same_pieces; ++i)
    {
        same_pieces = pieces.at(i).id == m_rows.at(i).id;
    }

    if (same_pieces)
    {
        for (int i = 0; i < pieces.size(); ++i)
        {
            Row& row = m_rows[i];
            row.name = pieces.at(i).name;
            row.color = pieces.at(i).color;
            row.mesh = pieces.at(i).mesh;
            row.positions = pieces.at(i).positions;
            row.grain_angle = pieces.at(i).grain_angle;
            row.placed = !row.positions.isEmpty();
            row.stitches = pieces.at(i).stitches;
            row.preview = pieces.at(i).preview;
            row.geometry->setMesh(row.mesh, row.positions, m_strain_shown, row.grain_angle);
            row.outline->setOutline(row.mesh, row.positions);
            showStitches(row);
        }
        if (!m_rows.isEmpty())
        {
            emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                             {PieceNameRole, PieceColorRole, PieceThreadColorRole, PlacedRole});
        }
    }
    else
    {
        beginResetModel();
        for (const Row& row : m_rows)
        {
            row.geometry->deleteLater();
            row.outline->deleteLater();
            row.stitch_geometry->deleteLater();
            row.preview_geometry->deleteLater();
        }
        m_rows.clear();

        for (const Piece& piece : pieces)
        {
            Row row;
            row.id = piece.id;
            row.name = piece.name;
            row.color = piece.color;
            row.mesh = piece.mesh;
            row.positions = piece.positions;
            row.grain_angle = piece.grain_angle;
            row.placed = !piece.positions.isEmpty();
            row.geometry = new PieceGeometry();
            row.geometry->setParent(this);
            row.geometry->setMesh(piece.mesh, piece.positions, m_strain_shown, row.grain_angle);
            row.outline = new PieceGeometry();
            row.outline->setParent(this);
            row.outline->setOutline(piece.mesh, piece.positions);
            row.stitches = piece.stitches;
            row.preview = piece.preview;
            row.stitch_geometry = new StitchGeometry();
            row.stitch_geometry->setParent(this);
            row.preview_geometry = new StitchGeometry();
            row.preview_geometry->setParent(this);
            showStitches(row);
            m_rows.append(row);
        }
        endResetModel();
        emit pieceCountChanged();

        // Drops the highlight if the selected piece is gone.
        setSelectedPiece(m_selected_piece);
    }

    // The board only holds the pieces that aren't placed on the avatar.
    m_piece_bounds = QRectF();
    for (const Piece& piece : pieces)
    {
        if (piece.positions.isEmpty())
        {
            m_piece_bounds = m_piece_bounds.united(piece.mesh.bounds());
        }
    }
    updateSceneBounds();

    if (was_empty && !m_rows.isEmpty())
    {
        emit framingRequested();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Moves a placed piece, as the drape simulation goes on. The mesh stays the same.
void GarmentSceneModel::setPiecePositions(quint32 id, const QVector<QVector3D>& positions)
{
    for (Row& row : m_rows)
    {
        if (row.id == id && row.placed && positions.size() == row.mesh.vertexCount())
        {
            row.positions = positions;
            row.geometry->setMesh(row.mesh, positions, m_strain_shown, row.grain_angle);
            row.outline->setOutline(row.mesh, positions);
            showStitches(row);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The topstitching of every piece shown, by its id; pieces left out have none.
void GarmentSceneModel::setStitches(const QHash<quint32, QVector<ThreadStitch>>& stitches)
{
    for (Row& row : m_rows)
    {
        const QVector<ThreadStitch> wanted = stitches.value(row.id);
        if (wanted != row.stitches)
        {
            row.stitches = wanted;
            row.stitch_geometry->setStitches(row.mesh, row.stitches, row.positions);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The topstitching the edge under the mouse would get, by the ids of the pieces it shows on.
void GarmentSceneModel::setStitchPreview(const QHash<quint32, QVector<ThreadStitch>>& preview)
{
    for (Row& row : m_rows)
    {
        const QVector<ThreadStitch> wanted = preview.value(row.id);
        if (wanted != row.preview)
        {
            row.preview = wanted;
            row.preview_geometry->setStitches(row.mesh, row.preview, row.positions, preview_scale);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the piece is shown placed on the avatar rather than on the board.
bool GarmentSceneModel::isPlaced(quint32 id) const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [id](const Row& row)
    {
        return row.id == id && row.placed;
    });
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pieces placed on the avatar, where they are shown.
QVector<GarmentSceneModel::Piece> GarmentSceneModel::placedPieces() const
{
    QVector<Piece> pieces;
    for (const Row& row : m_rows)
    {
        if (row.placed)
        {
            Piece piece;
            piece.id = row.id;
            piece.name = row.name;
            piece.color = row.color;
            piece.mesh = row.mesh;
            piece.positions = row.positions;
            piece.grain_angle = row.grain_angle;
            piece.stitches = row.stitches;
            pieces.append(piece);
        }
    }
    return pieces;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::clear()
{
    setSelectedPiece(0);
    setPieces(QVector<Piece>());
    clearAvatar();
}

//---------------------------------------------------------------------------------------------------------------------
quint32 GarmentSceneModel::selectedPiece() const
{
    return m_selected_piece;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Highlights the piece; 0, or a piece the scene doesn't show, highlights none.
void GarmentSceneModel::setSelectedPiece(quint32 id)
{
    const bool shown = std::any_of(m_rows.cbegin(), m_rows.cend(), [id](const Row& row)
    {
        return patternPiece(row.id) == id;
    });
    const quint32 piece_id = shown ? id : 0;

    if (piece_id != m_selected_piece)
    {
        const quint32 previous = m_selected_piece;
        m_selected_piece = piece_id;
        for (int i = 0; i < m_rows.size(); ++i)
        {
            const quint32 row_piece = patternPiece(m_rows.at(i).id);
            if (row_piece == previous || row_piece == piece_id)
            {
                emit dataChanged(index(i), index(i), {SelectedRole});
            }
        }
        emit selectedPieceChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
int GarmentSceneModel::pieceCount() const
{
    return static_cast<int>(m_rows.size());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Middle of all pieces in scene coordinates (cm, y up).
QVector3D GarmentSceneModel::sceneCenter() const
{
    return m_scene_center;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Radius in cm of a sphere around sceneCenter() that holds all pieces.
qreal GarmentSceneModel::sceneRadius() const
{
    return m_scene_radius;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a piece is clicked, with 0 when the click hits no piece.
void GarmentSceneModel::pickPiece(int id)
{
    const quint32 piece_id = id != 0 ? patternPiece(static_cast<quint32>(id)) : 0;
    setSelectedPiece(piece_id);
    emit piecePicked(piece_id);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when the avatar is clicked while arranging, with the point hit in scene coordinates.
void GarmentSceneModel::placeAt(qreal x, qreal y, qreal z)
{
    emit placeRequested(QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a piece is pressed on while arranging, with where the mouse is on the avatar in scene
/// coordinates. A placed piece is picked and follows the mouse from then on; says whether it was a placed piece.
bool GarmentSceneModel::grabPiece(int id, qreal x, qreal y, qreal z)
{
    const bool grabbed = m_arranging && id != 0 && isPlaced(static_cast<quint32>(id));
    if (grabbed)
    {
        pickPiece(id);
        emit grabRequested(static_cast<quint32>(id),
                           QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
    }
    return grabbed;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML while a grabbed piece is dragged, with where the mouse is now, in scene coordinates.
void GarmentSceneModel::dragTo(qreal x, qreal y, qreal z)
{
    emit dragRequested(QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a dragged piece is let go.
void GarmentSceneModel::dropPiece()
{
    emit dropRequested();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The color of the topstitching's thread, or with an invalid color thread matching each piece's cloth.
void GarmentSceneModel::setThreadColor(const QColor& color)
{
    if (color != m_thread_color)
    {
        m_thread_color = color;
        if (!m_rows.isEmpty())
        {
            emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {PieceThreadColorRole});
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The color the thread is on cloth of the given color: the thread's own, or matching the cloth, a little
/// darker on light cloth and lighter on dark.
QColor GarmentSceneModel::threadColor(const QColor& cloth) const
{
    if (m_thread_color.isValid())
    {
        return m_thread_color;
    }
    const qreal target = cloth.lightnessF() > 0.5 ? 0.0 : 1.0;
    return QColor::fromRgbF(static_cast<float>(cloth.redF() + (target - cloth.redF()) * matching_thread_share),
                            static_cast<float>(cloth.greenF() + (target - cloth.greenF()) * matching_thread_share),
                            static_cast<float>(cloth.blueF() + (target - cloth.blueF()) * matching_thread_share));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML with a point picked on a piece, in the coordinates its geometry is in: where that point is
/// in the flat piece, in cm, taken from the nearest point of the piece's mesh as it is shown. Nothing for a piece the
/// scene doesn't show.
QVariant GarmentSceneModel::restPoint(int id, const QVector3D& point) const
{
    for (const Row& row : m_rows)
    {
        if (row.id != static_cast<quint32>(id) || row.mesh.triangleCount() == 0)
        {
            continue;
        }

        const QVector<QVector3D> placed = PieceGeometry::placedPositions(row.mesh, row.positions);
        float nearest = std::numeric_limits<float>::max();
        QPointF rest;
        for (int i = 0; i + 2 < row.mesh.indices.size(); i += 3)
        {
            const int a = static_cast<int>(row.mesh.indices.at(i));
            const int b = static_cast<int>(row.mesh.indices.at(i + 1));
            const int c = static_cast<int>(row.mesh.indices.at(i + 2));
            const QVector3D weights = nearestWeights(point, placed.at(a), placed.at(b), placed.at(c));
            const QVector3D on_mesh = placed.at(a) * weights.x() + placed.at(b) * weights.y()
                                      + placed.at(c) * weights.z();
            const float distance = (on_mesh - point).lengthSquared();
            if (distance < nearest)
            {
                nearest = distance;
                rest = row.mesh.rest_positions.at(a) * weights.x() + row.mesh.rest_positions.at(b) * weights.y()
                       + row.mesh.rest_positions.at(c) * weights.z();
            }
        }
        return rest;
    }
    return QVariant();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief While arranging, clicks on the avatar place the selected piece.
bool GarmentSceneModel::isArranging() const
{
    return m_arranging;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setArranging(bool arranging)
{
    if (arranging != m_arranging)
    {
        m_arranging = arranging;
        emit arrangingChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief What to do next while arranging or simulating; empty otherwise.
QString GarmentSceneModel::hint() const
{
    return m_hint;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setHint(const QString& hint)
{
    if (hint != m_hint)
    {
        m_hint = hint;
        emit hintChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the pieces are colored by how much their cloth is stretched rather than in their own colors.
bool GarmentSceneModel::isStrainShown() const
{
    return m_strain_shown;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setStrainShown(bool shown)
{
    if (shown != m_strain_shown)
    {
        m_strain_shown = shown;
        for (const Row& row : m_rows)
        {
            row.geometry->setMesh(row.mesh, row.positions, m_strain_shown, row.grain_angle);
        }
        emit strainShownChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the pieces are shown in checks that run along their grainlines, to see how they are cut and whether
/// the checks meet at the seams.
bool GarmentSceneModel::isChecksShown() const
{
    return m_checks_shown;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setChecksShown(bool shown)
{
    if (shown != m_checks_shown)
    {
        m_checks_shown = shown;
        emit checksShownChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far the checks repeat, in cm, across and along the grain.
qreal GarmentSceneModel::checkRepeat() const
{
    return check_repeat;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The strain shown in full red, as a share of the drafted size.
qreal GarmentSceneModel::fullStrain() const
{
    return PieceGeometry::fullStrain();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The colors the strain is shown in, for the legend: for none, half the full strain and the full strain.
QVariantList GarmentSceneModel::strainColors() const
{
    QVariantList colors;
    for (const QColor& color : PieceGeometry::strainColors())
    {
        colors.append(color);
    }
    return colors;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Shows the avatar, a body given by its vertex positions in cm. The note says how far it is from the wanted
/// measurements, or is empty.
void GarmentSceneModel::setAvatar(const QVector<QVector3D>& positions, const QVector<quint32>& triangles,
                                  int skin_vertex_count, const QString& note)
{
    // The first avatar frames the view again: the pieces arranged on it were on the board until it came, so the view
    // was framed on them there.
    const bool first_avatar = !m_has_avatar;

    if (m_avatar == nullptr)
    {
        m_avatar = new AvatarGeometry();
        m_avatar->setParent(this);
    }
    m_avatar->setBody(positions, triangles, skin_vertex_count);

    const float largest = std::numeric_limits<float>::max();
    m_avatar_minimum = QVector3D(largest, largest, largest);
    m_avatar_maximum = -m_avatar_minimum;
    for (int i = 0; i < skin_vertex_count; ++i)
    {
        const QVector3D& position = positions.at(i);
        m_avatar_minimum = lowerCorner(m_avatar_minimum, position);
        m_avatar_maximum = upperCorner(m_avatar_maximum, position);
    }

    m_has_avatar = true;
    m_avatar_note = note;
    emit avatarChanged();

    updateSceneBounds();
    if (first_avatar)
    {
        emit framingRequested();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::clearAvatar()
{
    if (m_has_avatar)
    {
        m_has_avatar = false;
        m_avatar_note.clear();
        emit avatarChanged();
        updateSceneBounds();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the pieces' board is moved to: behind the avatar and centred on it, or nowhere without an avatar.
QVector3D GarmentSceneModel::boardOffset() const
{
    return m_board_offset;
}

//---------------------------------------------------------------------------------------------------------------------
bool GarmentSceneModel::hasAvatar() const
{
    return m_has_avatar;
}

//---------------------------------------------------------------------------------------------------------------------
QObject* GarmentSceneModel::avatarGeometry() const
{
    return m_avatar;
}

//---------------------------------------------------------------------------------------------------------------------
QString GarmentSceneModel::avatarNote() const
{
    return m_avatar_note;
}

//---------------------------------------------------------------------------------------------------------------------
// Places the board of pieces and works out what the camera has to see. In 3D the pieces' y axis points up, so their
// rectangle is flipped.
void GarmentSceneModel::updateSceneBounds()
{
    const QVector3D board_center(static_cast<float>(m_piece_bounds.center().x()),
                                 static_cast<float>(-m_piece_bounds.center().y()), 0.0f);
    const QVector3D board_half(static_cast<float>(m_piece_bounds.width() / 2.0),
                               static_cast<float>(m_piece_bounds.height() / 2.0), 0.0f);

    QVector3D minimum = board_center - board_half;
    QVector3D maximum = board_center + board_half;
    m_board_offset = QVector3D();

    if (m_has_avatar)
    {
        // The board stands a little behind the avatar, centred on it, its middle no lower than the avatar's.
        const float middle = qMax(board_half.y(), (m_avatar_minimum.y() + m_avatar_maximum.y()) / 2.0f);
        m_board_offset = QVector3D(-board_center.x(), middle - board_center.y(), m_avatar_minimum.z() - board_gap);

        if (m_piece_bounds.isEmpty())
        {
            minimum = m_avatar_minimum;
            maximum = m_avatar_maximum;
        }
        else
        {
            minimum = lowerCorner(minimum + m_board_offset, m_avatar_minimum);
            maximum = upperCorner(maximum + m_board_offset, m_avatar_maximum);
        }
    }

    m_scene_center = (minimum + maximum) / 2.0f;
    m_scene_radius = static_cast<qreal>((maximum - minimum).length()) / 2.0;
    emit sceneBoundsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
// Lays the row's topstitching, and the stitches an edge under the mouse would get, onto its mesh as it is shown.
void GarmentSceneModel::showStitches(const Row& row) const
{
    row.stitch_geometry->setStitches(row.mesh, row.stitches, row.positions);
    row.preview_geometry->setStitches(row.mesh, row.preview, row.positions, preview_scale);
}
