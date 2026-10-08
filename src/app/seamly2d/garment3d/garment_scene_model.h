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
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>
#include <QVector>

#include "../vgarment/body_collider.h"
#include "../vgarment/body_wrap.h"
#include "../vgarment/garment_mesh.h"
#include "../vgarment/topstitch.h"
#include "piece_geometry.h"

class AvatarGeometry;
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
    Q_PROPERTY(bool simulating READ isSimulating NOTIFY simulatingChanged)
    Q_PROPERTY(QVariantList pins READ pins NOTIFY pinsChanged)
    Q_PROPERTY(QVariantList arrangementPoints READ arrangementPoints NOTIFY arrangementPointsChanged)
    Q_PROPERTY(QObject* previewGeometry READ previewGeometry NOTIFY previewChanged)
    Q_PROPERTY(bool previewShown READ isPreviewShown NOTIFY previewChanged)
    Q_PROPERTY(QVariantMap gizmo READ gizmo NOTIFY gizmoChanged)
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

    /// The parts of the gizmo of a piece on the avatar, as CLO's: arrows to move it around its part of the body, up or
    /// down it and out from it, and rings to rotate it, lean it and swing it.
    enum GizmoPart
    {
        MoveAcross,
        MoveUp,
        MoveOut,
        Rotate,
        Lean,
        Swing
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
        PieceLinesRole,
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
        QVector<DrawnLine> lines;             ///< lines drawn on its cloth, such as its internal paths
    };

    explicit               GarmentSceneModel(QObject* parent = nullptr);

    virtual int            rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant       data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void                   setPieces(const QVector<Piece>& pieces);
    void                   setPiecePositions(quint32 id, const QVector<QVector3D>& positions);
    void                   setStitches(const QHash<quint32, QVector<ThreadStitch>>& stitches);
    void                   setStitchPreview(const QHash<quint32, QVector<ThreadStitch>>& preview);
    void                   setLines(const QHash<quint32, QVector<DrawnLine>>& lines);
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

    bool                   isSimulating() const;
    void                   setSimulating(bool simulating);
    QVariantList           pins() const;
    void                   setPins(const QVector<QVector3D>& pins);
    QVariantList           arrangementPoints() const;
    void                   setArrangementPoints(const QVector<ArrangementPoint>& points);
    QObject*               previewGeometry() const;
    bool                   isPreviewShown() const;
    void                   setPreview(const GarmentMesh& mesh, const QVector<QVector3D>& positions);
    void                   clearPreview();
    QVariantMap            gizmo() const;
    void                   setGizmo(const QVariantMap& gizmo);

    Q_INVOKABLE void       pickPiece(int id);
    Q_INVOKABLE void       placeAt(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       placeAtPoint(int index);
    Q_INVOKABLE void       previewAt(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       previewAtPoint(int index);
    Q_INVOKABLE void       leaveAvatar();
    Q_INVOKABLE void       showPieceMenu(int id, qreal x, qreal y);
    Q_INVOKABLE bool       grabGizmo(int part);
    Q_INVOKABLE void       dragGizmo(qreal amount);
    Q_INVOKABLE void       dropGizmo();
    Q_INVOKABLE void       hoverGizmo(int part);
    Q_INVOKABLE bool       grabPiece(int id, qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dragTo(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       dropPiece();
    Q_INVOKABLE QVariant   restPoint(int id, const QVector3D& point) const;
    Q_INVOKABLE bool       pullCloth(int id, qreal x, qreal y, qreal z);
    Q_INVOKABLE bool       pullPin(int index);
    Q_INVOKABLE void       pullTo(qreal x, qreal y, qreal z);
    Q_INVOKABLE void       releasePull();
    Q_INVOKABLE void       pinCloth(int id, qreal x, qreal y, qreal z);
    Q_INVOKABLE void       unpin(int index);

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
    void                   simulatingChanged();
    void                   pinsChanged();
    void                   arrangementPointsChanged();
    void                   previewChanged();
    void                   gizmoChanged();
    void                   pullRequested(quint32 id, const QVector3D& point);
    void                   pinPullRequested(int index);
    void                   pullMoved(const QVector3D& point);
    void                   pullReleased();
    void                   pinRequested(quint32 id, const QVector3D& point);
    void                   unpinRequested(int index);
    void                   placeRequested(const QVector3D& point);
    void                   placePointRequested(int index);
    void                   previewRequested(int index, const QVector3D& point);
    void                   previewLeft();
    void                   pieceMenuRequested(const QPointF& at);
    void                   gizmoGrabRequested(int part);
    void                   gizmoDragRequested(qreal amount);
    void                   gizmoDropRequested();
    void                   gizmoHovered(int part);
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
        QVector<DrawnLine> lines;
        PieceGeometry* geometry = nullptr;
        PieceGeometry* outline = nullptr;
        PieceGeometry* edges = nullptr;
        PieceGeometry* line_geometry = nullptr;
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
    bool                   m_simulating;
    QVariantList           m_pins;
    QVariantList           m_arrangement_points;
    PieceGeometry*         m_preview;
    bool                   m_preview_shown;
    QVariantMap            m_gizmo;
    QColor                 m_thread_color;
    QHash<QByteArray, FabricImage> m_images;

    void                   updateSceneBounds();
    void                   updateImages();
    QColor                 clothColor(const Row& row) const;
    void                   showStitches(const Row& row) const;
    void                   showMesh(const Row& row) const;
    void                   showEdges(const Row& row) const;
    void                   showLines(const Row& row) const;
    QVector<QColor>        vertexColors(const Row& row) const;
};

#endif // GARMENT_SCENE_MODEL_H
