//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_view_widget.h
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

#ifndef GARMENT_VIEW_WIDGET_H
#define GARMENT_VIEW_WIDGET_H

#include <QFutureWatcher>
#include <QHash>
#include <QSet>
#include <QPointF>
#include <QScopedPointer>
#include <QVector3D>
#include <QVector>
#include <QWidget>

#include "../vgarment/body_collider.h"
#include "../vgarment/body_fitter.h"
#include "../vgarment/body_model.h"
#include "../vgarment/body_wrap.h"
#include "../vgarment/garment_export.h"
#include "../vgarment/garment_mesh.h"
#include "../vgarment/garment_symmetry.h"
#include "../vgarment/piece_mesher.h"
#include "../vgarment/piece_outline.h"
#include "../vgarment/topstitch.h"

class DrapeRunner;
class GarmentSceneModel;
class QAction;
class QActionGroup;
class QComboBox;
class QIcon;
class QLabel;
class QQuickView;
class QTimer;
class SeamEditor;
class StitchEditor;
class VAbstractPattern;
class VContainer;
struct VPieceArrangement;
struct VSeam;
struct VSeamSide;
struct VTopstitches;

/// @brief Content of the 3D View dock: the pattern's pieces and an avatar fitted to the pattern's measurements, in a
/// 3D scene that follows every edit, and the seams that sew the pieces together.
///
/// The Qt Quick scene, and with it the GPU context, is only created the first time the dock is shown, and the
/// pieces are only meshed while it is visible, so the view costs nothing until it is used. The avatar is fitted on a
/// worker thread and only when the measurements change. Seams are sewn on the board of pieces and pieces arranged on
/// the avatar, both through the undo stack like any other change to the pattern. The drape is simulated on a thread
/// of its own.
class GarmentViewWidget : public QWidget
{
    Q_OBJECT

public:
                       GarmentViewWidget(VContainer* data, VAbstractPattern* doc, QWidget* parent = nullptr);
    virtual           ~GarmentViewWidget();

signals:
    void               pieceSelected(quint32 id);

public slots:
    void               setWearer(qreal gender, qreal age_years);
    void               updatePieces();
    void               selectPiece(quint32 id);
    void               clear();

protected:
    virtual void       showEvent(QShowEvent* event) override;
    virtual void       changeEvent(QEvent* event) override;
    virtual void       hideEvent(QHideEvent* event) override;
    virtual bool       eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void               rebuildScene();
    void               scenePicked(quint32 id);
    void               avatarFitted();
    void               updateSeams();
    void               updateArrangements();
    void               updateFabrics();
    void               chooseFabric(int index);
    void               flipSeam();
    void               removeSelected();
    void               cancel();
    void               setArranging(bool arranging);
    void               placePiece(const QVector3D& point);
    void               grabPiece(quint32 id, const QVector3D& point);
    void               dragPiece(const QVector3D& point);
    void               dropPiece();
    void               setSimulating(bool simulating);
    void               resetDrape();
    void               setFine(bool fine);
    void               setStitching(bool stitching);
    void               stitchEveryEdge(bool every);
    void               chooseStitchStyle(QAction* action);
    void               chooseThread(QAction* action);
    void               updateTopstitches();
    void               showStitches();
    void               showStitchPreview();
    void               showSeamsOnAvatar();
    void               exportDrape();
    void               drapeFrame(int generation, const QVector<QVector3D>& positions);
    void               drapeSettled(int generation);
    void               updateActions();

private:
    Q_DISABLE_COPY(GarmentViewWidget)

    // A piece's mesh as drafted, for the board, and as it is in the garment: unfolded for a piece cut on the fold,
    // with a mirrored copy for a piece cut twice.
    struct CachedMesh
    {
        PieceOutline  outline;
        PieceSymmetry wanted = PieceSymmetry::Single;
        qreal         edge_length = 0;
        GarmentMesh   mesh;
        PieceSymmetry symmetry = PieceSymmetry::Single;
        quint32       fold_start = 0;
        quint32       fold_end = 0;
        GarmentMesh   garment_mesh;
        GarmentMesh   mirror_mesh;
    };

    // A piece in the garment: a pattern piece or a mirrored copy, with the mesh it is simulated with and the way its
    // grain runs in it.
    struct GarmentPiece
    {
        quint32     id = 0;
        GarmentMesh mesh;
        qreal       grain_angle = 90.0;
    };

    // What an avatar is fitted to; a new fit only starts when this changes.
    struct AvatarRequest
    {
        BodyMeasurements wanted;
        qreal            gender = 0.5;
        qreal            age = 0.5;

        bool             operator==(const AvatarRequest& other) const;
        bool             hasMeasurements() const;
    };

    struct AvatarFit
    {
        quint64            generation = 0;
        AvatarRequest      request;
        BodyFit            fit;
        QVector<QVector3D> positions;
    };

    // A placed piece being dragged around the part of the body it is on.
    struct PieceDrag
    {
        quint32          piece = 0;         ///< the pattern piece; 0 while none is dragged
        bool             mirrored = false;  ///< held by its copy on the other side of the body
        PieceArrangement grabbed;           ///< where on the piece's part the mouse took hold of it
        PieceArrangement start;             ///< where the piece was arranged then
        PieceArrangement current;
        QHash<quint32, QVector<QVector3D>> draped;  ///< the drape of the piece and its copy, back if called off
    };

    // Where a piece's vertices are among all the vertices of the drape being simulated.
    struct DrapePiece
    {
        quint32 id = 0;
        int     offset = 0;
        int     count = 0;
    };

    VContainer*                m_data;
    VAbstractPattern*          m_doc;
    GarmentSceneModel*         m_scene_model;
    SeamEditor*                m_seam_editor;
    StitchEditor*              m_stitch_editor;
    QAction*                   m_sew_action;
    QAction*                   m_flip_action;
    QAction*                   m_remove_action;
    QAction*                   m_topstitch_action;
    QAction*                   m_every_edge_action;
    QActionGroup*              m_stitch_styles;
    QActionGroup*              m_threads;
    QAction*                   m_other_thread_action;
    QAction*                   m_cancel_action;
    QAction*                   m_arrange_action;
    QAction*                   m_simulate_action;
    QAction*                   m_reset_action;
    QAction*                   m_fine_action;
    QAction*                   m_strain_action;
    QAction*                   m_checks_action;
    QAction*                   m_export_action;
    QComboBox*                 m_fabric_box;
    QQuickView*                m_quick_view;
    QWidget*                   m_view_container;
    QLabel*                    m_message_label;
    QTimer*                    m_rebuild_timer;
    PieceMesher                m_mesher;
    QHash<quint32, CachedMesh> m_mesh_cache;
    bool                       m_rebuild_pending;
    qreal                      m_wearer_gender;
    qreal                      m_wearer_age;
    QScopedPointer<BodyModel>  m_body_model;
    QFutureWatcher<AvatarFit>* m_fit_watcher;
    AvatarRequest              m_avatar_request;
    bool                       m_has_avatar_request;
    quint64                    m_avatar_generation;
    QScopedPointer<BodyWrap>   m_wrap;
    BodyCollider               m_collider;
    QHash<quint32, PieceArrangement>   m_arrangements;
    QHash<quint32, QVector<QVector3D>> m_draped;
    QSet<quint32>              m_turned_pairs;
    QVector<DrapePiece>        m_drape_pieces;
    QVector<GarmentPiece>      m_garment_pieces;
    DrapeRunner*               m_runner;
    PieceDrag                  m_drag;

    void               createScene();
    void               createToolBar();
    void               updateIcons();
    QIcon              toolIcon(const QString& name) const;
    void               showError(const QString& error);
    void               sewSeam(const VSeam& seam);
    void               saveSeams(const QString& text, const QVector<VSeam>& seams);
    void               saveArrangements(const QString& text, const QVector<VPieceArrangement>& arrangements);
    void               saveTopstitches(const VTopstitches& topstitches, const QString& text);
    QVector<QVector<QPointF>> stitchedPaths(const VPiece& piece) const;
    void               storeArrangement(quint32 piece, const PieceArrangement& wanted, const QString& text);
    void               showArrangement(quint32 piece, const PieceArrangement& arrangement);
    void               callOffDrag();
    void               showPlaced(quint32 piece);
    void               readArrangements();
    qreal              grainAngle(const VPiece& piece) const;
    bool               carryDrape(quint32 id, const GarmentMesh& before, const GarmentMesh& after);
    QString            fabricTitle(const QString& fabric) const;
    QString            stitchStyleTitle(const TopstitchStyle& style) const;
    QVector<ExportMesh> exportMeshes() const;
    QVector<QVector3D> piecePositions(quint32 id, const GarmentMesh& mesh) const;
    QSet<quint32>      turnedPairs() const;
    qreal              acrossBody(const GarmentMesh& mesh, const QVector<QVector3D>& positions,
                                  const VSeamSide& side) const;
    CachedMesh         garmentMeshes(quint32 id, const PieceOutline& outline, PieceSymmetry wanted) const;
    void               startSimulation();
    void               updateHint();
    void               updateAvatar();
    AvatarRequest      wantedAvatar() const;
    QString            avatarNote(const AvatarFit& result) const;
};

#endif // GARMENT_VIEW_WIDGET_H
