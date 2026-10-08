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

#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QSet>
#include <QVariantMap>
#include <QtMath>
#include <QtQuick3D/QQuick3DTextureData>

#include <algorithm>
#include <limits>

#include "../vgarment/garment_fit.h"
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

// The most pixels an image of a fabric is drawn with, across it and along it; larger images are scaled down.
const int image_limit = 2048;

// The fit maps' colors, at the values they stand for, from the lowest up: strain in percent, green unstretched to
// dark red twice as stretched as red; ease in cm, red touching the body to blue well off it; pressure in kPa, green
// none to dark red, as tight as compression wear.
struct FitScale
{
    qreal       values[4];
    const char* colors[4];
    const char* unit;
};
const FitScale strain_scale = {{0.0, 5.0, 10.0, 20.0}, {"#3db24a", "#ffd400", "#e61a1a", "#7a0d0d"}, "%"};
const FitScale ease_scale = {{0.0, 1.5, 4.0, 8.0}, {"#e61a1a", "#ffd400", "#3db24a", "#2f6fd6"}, " cm"};
const FitScale pressure_scale = {{0.0, 1.0, 2.0, 4.0}, {"#3db24a", "#ffd400", "#e61a1a", "#7a0d0d"}, " kPa"};

// Pieces a fit map can't tell anything about, as those on the board for ease and pressure, are this grey.
const char* const unmapped_color = "#c8c8c8";

//---------------------------------------------------------------------------------------------------------------------
const FitScale& fitScale(GarmentSceneModel::FitMap map)
{
    return map == GarmentSceneModel::FitMap::Ease       ? ease_scale
           : map == GarmentSceneModel::FitMap::Pressure ? pressure_scale
                                                        : strain_scale;
}

//---------------------------------------------------------------------------------------------------------------------
// The scale's color for the value, between the colors of the values around it; past the last, the last.
QColor scaleColor(const FitScale& scale, qreal value)
{
    int above = 1;
    while (above < 3 && value > scale.values[above])
    {
        ++above;
    }
    const qreal span = scale.values[above] - scale.values[above - 1];
    const qreal t = qBound(0.0, (value - scale.values[above - 1]) / span, 1.0);
    const QColor low(scale.colors[above - 1]);
    const QColor high(scale.colors[above]);
    return QColor::fromRgbF(static_cast<float>(low.redF() + (high.redF() - low.redF()) * t),
                            static_cast<float>(low.greenF() + (high.greenF() - low.greenF()) * t),
                            static_cast<float>(low.blueF() + (high.blueF() - low.blueF()) * t));
}

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
    , m_fit_map(FitMap::None)
    , m_body()
    , m_checks_shown(false)
    , m_avatar_shown(true)
    , m_mesh_shown(false)
    , m_hidden_pieces()
    , m_simulating(false)
    , m_pins()
    , m_arrangement_points()
    , m_preview(nullptr)
    , m_preview_shown(false)
    , m_thread_color()
    , m_images()
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
                value = threadColor(clothColor(row));
                break;
            case PieceTextureRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.image.data));
                break;
            case PieceEdgesRole:
                value = QVariant::fromValue(static_cast<QObject*>(row.edges));
                break;
            case PieceShownRole:
                value = !m_hidden_pieces.contains(patternPiece(row.id));
                break;
            case PieceTextureSizeRole:
                if (row.image.data != nullptr)
                {
                    value = QSizeF(row.texture_width,
                                   row.texture_width * row.image.size.height() / qMax(row.image.size.width(), 1));
                }
                else
                {
                    value = QSizeF();
                }
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
            {PieceTextureRole, QByteArrayLiteral("pieceTexture")},
            {PieceTextureSizeRole, QByteArrayLiteral("pieceTextureSize")},
            {PieceEdgesRole, QByteArrayLiteral("pieceEdges")},
            {PieceShownRole, QByteArrayLiteral("pieceShown")},
            {SelectedRole, QByteArrayLiteral("selected")},
            {PlacedRole, QByteArrayLiteral("placed")}};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Shows these pieces. If they are the same pieces as before, in the same order, only their meshes, names,
/// colors and images are updated, so the scene doesn't flicker while the pattern is edited.
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
            row.thickness = pieces.at(i).thickness;
            row.texture = pieces.at(i).texture;
            row.texture_width = pieces.at(i).texture_width;
            row.placed = !row.positions.isEmpty();
            row.stitches = pieces.at(i).stitches;
            row.preview = pieces.at(i).preview;
            showMesh(row);
            row.outline->setOutline(row.mesh, row.positions, row.thickness);
            showEdges(row);
            showStitches(row);
        }
        updateImages();
        if (!m_rows.isEmpty())
        {
            emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                             {PieceNameRole, PieceColorRole, PieceThreadColorRole, PieceTextureRole,
                              PieceTextureSizeRole, PlacedRole});
        }
    }
    else
    {
        beginResetModel();
        for (const Row& row : m_rows)
        {
            row.geometry->deleteLater();
            row.outline->deleteLater();
            row.edges->deleteLater();
            row.stitch_geometry->deleteLater();
            row.preview_geometry->deleteLater();
        }
        m_rows.clear();

        // New pieces get new images of their fabrics too: once the pieces drawn in an image are gone, the scene lets
        // go of its pixels and doesn't take them up again for other pieces.
        for (const FabricImage& image : m_images)
        {
            if (image.data != nullptr)
            {
                image.data->deleteLater();
            }
        }
        m_images.clear();

        for (const Piece& piece : pieces)
        {
            Row row;
            row.id = piece.id;
            row.name = piece.name;
            row.color = piece.color;
            row.mesh = piece.mesh;
            row.positions = piece.positions;
            row.grain_angle = piece.grain_angle;
            row.thickness = piece.thickness;
            row.texture = piece.texture;
            row.texture_width = piece.texture_width;
            row.placed = !piece.positions.isEmpty();
            row.geometry = new PieceGeometry();
            row.geometry->setParent(this);
            showMesh(row);
            row.outline = new PieceGeometry();
            row.outline->setParent(this);
            row.outline->setOutline(piece.mesh, piece.positions, piece.thickness);
            row.edges = new PieceGeometry();
            row.edges->setParent(this);
            showEdges(row);
            row.stitches = piece.stitches;
            row.preview = piece.preview;
            row.stitch_geometry = new StitchGeometry();
            row.stitch_geometry->setParent(this);
            row.preview_geometry = new StitchGeometry();
            row.preview_geometry->setParent(this);
            showStitches(row);
            m_rows.append(row);
        }
        updateImages();
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
            showMesh(row);
            row.outline->setOutline(row.mesh, positions, row.thickness);
            showEdges(row);
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
            row.stitch_geometry->setStitches(row.mesh, row.stitches, row.positions, 1.0, row.thickness);
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
            row.preview_geometry->setStitches(row.mesh, row.preview, row.positions, preview_scale, row.thickness);
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
            piece.thickness = row.thickness;
            piece.texture = row.texture;
            piece.texture_width = row.texture_width;
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
/// @brief What the cloth is colored by instead of the pieces' own colors, if anything.
GarmentSceneModel::FitMap GarmentSceneModel::fitMap() const
{
    return m_fit_map;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setFitMap(FitMap map)
{
    if (map != m_fit_map)
    {
        m_fit_map = map;
        for (const Row& row : m_rows)
        {
            showMesh(row);
        }
        emit fitMapChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool GarmentSceneModel::isFitMapShown() const
{
    return m_fit_map != FitMap::None;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fit map's colors, for its legend: at the four values fitLabels() names, from the lowest up.
QVariantList GarmentSceneModel::fitColors() const
{
    QVariantList colors;
    for (const char* const color : fitScale(m_fit_map).colors)
    {
        colors.append(QColor(color));
    }
    return colors;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The values the fit map's colors stand for, from the lowest up, the last one and beyond.
QStringList GarmentSceneModel::fitLabels() const
{
    const FitScale& scale = fitScale(m_fit_map);
    QStringList labels;
    for (int i = 0; i < 4; ++i)
    {
        labels.append(QString::number(scale.values[i]) + (i == 3 ? QStringLiteral("+") : QString())
                      + QString::fromLatin1(scale.unit));
    }
    return labels;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar's body, which the ease and pressure maps measure the cloth against.
void GarmentSceneModel::setBody(const BodyCollider& body)
{
    m_body = body;
    if (m_fit_map == FitMap::Ease || m_fit_map == FitMap::Pressure)
    {
        for (const Row& row : m_rows)
        {
            showMesh(row);
        }
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
/// @brief Whether the avatar is shown. Hidden, the cloth still drapes on it, and it shows while arranging, to put
/// pieces on.
bool GarmentSceneModel::isAvatarShown() const
{
    return m_avatar_shown;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setAvatarShown(bool shown)
{
    if (shown != m_avatar_shown)
    {
        m_avatar_shown = shown;
        emit avatarShownChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the edges of the triangles the cloth is made of are shown, as the drape works them out.
bool GarmentSceneModel::isMeshShown() const
{
    return m_mesh_shown;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setMeshShown(bool shown)
{
    if (shown != m_mesh_shown)
    {
        m_mesh_shown = shown;
        for (const Row& row : m_rows)
        {
            showEdges(row);
        }
        emit meshShownChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern pieces not shown, with their copies; they still drape. A hidden piece isn't selected.
QSet<quint32> GarmentSceneModel::hiddenPieces() const
{
    return m_hidden_pieces;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setHiddenPieces(const QSet<quint32>& pieces)
{
    if (pieces != m_hidden_pieces)
    {
        m_hidden_pieces = pieces;
        if (m_hidden_pieces.contains(m_selected_piece))
        {
            setSelectedPiece(0);
        }
        if (!m_rows.isEmpty())
        {
            emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {PieceShownRole});
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the cloth is draping, which lets it be pulled with the mouse.
bool GarmentSceneModel::isSimulating() const
{
    return m_simulating;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setSimulating(bool simulating)
{
    if (simulating != m_simulating)
    {
        m_simulating = simulating;
        emit simulatingChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the pins holding the cloth are, in scene coordinates.
QVariantList GarmentSceneModel::pins() const
{
    return m_pins;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setPins(const QVector<QVector3D>& pins)
{
    QVariantList shown;
    for (const QVector3D& pin : pins)
    {
        shown.append(pin);
    }
    if (shown != m_pins)
    {
        m_pins = shown;
        emit pinsChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar's arrangement points, for QML: their names, where each is, in scene coordinates, and which way
/// it faces.
QVariantList GarmentSceneModel::arrangementPoints() const
{
    return m_arrangement_points;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::setArrangementPoints(const QVector<ArrangementPoint>& points)
{
    QVariantList shown;
    for (const ArrangementPoint& point : points)
    {
        QVariantMap item;
        item.insert(QStringLiteral("name"), point.name);
        item.insert(QStringLiteral("position"), point.position);
        item.insert(QStringLiteral("normal"), point.normal);
        shown.append(item);
    }
    if (shown != m_arrangement_points)
    {
        m_arrangement_points = shown;
        emit arrangementPointsChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the selected piece would go while arranging, in scene coordinates as placed pieces, for QML.
QObject* GarmentSceneModel::previewGeometry() const
{
    return m_preview;
}

//---------------------------------------------------------------------------------------------------------------------
bool GarmentSceneModel::isPreviewShown() const
{
    return m_preview_shown;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Shows where the selected piece would go: its mesh, with a mirrored copy's after it, at these positions.
void GarmentSceneModel::setPreview(const GarmentMesh& mesh, const QVector<QVector3D>& positions)
{
    if (m_preview == nullptr)
    {
        m_preview = new PieceGeometry();
        m_preview->setParent(this);
    }
    m_preview->setMesh(mesh, positions);
    m_preview_shown = true;
    emit previewChanged();
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::clearPreview()
{
    if (m_preview_shown)
    {
        m_preview_shown = false;
        emit previewChanged();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when an arrangement point is clicked while arranging: the selected piece goes there.
void GarmentSceneModel::placeAtPoint(int index)
{
    if (index >= 0 && index < m_arrangement_points.size())
    {
        emit placePointRequested(index);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML while arranging when the mouse is over the avatar, at a point in scene coordinates: shows
/// where the selected piece would go if it were put there.
void GarmentSceneModel::previewAt(qreal x, qreal y, qreal z)
{
    emit previewRequested(-1, QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML while arranging when the mouse is over an arrangement point: shows where the selected piece
/// would go if it were put there.
void GarmentSceneModel::previewAtPoint(int index)
{
    if (index >= 0 && index < m_arrangement_points.size())
    {
        emit previewRequested(index, m_arrangement_points.at(index).toMap().value(QStringLiteral("position"))
                                         .value<QVector3D>());
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML while arranging when the mouse is off the avatar and its points.
void GarmentSceneModel::leaveAvatar()
{
    emit previewLeft();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a piece on the avatar is right-clicked, at a point of the view: picks it, and asks for
/// what can be done with it.
void GarmentSceneModel::showPieceMenu(int id, qreal x, qreal y)
{
    if (id != 0 && isPlaced(static_cast<quint32>(id)))
    {
        pickPiece(id);
        emit pieceMenuRequested(QPointF(x, y));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when the cloth of a piece is pressed on while draping, at a point in scene coordinates: a
/// piece on the avatar is taken hold of there and follows the mouse until let go. Says whether it was taken hold of.
bool GarmentSceneModel::pullCloth(int id, qreal x, qreal y, qreal z)
{
    const bool pulled = m_simulating && id != 0 && isPlaced(static_cast<quint32>(id));
    if (pulled)
    {
        emit pullRequested(static_cast<quint32>(id),
                           QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
    }
    return pulled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a pin is pressed on while draping: it follows the mouse, the cloth with it. Says whether
/// it was taken hold of.
bool GarmentSceneModel::pullPin(int index)
{
    const bool pulled = m_simulating && index >= 0 && index < m_pins.size();
    if (pulled)
    {
        emit pinPullRequested(index);
    }
    return pulled;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML while the cloth or a pin is held, with where the mouse is, in scene coordinates.
void GarmentSceneModel::pullTo(qreal x, qreal y, qreal z)
{
    emit pullMoved(QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when the cloth or a pin held is let go.
void GarmentSceneModel::releasePull()
{
    emit pullReleased();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a piece is clicked to pin it, at a point in scene coordinates; only pieces on the avatar
/// are pinned.
void GarmentSceneModel::pinCloth(int id, qreal x, qreal y, qreal z)
{
    if (id != 0 && isPlaced(static_cast<quint32>(id)))
    {
        emit pinRequested(static_cast<quint32>(id),
                          QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Called from QML when a pin is clicked to take it out.
void GarmentSceneModel::unpin(int index)
{
    if (index >= 0 && index < m_pins.size())
    {
        emit unpinRequested(index);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Asks the scene to look at everything from a direction: turned up or down by the pitch and around by the
/// yaw, in degrees, 0 and 0 from the front.
void GarmentSceneModel::requestView(qreal pitch, qreal yaw)
{
    emit viewRequested(pitch, yaw);
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
/// @brief Where the avatar stands: the point of the floor under its middle.
QVector3D GarmentSceneModel::avatarFloor() const
{
    const QVector3D middle = (m_avatar_minimum + m_avatar_maximum) / 2.0f;
    return m_has_avatar ? QVector3D(middle.x(), m_avatar_minimum.y(), middle.z()) : QVector3D();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far the avatar reaches out from its middle, arms and all, in cm.
qreal GarmentSceneModel::avatarReach() const
{
    return m_has_avatar ? static_cast<qreal>((m_avatar_maximum - m_avatar_minimum).length()) / 2.0 : 0.0;
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
// Gives each row the image of its fabric, made once for all rows with the same image, and lets go of images no row
// has any more. An image that can't be read is none.
void GarmentSceneModel::updateImages()
{
    QSet<QByteArray> used;
    for (Row& row : m_rows)
    {
        row.image = FabricImage();
        if (row.texture.isEmpty() || row.texture_width <= 0)
        {
            continue;
        }
        used.insert(row.texture);

        auto found = m_images.find(row.texture);
        if (found == m_images.end())
        {
            FabricImage image;
            QImage pixels = QImage::fromData(row.texture);
            if (!pixels.isNull())
            {
                if (pixels.width() > image_limit || pixels.height() > image_limit)
                {
                    pixels = pixels.scaled(image_limit, image_limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }

                // Cloth is drawn solid: what an image leaves see-through is white.
                QImage opaque(pixels.size(), QImage::Format_RGBA8888);
                opaque.fill(Qt::white);
                QPainter painter(&opaque);
                painter.drawImage(0, 0, pixels);
                painter.end();

                image.data = new QQuick3DTextureData();
                image.data->setParent(this);
                image.data->setSize(opaque.size());
                image.data->setFormat(QQuick3DTextureData::RGBA8);
                image.data->setTextureData(QByteArray(reinterpret_cast<const char*>(opaque.constBits()),
                                                      static_cast<int>(opaque.sizeInBytes())));
                image.size = opaque.size();
                image.color = opaque.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixelColor(0, 0);
            }
            found = m_images.insert(row.texture, image);
        }
        row.image = found.value();
    }

    for (auto image = m_images.begin(); image != m_images.end();)
    {
        if (used.contains(image.key()))
        {
            ++image;
        }
        else
        {
            if (image.value().data != nullptr)
            {
                image.value().data->deleteLater();
            }
            image = m_images.erase(image);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The color the piece's cloth looks from afar: its fabric image's, or its own; invalid for a piece the scene
/// doesn't show.
QColor GarmentSceneModel::clothColor(quint32 id) const
{
    for (const Row& row : m_rows)
    {
        if (row.id == id)
        {
            return clothColor(row);
        }
    }
    return QColor();
}

//---------------------------------------------------------------------------------------------------------------------
// The color the row's cloth looks from afar: its image's, or its own.
QColor GarmentSceneModel::clothColor(const Row& row) const
{
    return row.image.data != nullptr ? row.image.color : row.color;
}

//---------------------------------------------------------------------------------------------------------------------
// Shows the row's mesh where it is, colored by the fit map if one is shown.
void GarmentSceneModel::showMesh(const Row& row) const
{
    row.geometry->setMesh(row.mesh, row.positions, vertexColors(row), row.grain_angle, row.thickness);
}

//---------------------------------------------------------------------------------------------------------------------
// The color of each of the row's vertices on the fit map shown; none without one. Ease and pressure only tell
// something about pieces on the avatar.
QVector<QColor> GarmentSceneModel::vertexColors(const Row& row) const
{
    QVector<QColor> colors;
    if (m_fit_map == FitMap::None)
    {
        return colors;
    }

    QVector<qreal> values;
    if (m_fit_map == FitMap::Strain)
    {
        values = row.mesh.strain(PieceGeometry::placedPositions(row.mesh, row.positions));
        for (qreal& value : values)
        {
            value *= 100.0;
        }
    }
    else if (!row.placed || m_body.isEmpty())
    {
        return QVector<QColor>(row.mesh.vertexCount(), QColor(unmapped_color));
    }
    else if (m_fit_map == FitMap::Ease)
    {
        values = GarmentFit::ease(m_body, row.positions);
    }
    else
    {
        values = GarmentFit::pressure(row.mesh, m_body, row.positions);
    }

    const FitScale& scale = fitScale(m_fit_map);
    colors.reserve(values.size());
    for (const qreal value : values)
    {
        colors.append(scaleColor(scale, value));
    }
    return colors;
}

//---------------------------------------------------------------------------------------------------------------------
// The edges of the row's triangles where it is, if the mesh is shown; nothing otherwise.
void GarmentSceneModel::showEdges(const Row& row) const
{
    row.edges->setEdges(m_mesh_shown ? row.mesh : GarmentMesh(), row.positions, row.thickness);
}

//---------------------------------------------------------------------------------------------------------------------
// Lays the row's topstitching, and the stitches an edge under the mouse would get, onto its mesh as it is shown.
void GarmentSceneModel::showStitches(const Row& row) const
{
    row.stitch_geometry->setStitches(row.mesh, row.stitches, row.positions, 1.0, row.thickness);
    row.preview_geometry->setStitches(row.mesh, row.preview, row.positions, preview_scale, row.thickness);
}
