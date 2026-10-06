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
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVector3D>
#include <QVector>

#include "../vgarment/garment_mesh.h"

class AvatarGeometry;
class PieceGeometry;

/// @brief What the 3D scene shows, for the scene's QML: the pieces, one row each, and the avatar.
///
/// Geometries are kept while the set of pieces stays the same, so editing a piece updates its mesh in place
/// instead of rebuilding the whole scene. Pieces placed on the avatar are drawn where they are put; the others lie
/// on a board, which stands behind the avatar when there is one.
class GarmentSceneModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int pieceCount READ pieceCount NOTIFY pieceCountChanged)
    Q_PROPERTY(quint32 selectedPiece READ selectedPiece NOTIFY selectedPieceChanged)
    Q_PROPERTY(QVector3D sceneCenter READ sceneCenter NOTIFY sceneBoundsChanged)
    Q_PROPERTY(qreal sceneRadius READ sceneRadius NOTIFY sceneBoundsChanged)
    Q_PROPERTY(QVector3D boardOffset READ boardOffset NOTIFY sceneBoundsChanged)
    Q_PROPERTY(bool hasAvatar READ hasAvatar NOTIFY avatarChanged)
    Q_PROPERTY(QObject* avatarGeometry READ avatarGeometry NOTIFY avatarChanged)
    Q_PROPERTY(QString avatarNote READ avatarNote NOTIFY avatarChanged)
    Q_PROPERTY(bool arranging READ isArranging NOTIFY arrangingChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)
    Q_PROPERTY(bool strainShown READ isStrainShown NOTIFY strainShownChanged)
    Q_PROPERTY(qreal fullStrain READ fullStrain CONSTANT)
    Q_PROPERTY(QVariantList strainColors READ strainColors CONSTANT)

public:
    enum Roles
    {
        PieceIdRole = Qt::UserRole + 1,
        PieceNameRole,
        PieceColorRole,
        PieceGeometryRole,
        PieceOutlineRole,
        SelectedRole,
        PlacedRole
    };

    struct Piece
    {
        quint32            id = 0;
        QString            name;
        QColor             color;
        GarmentMesh        mesh;
        QVector<QVector3D> positions;  ///< where the piece is put, in scene coordinates; none for the board
    };

    explicit               GarmentSceneModel(QObject* parent = nullptr);

    virtual int            rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant       data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void                   setPieces(const QVector<Piece>& pieces);
    void                   setPiecePositions(quint32 id, const QVector<QVector3D>& positions);
    bool                   isPlaced(quint32 id) const;
    QVector<Piece>         placedPieces() const;
    void                   clear();

    quint32                selectedPiece() const;
    void                   setSelectedPiece(quint32 id);

    void                   setAvatar(const QVector<QVector3D>& positions, const QVector<quint32>& triangles,
                                     int skin_vertex_count, const QString& note);
    void                   clearAvatar();

    int                    pieceCount() const;
    QVector3D              sceneCenter() const;
    qreal                  sceneRadius() const;
    QVector3D              boardOffset() const;
    bool                   hasAvatar() const;
    QObject*               avatarGeometry() const;
    QString                avatarNote() const;

    bool                   isArranging() const;
    void                   setArranging(bool arranging);
    QString                hint() const;
    void                   setHint(const QString& hint);

    bool                   isStrainShown() const;
    void                   setStrainShown(bool shown);
    qreal                  fullStrain() const;
    QVariantList           strainColors() const;

    Q_INVOKABLE void       pickPiece(int id);
    Q_INVOKABLE void       placeAt(qreal x, qreal y, qreal z);
    Q_INVOKABLE bool       grabPiece(int id, qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dragTo(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dropPiece();

signals:
    void                   pieceCountChanged();
    void                   selectedPieceChanged();
    void                   sceneBoundsChanged();
    void                   avatarChanged();
    void                   framingRequested();
    void                   piecePicked(quint32 id);
    void                   arrangingChanged();
    void                   hintChanged();
    void                   strainShownChanged();
    void                   placeRequested(const QVector3D& point);
    void                   grabRequested(quint32 id, const QVector3D& point);
    void                   dragRequested(const QVector3D& point);
    void                   dropRequested();

private:
    Q_DISABLE_COPY(GarmentSceneModel)

    struct Row
    {
        quint32        id = 0;
        QString        name;
        QColor         color;
        GarmentMesh    mesh;
        QVector<QVector3D> positions;
        bool           placed = false;
        PieceGeometry* geometry = nullptr;
        PieceGeometry* outline = nullptr;
    };

    QVector<Row>           m_rows;
    quint32                m_selected_piece;
    QRectF                 m_piece_bounds;
    QVector3D              m_scene_center;
    qreal                  m_scene_radius;
    QVector3D              m_board_offset;
    AvatarGeometry*        m_avatar;
    bool                   m_has_avatar;
    QVector3D              m_avatar_minimum;
    QVector3D              m_avatar_maximum;
    QString                m_avatar_note;
    bool                   m_arranging;
    QString                m_hint;
    bool                   m_strain_shown;

    void                   updateSceneBounds();
};

#endif // GARMENT_SCENE_MODEL_H
