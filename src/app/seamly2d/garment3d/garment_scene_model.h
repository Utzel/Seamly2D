//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_scene_model.h
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

#ifndef GARMENT_SCENE_MODEL_H
#define GARMENT_SCENE_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVector3D>
#include <QVector>

#include "../vgarment/garment_mesh.h"

class PieceGeometry;

/// @brief The pieces the 3D scene shows, one row each, for the scene's QML.
///
/// Geometries are kept while the set of pieces stays the same, so editing a piece updates its mesh in place
/// instead of rebuilding the whole scene.
class GarmentSceneModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int pieceCount READ pieceCount NOTIFY pieceCountChanged)
    Q_PROPERTY(quint32 selectedPiece READ selectedPiece NOTIFY selectedPieceChanged)
    Q_PROPERTY(QVector3D sceneCenter READ sceneCenter NOTIFY sceneBoundsChanged)
    Q_PROPERTY(qreal sceneRadius READ sceneRadius NOTIFY sceneBoundsChanged)

public:
    enum Roles
    {
        PieceIdRole = Qt::UserRole + 1,
        PieceNameRole,
        PieceColorRole,
        PieceGeometryRole,
        PieceOutlineRole,
        SelectedRole
    };

    struct Piece
    {
        quint32     id = 0;
        QString     name;
        QColor      color;
        GarmentMesh mesh;
    };

    explicit               GarmentSceneModel(QObject* parent = nullptr);

    virtual int            rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant       data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void                   setPieces(const QVector<Piece>& pieces);
    void                   clear();

    quint32                selectedPiece() const;
    void                   setSelectedPiece(quint32 id);

    int                    pieceCount() const;
    QVector3D              sceneCenter() const;
    qreal                  sceneRadius() const;

    Q_INVOKABLE void       pickPiece(int id);

signals:
    void                   pieceCountChanged();
    void                   selectedPieceChanged();
    void                   sceneBoundsChanged();
    void                   framingRequested();
    void                   piecePicked(quint32 id);

private:
    Q_DISABLE_COPY(GarmentSceneModel)

    struct Row
    {
        quint32        id = 0;
        QString        name;
        QColor         color;
        PieceGeometry* geometry = nullptr;
        PieceGeometry* outline = nullptr;
    };

    QVector<Row>           m_rows;
    quint32                m_selected_piece;
    QVector3D              m_scene_center;
    qreal                  m_scene_radius;

    void                   updateSceneBounds(const QVector<Piece>& pieces);
};

#endif // GARMENT_SCENE_MODEL_H
