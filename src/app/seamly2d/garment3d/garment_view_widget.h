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
#include <QPointF>
#include <QScopedPointer>
#include <QVector3D>
#include <QVector>
#include <QWidget>

#include "../vgarment/body_fitter.h"
#include "../vgarment/body_model.h"
#include "../vgarment/garment_mesh.h"
#include "../vgarment/piece_mesher.h"

class GarmentSceneModel;
class QLabel;
class QQuickWidget;
class QTimer;
class VContainer;

/// @brief Content of the 3D View dock: the pattern's pieces and an avatar fitted to the pattern's measurements, in a
/// 3D scene that follows every edit.
///
/// The Qt Quick scene, and with it the GPU context, is only created the first time the dock is shown, and the
/// pieces are only meshed while it is visible, so the view costs nothing until it is used. The avatar is fitted on a
/// worker thread and only when the measurements change.
class GarmentViewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit           GarmentViewWidget(VContainer* data, QWidget* parent = nullptr);

signals:
    void               pieceSelected(quint32 id);

public slots:
    void               setWearer(qreal gender, qreal age_years);
    void               updatePieces();
    void               selectPiece(quint32 id);
    void               clear();

protected:
    virtual void       showEvent(QShowEvent* event) override;

private slots:
    void               rebuildScene();
    void               scenePicked(quint32 id);
    void               avatarFitted();

private:
    Q_DISABLE_COPY(GarmentViewWidget)

    struct CachedMesh
    {
        QVector<QPointF> outline;
        GarmentMesh      mesh;
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

    VContainer*                m_data;
    GarmentSceneModel*         m_scene_model;
    QQuickWidget*              m_quick_widget;
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

    void               createScene();
    void               showError(const QString& error);
    void               updateAvatar();
    AvatarRequest      wantedAvatar() const;
    QString            avatarNote(const AvatarFit& result) const;
};

#endif // GARMENT_VIEW_WIDGET_H
