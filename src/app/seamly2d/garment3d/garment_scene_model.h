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
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector3D>
#include <QVector>

#include "../vgarment/body_collider.h"
#include "../vgarment/garment_mesh.h"
#include "../vgarment/topstitch.h"

class AvatarGeometry;
class PieceGeometry;
class QQuick3DTextureData;
class StitchGeometry;

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
    Q_PROPERTY(QVector3D avatarFloor READ avatarFloor NOTIFY avatarChanged)
    Q_PROPERTY(qreal avatarReach READ avatarReach NOTIFY avatarChanged)
    Q_PROPERTY(bool arranging READ isArranging NOTIFY arrangingChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)
    Q_PROPERTY(bool fitMapShown READ isFitMapShown NOTIFY fitMapChanged)
    Q_PROPERTY(QVariantList fitColors READ fitColors NOTIFY fitMapChanged)
    Q_PROPERTY(QStringList fitLabels READ fitLabels NOTIFY fitMapChanged)
    Q_PROPERTY(bool checksShown READ isChecksShown NOTIFY checksShownChanged)
    Q_PROPERTY(bool avatarShown READ isAvatarShown NOTIFY avatarShownChanged)
    Q_PROPERTY(bool meshShown READ isMeshShown NOTIFY meshShownChanged)
    Q_PROPERTY(qreal checkRepeat READ checkRepeat CONSTANT)

public:
    /// What the cloth can be colored by, as CLO's fit maps: how much it is stretched, how far it stands off the body,
    /// how hard it presses on it.
    enum class FitMap
    {
        None,
        Strain,
        Ease,
        Pressure
    };

    enum Roles
    {
        PieceIdRole = Qt::UserRole + 1,
        PieceNameRole,
        PieceColorRole,
        PieceGeometryRole,
        PieceOutlineRole,
        PieceStitchesRole,
        PieceStitchPreviewRole,
        PieceThreadColorRole,
        PieceTextureRole,
        PieceTextureSizeRole,
        PieceEdgesRole,
        PieceShownRole,
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
        qreal              grain_angle = 90.0;  ///< degrees anticlockwise from the piece's x axis
        qreal              thickness = 0.0;     ///< of its fabric, in cm
        QByteArray         texture;             ///< an image of its fabric, an image file's bytes; empty for none
        qreal              texture_width = 0.0; ///< how wide the cloth the image shows is, in cm
        QVector<ThreadStitch> stitches;       ///< its topstitching, on its mesh
        QVector<ThreadStitch> preview;        ///< the topstitching an edge under the mouse would get
    };

    explicit               GarmentSceneModel(QObject* parent = nullptr);

    virtual int            rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant       data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void                   setPieces(const QVector<Piece>& pieces);
    void                   setPiecePositions(quint32 id, const QVector<QVector3D>& positions);
    void                   setStitches(const QHash<quint32, QVector<ThreadStitch>>& stitches);
    void                   setStitchPreview(const QHash<quint32, QVector<ThreadStitch>>& preview);
    void                   setThreadColor(const QColor& color);
    QColor                 threadColor(const QColor& cloth) const;
    QColor                 clothColor(quint32 id) const;
    Q_INVOKABLE bool       isPlaced(quint32 id) const;
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
    QVector3D              avatarFloor() const;
    qreal                  avatarReach() const;

    bool                   isArranging() const;
    void                   setArranging(bool arranging);
    QString                hint() const;
    void                   setHint(const QString& hint);

    FitMap                 fitMap() const;
    void                   setFitMap(FitMap map);
    bool                   isFitMapShown() const;
    QVariantList           fitColors() const;
    QStringList            fitLabels() const;
    void                   setBody(const BodyCollider& body);

    bool                   isChecksShown() const;
    void                   setChecksShown(bool shown);
    qreal                  checkRepeat() const;

    bool                   isAvatarShown() const;
    void                   setAvatarShown(bool shown);
    bool                   isMeshShown() const;
    void                   setMeshShown(bool shown);
    QSet<quint32>          hiddenPieces() const;
    void                   setHiddenPieces(const QSet<quint32>& pieces);
    void                   requestView(qreal pitch, qreal yaw);

    Q_INVOKABLE void       pickPiece(int id);
    Q_INVOKABLE void       placeAt(qreal x, qreal y, qreal z);
    Q_INVOKABLE bool       grabPiece(int id, qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dragTo(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dropPiece();
    Q_INVOKABLE QVariant   restPoint(int id, const QVector3D& point) const;

signals:
    void                   pieceCountChanged();
    void                   selectedPieceChanged();
    void                   sceneBoundsChanged();
    void                   avatarChanged();
    void                   framingRequested();
    void                   piecePicked(quint32 id);
    void                   arrangingChanged();
    void                   hintChanged();
    void                   fitMapChanged();
    void                   checksShownChanged();
    void                   avatarShownChanged();
    void                   meshShownChanged();
    void                   viewRequested(qreal pitch, qreal yaw);
    void                   placeRequested(const QVector3D& point);
    void                   grabRequested(quint32 id, const QVector3D& point);
    void                   dragRequested(const QVector3D& point);
    void                   dropRequested();

private:
    Q_DISABLE_COPY(GarmentSceneModel)

    // An image of a fabric as the scene draws it: its pixels, and the color of the cloth seen from afar.
    struct FabricImage
    {
        QQuick3DTextureData* data = nullptr;
        QSize          size;
        QColor         color;
    };

    struct Row
    {
        quint32        id = 0;
        QString        name;
        QColor         color;
        GarmentMesh    mesh;
        QVector<QVector3D> positions;
        qreal          grain_angle = 90.0;
        qreal          thickness = 0.0;
        QByteArray     texture;
        qreal          texture_width = 0.0;
        FabricImage    image;
        bool           placed = false;
        QVector<ThreadStitch> stitches;
        QVector<ThreadStitch> preview;
        PieceGeometry* geometry = nullptr;
        PieceGeometry* outline = nullptr;
        PieceGeometry* edges = nullptr;
        StitchGeometry* stitch_geometry = nullptr;
        StitchGeometry* preview_geometry = nullptr;
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
    FitMap                 m_fit_map;
    BodyCollider           m_body;
    bool                   m_checks_shown;
    bool                   m_avatar_shown;
    bool                   m_mesh_shown;
    QSet<quint32>          m_hidden_pieces;
    QColor                 m_thread_color;
    QHash<QByteArray, FabricImage> m_images;

    void                   updateSceneBounds();
    void                   updateImages();
    QColor                 clothColor(const Row& row) const;
    void                   showStitches(const Row& row) const;
    void                   showMesh(const Row& row) const;
    void                   showEdges(const Row& row) const;
    QVector<QColor>        vertexColors(const Row& row) const;
};

#endif // GARMENT_SCENE_MODEL_H
