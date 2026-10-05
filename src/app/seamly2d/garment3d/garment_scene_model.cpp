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

#include "piece_geometry.h"

//---------------------------------------------------------------------------------------------------------------------
GarmentSceneModel::GarmentSceneModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_rows()
    , m_selected_piece(0)
    , m_scene_center()
    , m_scene_radius(0)
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
            case SelectedRole:
                value = row.id == m_selected_piece;
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
            {SelectedRole, QByteArrayLiteral("selected")}};
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
            row.geometry->setMesh(pieces.at(i).mesh);
            row.outline->setOutline(pieces.at(i).mesh);
        }
        if (!m_rows.isEmpty())
        {
            emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1), {PieceNameRole, PieceColorRole});
        }
    }
    else
    {
        beginResetModel();
        for (const Row& row : m_rows)
        {
            row.geometry->deleteLater();
            row.outline->deleteLater();
        }
        m_rows.clear();

        for (const Piece& piece : pieces)
        {
            Row row;
            row.id = piece.id;
            row.name = piece.name;
            row.color = piece.color;
            row.geometry = new PieceGeometry();
            row.geometry->setParent(this);
            row.geometry->setMesh(piece.mesh);
            row.outline = new PieceGeometry();
            row.outline->setParent(this);
            row.outline->setOutline(piece.mesh);
            m_rows.append(row);
        }
        endResetModel();
        emit pieceCountChanged();

        // Drops the highlight if the selected piece is gone.
        setSelectedPiece(m_selected_piece);
    }

    updateSceneBounds(pieces);
    if (was_empty && !m_rows.isEmpty())
    {
        emit framingRequested();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::clear()
{
    setSelectedPiece(0);
    setPieces(QVector<Piece>());
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
        return row.id == id;
    });
    const quint32 piece_id = shown ? id : 0;

    if (piece_id != m_selected_piece)
    {
        const quint32 previous = m_selected_piece;
        m_selected_piece = piece_id;
        for (int i = 0; i < m_rows.size(); ++i)
        {
            if (m_rows.at(i).id == previous || m_rows.at(i).id == piece_id)
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
    const quint32 piece_id = id > 0 ? static_cast<quint32>(id) : 0;
    setSelectedPiece(piece_id);
    emit piecePicked(piece_id);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentSceneModel::updateSceneBounds(const QVector<Piece>& pieces)
{
    QRectF bounds;
    for (const Piece& piece : pieces)
    {
        bounds = bounds.united(piece.mesh.bounds());
    }

    m_scene_center = QVector3D(static_cast<float>(bounds.center().x()), static_cast<float>(-bounds.center().y()), 0.0f);
    m_scene_radius = qSqrt(bounds.width() * bounds.width() + bounds.height() * bounds.height()) / 2.0;
    emit sceneBoundsChanged();
}
