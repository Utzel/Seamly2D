//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_view_widget.cpp
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

#include "garment_view_widget.h"

#include <QColor>
#include <QLabel>
#include <QList>
#include <QPalette>
#include <QQmlError>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>

#include "../ifc/exception/vexception.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "garment_scene_model.h"

namespace
{
// Edits come in bursts (dragging a point sends one per mouse move), so re-mesh once they pause.
const int rebuild_delay_ms = 150;
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
GarmentViewWidget::GarmentViewWidget(VContainer* data, QWidget* parent)
    : QWidget(parent)
    , m_data(data)
    , m_scene_model(new GarmentSceneModel(this))
    , m_quick_widget(nullptr)
    , m_message_label(new QLabel(this))
    , m_rebuild_timer(new QTimer(this))
    , m_mesher()
    , m_mesh_cache()
    , m_rebuild_pending(false)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_message_label->setWordWrap(true);
    m_message_label->setAlignment(Qt::AlignCenter);
    m_message_label->hide();
    layout->addWidget(m_message_label);

    m_rebuild_timer->setSingleShot(true);
    m_rebuild_timer->setInterval(rebuild_delay_ms);
    connect(m_rebuild_timer, &QTimer::timeout, this, &GarmentViewWidget::rebuildScene);
    connect(m_scene_model, &GarmentSceneModel::piecePicked, this, &GarmentViewWidget::scenePicked);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern changed. The scene is rebuilt after a short pause, or when the view is shown next.
void GarmentViewWidget::updatePieces()
{
    m_rebuild_pending = true;
    if (isVisible())
    {
        m_rebuild_timer->start();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Highlights a piece selected somewhere else, without reporting it back.
void GarmentViewWidget::selectPiece(quint32 id)
{
    m_scene_model->setSelectedPiece(id);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::clear()
{
    m_rebuild_timer->stop();
    m_rebuild_pending = false;
    m_mesh_cache.clear();
    m_scene_model->clear();
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_quick_widget == nullptr)
    {
        createScene();
    }
    if (m_rebuild_pending)
    {
        m_rebuild_timer->start();
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Meshes the pieces included in the layout and hands them to the scene. Pieces whose seam line didn't change
/// keep their mesh.
void GarmentViewWidget::rebuildScene()
{
    m_rebuild_pending = false;

    const QHash<quint32, VPiece>* pieces = m_data->DataPieces();
    QList<quint32> ids = pieces->keys();
    std::sort(ids.begin(), ids.end());

    QVector<GarmentSceneModel::Piece> scene_pieces;
    QHash<quint32, CachedMesh> mesh_cache;
    for (const quint32 id : ids)
    {
        const VPiece& piece = pieces->constFind(id).value();
        if (piece.isInLayout())
        {
            try
            {
                const QVector<QPointF> outline = PieceMesher::pieceOutline(piece, m_data);
                CachedMesh cached = m_mesh_cache.value(id);
                if (cached.outline != outline)
                {
                    cached.outline = outline;
                    cached.mesh = m_mesher.meshPolygon(outline);
                    cached.mesh.piece_id = id;
                }
                mesh_cache.insert(id, cached);

                if (!cached.mesh.isEmpty())
                {
                    const QColor color(piece.getColor());

                    GarmentSceneModel::Piece scene_piece;
                    scene_piece.id = id;
                    scene_piece.name = piece.GetName();
                    scene_piece.color = color.isValid() ? color : QColor(Qt::white);
                    scene_piece.mesh = cached.mesh;
                    scene_pieces.append(scene_piece);
                }
            }
            catch (const VException&)
            {
                // The piece scene already shows what is wrong with a piece whose points can't be found.
            }
        }
    }

    m_mesh_cache = mesh_cache;
    m_scene_model->setPieces(scene_pieces);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::scenePicked(quint32 id)
{
    if (id != 0)
    {
        emit pieceSelected(id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::createScene()
{
    // Created without a parent and added to the layout last: the first Qt Quick widget in a window makes Qt
    // recreate the window, which shows this widget again and would otherwise call createScene() a second time.
    m_quick_widget = new QQuickWidget();
    m_quick_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(m_quick_widget, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status)
    {
        if (status == QQuickWidget::Error)
        {
            QStringList errors;
            for (const QQmlError& error : m_quick_widget->errors())
            {
                errors.append(error.toString());
            }
            showError(errors.join(QLatin1Char('\n')));
        }
    });
    connect(m_quick_widget, &QQuickWidget::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& message)
    {
        showError(message);
    });

    // White, the default piece color, needs a background a little darker than the window to stand out.
    const QPalette colors = palette();
    QVariantMap properties;
    properties.insert(QStringLiteral("sceneModel"), QVariant::fromValue(m_scene_model));
    properties.insert(QStringLiteral("emptyText"), tr("Pieces included in the layout show up here."));
    properties.insert(QStringLiteral("hintText"),
                      tr("Drag to turn, Ctrl+drag to move, scroll to zoom, double-click to fit"));
    properties.insert(QStringLiteral("backgroundColor"), colors.color(QPalette::Window).darker(125));
    properties.insert(QStringLiteral("textColor"), colors.color(QPalette::WindowText));
    properties.insert(QStringLiteral("highlightColor"), colors.color(QPalette::Highlight));
    m_quick_widget->setInitialProperties(properties);
    m_quick_widget->setSource(QUrl(QStringLiteral("qrc:/garment3d/garment_scene.qml")));

    layout()->addWidget(m_quick_widget);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showError(const QString& error)
{
    if (m_quick_widget != nullptr)
    {
        m_quick_widget->hide();
    }
    m_message_label->setText(tr("The 3D view could not be started.\n%1").arg(error));
    m_message_label->show();
}
