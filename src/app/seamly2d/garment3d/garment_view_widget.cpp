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

#include <QAction>
#include <QActionGroup>
#include <QBuffer>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMessageBox>
#include <QList>
#include <QMap>
#include <QPalette>
#include <QPixmap>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <tuple>
#include <utility>

#include "../ifc/exception/vexception.h"
#include "../ifc/exception/vexceptionbadid.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../qmuparser/qmuparsererror.h"
#include "../vgeometry/vpointf.h"
#include "../vmisc/def.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"
#include "../vpatterndb/calculator.h"
#include "../vpatterndb/floatItemData/vgrainlinedata.h"
#include "../vpatterndb/floatItemData/vpiecelabeldata.h"
#include "../vpatterndb/measurements_def.h"
#include "../vpatterndb/variables/vinternalvariable.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vpatterndb/vpiecepath.h"
#include "../vgarment/cloth_solver.h"
#include "../vgarment/fabric.h"
#include "../vgarment/standard_sizes.h"
#include "../vtools/undocommands/save_arrangements.h"
#include "../vtools/undocommands/save_avatar.h"
#include "../vtools/undocommands/save_fabrics.h"
#include "../vtools/undocommands/save_elastics.h"
#include "../vtools/undocommands/save_folds.h"
#include "../vtools/undocommands/save_layers.h"
#include "../vtools/undocommands/save_seams.h"
#include "../vtools/undocommands/save_topstitches.h"
#include "avatar_dialog.h"
#include "drape_runner.h"
#include "flow_layout.h"
#include "elastic_editor.h"
#include "fabric_dialog.h"
#include "fabric_library.h"
#include "fold_editor.h"
#include "garment_scene_model.h"
#include "piece_geometry.h"
#include "seam_editor.h"
#include "stitch_editor.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// How the piece is made up into the garment, from what its label says.
PieceSymmetry symmetryOf(const VPiece& piece)
{
    const VPieceLabelData& label = piece.GetPatternPieceData();
    PieceSymmetry symmetry = PieceSymmetry::Single;
    if (label.IsOnFold())
    {
        symmetry = PieceSymmetry::Fold;
    }
    else if (label.GetQuantity() >= 2)
    {
        symmetry = PieceSymmetry::Pair;
    }
    return symmetry;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether two arrangements put a piece in the same place, the same way round.
bool sameArrangement(const PieceArrangement& one, const PieceArrangement& other)
{
    return one.part == other.part && qFuzzyCompare(1.0 + one.angle, 1.0 + other.angle)
           && qFuzzyCompare(1.0 + one.height, 1.0 + other.height)
           && qFuzzyCompare(1.0 + one.rotation, 1.0 + other.rotation) && one.turned_over == other.turned_over
           && qFuzzyCompare(1.0 + one.distance, 1.0 + other.distance) && qFuzzyCompare(1.0 + one.lean, 1.0 + other.lean)
           && qFuzzyCompare(1.0 + one.swing, 1.0 + other.swing);
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern piece a piece of the garment is, or is the mirrored copy of.
quint32 patternPiece(quint32 id)
{
    return PieceOutline::isMirrorId(id) ? PieceOutline::mirrorId(id) : id;
}

// Edits come in bursts (dragging a point sends one per mouse move), so re-mesh once they pause.
const int rebuild_delay_ms = 150;

// A preview of where a piece would go stands this many cm further out than a piece put there, so it shows in front of
// the piece when that is there already.
const qreal preview_out = 1.0;

// A piece moved in towards the body by its gizmo stops this many cm in from where pieces start out, still clear of the
// skin; leaning and swinging go at most this many degrees either way.
const qreal nearest_distance = -1.5;
const qreal steepest_turn = 90.0;

// Turned by its gizmo, a piece snaps to the nearest multiple of this many degrees when it comes this close to it.
const qreal snap_angle = 45.0;
const qreal snap_reach = 3.0;

//---------------------------------------------------------------------------------------------------------------------
// The angle, snapped to a multiple of snap_angle when it is that close to one.
qreal snappedAngle(qreal degrees)
{
    const qreal nearest = qRound(degrees / snap_angle) * snap_angle;
    return qAbs(degrees - nearest) <= snap_reach ? nearest : degrees;
}

// Fitted measurements this far off, in cm, are mentioned in the avatar's note.
const qreal note_tolerance = 1.0;

// Birth dates further back than this are taken as not given; SeamlyMe's default is 1800-01-01.
const qreal oldest_age = 110.0;

// Age used when the measurements don't say, MakeHuman's young adult.
const qreal default_age = 25.0;

// The toolbar's icons are drawn this many pixels wide, and twice that for high resolution screens.
const int icon_size = 32;

// Gap between the toolbar's groups side by side, in pixels.
const int tool_group_spacing = 6;

// Fabrics give their thickness in mm, the scene is in cm.
const qreal millimetres_per_cm = 10.0;

// How wide the cloth an image of a fabric shows is taken to be until said otherwise, and how narrow and how wide it
// can be said to be, in cm.
const qreal default_image_width = 10.0;
const qreal narrowest_image_width = 0.5;
const qreal widest_image_width = 500.0;

// Images of fabrics are kept in the pattern at most this many pixels across and along; larger ones are scaled down.
const int kept_image_limit = 2048;

// The items of the fabric box that are no fabrics have this role: making a new one, editing the one shown, adding it
// to the fabric library, or opening the library's folder. The library's fabrics have their files in another one.
const int fabric_command_role = Qt::UserRole + 1;
const int fabric_file_role = Qt::UserRole + 2;
const int new_fabric = 1;
const int edit_fabric = 2;
const int add_to_library = 3;
const int open_library = 4;

// A fabric shrinks to no less than this share of its drafted size, and stretches out to no more than this one.
const qreal least_shrinkage = 0.5;
const qreal most_shrinkage = 1.5;

// Edge length of the triangles in cm for a final drape; CLO recommends 20 mm while editing and 5 to 10 mm for the
// final drape.
const qreal fine_edge_length = 1.0;

// Folds hold their angle this many times as stiffly as their fabric bends, as a pressed fold does.
const qreal fold_strength = 10.0;

// The angles the Fold menu offers to fold at, in degrees on the cloth's right side: wrong sides together, as hems,
// facings and collars fold, at right angles either way, and right sides together.
const qreal fold_angles[] = {360.0, 270.0, 90.0, 0.0};

// The seam types the Sew menu offers, by the angle they hold the pieces at, as VSeam says: none, flat, turned and
// right sides together.
const qreal seam_angles[] = {-1.0, 180.0, 360.0, 0.0};

// The elastic's lengths the Elastic menu offers, as shares of the line's length.
const qreal elastic_ratios[] = {0.9, 0.8, 0.7, 0.6};

// The menus offer the layers from 0, next to the body, to this one; a garment of a shell, its lining and pockets, worn
// over another, rarely needs more.
const int highest_layer = 5;

// Folds within this many degrees of the cloth onto itself start out folded; the part folded over lies this many cm off
// the rest, on the side it folds to.
const qreal folded_over = 45.0;
const qreal fold_gap = 0.5;

// Vertices closer to a fold's line than this, in cm, lie on it.
const qreal on_fold_line = 1e-6;

//---------------------------------------------------------------------------------------------------------------------
// The point mirrored across the line through a and b.
QPointF reflected(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF along = b - a;
    const QPointF offset = point - a;
    const QPointF onto = along * (QPointF::dotProduct(offset, along) / QPointF::dotProduct(along, along));
    return a + onto * 2.0 - offset;
}

// An avatar's fingerprint is this many hex digits of a SHA-1 of what it is fitted to.
const int fingerprint_length = 16;

// The pattern keeps its cloth's vertices as 32-bit floats: one that close to a vertex of a mesh is that vertex, in cm.
const qreal same_vertex_tolerance = 1e-3;

// The avatar's grey, as garment_scene.qml draws it.
const char* const avatar_color = "#b9b4ad";

// Thread colors the Topstitch menu offers besides thread matching the cloth: white, black, the gold of jeans' stitching
// and red.
const char* const thread_colors[] = {"#f2f0eb", "#202020", "#c8962d", "#b3261e"};

// A palette whose windows are darker than this lightness is dark; icon pixels darker than this in every channel are
// outline.
const int dark_lightness = 128;
const int black_level = 60;

//---------------------------------------------------------------------------------------------------------------------
// Whether points are the vertices of a mesh, the same number of them, each near enough where the mesh has it.
bool sameVertices(const QVector<QPointF>& points, const GarmentMesh& mesh)
{
    bool same = points.size() == mesh.vertexCount();
    for (int i = 0; same && i < points.size(); ++i)
    {
        same = QLineF(points.at(i), mesh.rest_positions.at(i)).length() < same_vertex_tolerance;
    }
    return same;
}

//---------------------------------------------------------------------------------------------------------------------
// The image file as an image of a fabric to keep in the pattern: a PNG or JPG file as it is, any other image, and any
// larger than kept_image_limit pixels across or along, scaled down to that and saved as PNG, or JPG for a JPG. Its
// width is left to be said. Null for a file that isn't an image.
VFabricTexture readFabricImage(const QString& path)
{
    VFabricTexture texture;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return texture;
    }
    QByteArray bytes = file.readAll();
    QImage image = QImage::fromData(bytes);
    if (image.isNull())
    {
        return texture;
    }

    QBuffer read(&bytes);
    read.open(QIODevice::ReadOnly);
    const QByteArray format = QImageReader::imageFormat(&read);
    const bool jpg = format == "jpeg" || format == "jpg";
    const bool fits = image.width() <= kept_image_limit && image.height() <= kept_image_limit;
    texture.extension = jpg ? QStringLiteral("JPG") : QStringLiteral("PNG");
    if (fits && (jpg || format == "png"))
    {
        texture.image = bytes;
    }
    else
    {
        if (!fits)
        {
            image = image.scaled(kept_image_limit, kept_image_limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QBuffer written;
        written.open(QIODevice::WriteOnly);
        image.save(&written, jpg ? "JPG" : "PNG");
        texture.image = written.data();
    }
    return texture;
}

//---------------------------------------------------------------------------------------------------------------------
// The fabric of that name: one of the pattern's own, or else one of the 3D View's.
Fabric namedFabric(const VGarmentFabrics& fabrics, const QString& name)
{
    const VCustomFabric custom = fabrics.customFabric(name);
    if (custom.name.isEmpty())
    {
        return Fabric::preset(name);
    }
    Fabric fabric;
    fabric.name = custom.name;
    fabric.weight = custom.weight;
    fabric.warp_stiffness = custom.warp;
    fabric.weft_stiffness = custom.weft;
    fabric.bias_stiffness = custom.bias;
    fabric.bending_warp = custom.bending_warp;
    fabric.bending_weft = custom.bending_weft;
    fabric.thickness = custom.thickness;
    return fabric;
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric's values, as a fabric of the pattern's own of that name.
VCustomFabric customFabric(const Fabric& fabric, const QString& name)
{
    VCustomFabric custom;
    custom.name = name;
    custom.weight = fabric.weight;
    custom.warp = fabric.warp_stiffness;
    custom.weft = fabric.weft_stiffness;
    custom.bias = fabric.bias_stiffness;
    custom.bending_warp = fabric.bending_warp;
    custom.bending_weft = fabric.bending_weft;
    custom.thickness = fabric.thickness;
    return custom;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
bool GarmentViewWidget::AvatarRequest::operator==(const AvatarRequest& other) const
{
    return qFuzzyCompare(1.0 + wanted.height, 1.0 + other.wanted.height)
           && qFuzzyCompare(1.0 + wanted.bust, 1.0 + other.wanted.bust)
           && qFuzzyCompare(1.0 + wanted.waist, 1.0 + other.wanted.waist)
           && qFuzzyCompare(1.0 + wanted.hip, 1.0 + other.wanted.hip)
           && qFuzzyCompare(1.0 + wanted.neck, 1.0 + other.wanted.neck)
           && qFuzzyCompare(1.0 + wanted.upper_arm, 1.0 + other.wanted.upper_arm)
           && qFuzzyCompare(1.0 + wanted.lower_arm, 1.0 + other.wanted.lower_arm)
           && qFuzzyCompare(1.0 + wanted.arm, 1.0 + other.wanted.arm)
           && qFuzzyCompare(1.0 + wanted.crotch, 1.0 + other.wanted.crotch)
           && qFuzzyCompare(1.0 + wanted.knee_height, 1.0 + other.wanted.knee_height)
           && qFuzzyCompare(1.0 + wanted.knee, 1.0 + other.wanted.knee)
           && qFuzzyCompare(1.0 + wanted.calf, 1.0 + other.wanted.calf)
           && qFuzzyCompare(1.0 + gender, 1.0 + other.gender)
           && qFuzzyCompare(1.0 + age, 1.0 + other.age);
}

//---------------------------------------------------------------------------------------------------------------------
bool GarmentViewWidget::AvatarRequest::hasMeasurements() const
{
    return wanted.height > 0 || wanted.bust > 0 || wanted.waist > 0 || wanted.hip > 0 || wanted.neck > 0
           || wanted.upper_arm > 0 || wanted.lower_arm > 0 || wanted.arm > 0 || wanted.crotch > 0
           || wanted.knee_height > 0 || wanted.knee > 0 || wanted.calf > 0;
}

//---------------------------------------------------------------------------------------------------------------------
// A short fingerprint of what the avatar is fitted to, so a drape the pattern keeps is known to be draped on an avatar
// like this one. The age is left out: worked out from a birth date, it changes from day to day.
QString GarmentViewWidget::AvatarRequest::fingerprint() const
{
    const QVector<qreal> values = {wanted.height, wanted.bust, wanted.waist, wanted.hip, wanted.neck, wanted.upper_arm,
                                   wanted.lower_arm, wanted.arm, wanted.crotch, wanted.knee_height, wanted.knee,
                                   wanted.calf, gender};
    QStringList texts;
    for (const qreal value : values)
    {
        texts.append(QString::number(value, 'f', 2));
    }
    const QByteArray hash = QCryptographicHash::hash(texts.join(QLatin1Char(' ')).toLatin1(), QCryptographicHash::Sha1);
    return QString::fromLatin1(hash.toHex().left(fingerprint_length));
}

//---------------------------------------------------------------------------------------------------------------------
GarmentViewWidget::GarmentViewWidget(VContainer* data, VAbstractPattern* doc, QWidget* parent)
    : QWidget(parent)
    , m_data(data)
    , m_doc(doc)
    , m_scene_model(new GarmentSceneModel(this))
    , m_seam_editor(new SeamEditor(this))
    , m_stitch_editor(new StitchEditor(this))
    , m_fold_editor(new FoldEditor(this))
    , m_elastic_editor(new ElasticEditor(this))
    , m_sew_action(nullptr)
    , m_flip_action(nullptr)
    , m_seam_types(nullptr)
    , m_other_seam_angle_action(nullptr)
    , m_remove_action(nullptr)
    , m_topstitch_action(nullptr)
    , m_every_edge_action(nullptr)
    , m_stitch_styles(nullptr)
    , m_threads(nullptr)
    , m_other_thread_action(nullptr)
    , m_fold_action(nullptr)
    , m_fold_angles(nullptr)
    , m_other_angle_action(nullptr)
    , m_elastic_action(nullptr)
    , m_elastic_ratios(nullptr)
    , m_other_ratio_action(nullptr)
    , m_cancel_action(nullptr)
    , m_avatar_action(nullptr)
    , m_arrange_action(nullptr)
    , m_rotate_clockwise_action(nullptr)
    , m_rotate_counterclockwise_action(nullptr)
    , m_turn_over_action(nullptr)
    , m_take_off_action(nullptr)
    , m_superimpose_over_action(nullptr)
    , m_superimpose_under_action(nullptr)
    , m_superimpose_side_action(nullptr)
    , m_layer_menu(nullptr)
    , m_piece_layers(nullptr)
    , m_piece_menu(nullptr)
    , m_simulate_action(nullptr)
    , m_reset_action(nullptr)
    , m_fine_action(nullptr)
    , m_device_action(nullptr)
    , m_remove_pins_action(nullptr)
    , m_fit_action(nullptr)
    , m_fit_maps(nullptr)
    , m_checks_action(nullptr)
    , m_export_action(nullptr)
    , m_view_action(nullptr)
    , m_hide_piece_action(nullptr)
    , m_show_pieces_action(nullptr)
    , m_snapshot_action(nullptr)
    , m_fabric_box(nullptr)
    , m_custom_fabrics_listed()
    , m_fabric_library(new FabricLibrary(this))
    , m_library_fabrics_listed()
    , m_image_action(nullptr)
    , m_image_width_action(nullptr)
    , m_remove_image_action(nullptr)
    , m_shrinkage_action(nullptr)
    , m_quick_view(nullptr)
    , m_view_container(nullptr)
    , m_message_label(new QLabel(this))
    , m_rebuild_timer(new QTimer(this))
    , m_mesher()
    , m_mesh_cache()
    , m_rebuild_pending(false)
    , m_wearer_gender(0.5)
    , m_wearer_age(default_age)
    , m_body_model()
    , m_fit_watcher(new QFutureWatcher<AvatarFit>(this))
    , m_avatar_request()
    , m_has_avatar_request(false)
    , m_avatar_generation(0)
    , m_wrap()
    , m_collider()
    , m_arrangements()
    , m_draped()
    , m_turned_pairs()
    , m_drape_pieces()
    , m_runner(new DrapeRunner(this))
    , m_drag()
    , m_pins()
    , m_resting(false)
    , m_pull()
    , m_avatar_fingerprint()
    , m_drape_taken_in(false)
    , m_cloths_taken_in()
    , m_pattern_drape()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    createToolBar();

    m_message_label->setWordWrap(true);
    m_message_label->setAlignment(Qt::AlignCenter);
    m_message_label->hide();
    layout->addWidget(m_message_label);

    m_seam_editor->setHighlightColor(palette().color(QPalette::Highlight));

    m_rebuild_timer->setSingleShot(true);
    m_rebuild_timer->setInterval(rebuild_delay_ms);
    connect(m_rebuild_timer, &QTimer::timeout, this, &GarmentViewWidget::rebuildScene);
    connect(m_scene_model, &GarmentSceneModel::piecePicked, this, &GarmentViewWidget::scenePicked);
    connect(m_fit_watcher, &QFutureWatcher<AvatarFit>::finished, this, &GarmentViewWidget::avatarFitted);
    connect(m_doc, &VAbstractPattern::seamsChanged, this, &GarmentViewWidget::updateSeams);
    connect(m_seam_editor, &SeamEditor::seamSewn, this, &GarmentViewWidget::sewSeam);
    connect(m_seam_editor, &SeamEditor::sewingChanged, this, &GarmentViewWidget::updateActions);
    connect(m_seam_editor, &SeamEditor::selectedSeamChanged, this, &GarmentViewWidget::updateActions);
    connect(m_doc, &VAbstractPattern::arrangementsChanged, this, &GarmentViewWidget::updateArrangements);
    connect(m_doc, &VAbstractPattern::layersChanged, this, &GarmentViewWidget::updateLayers);
    connect(m_doc, &VAbstractPattern::fabricsChanged, this, &GarmentViewWidget::updateFabrics);
    connect(m_fabric_library, &FabricLibrary::changed, this, &GarmentViewWidget::updateActions);
    connect(m_doc, &VAbstractPattern::topstitchesChanged, this, &GarmentViewWidget::updateTopstitches);
    connect(m_doc, &VAbstractPattern::avatarChanged, this, &GarmentViewWidget::updateChosenAvatar);
    connect(m_stitch_editor, &StitchEditor::topstitchesEdited, this, &GarmentViewWidget::saveTopstitches);
    connect(m_stitch_editor, &StitchEditor::stitchingChanged, this, &GarmentViewWidget::updateActions);
    connect(m_stitch_editor, &StitchEditor::stitchesChanged, this, &GarmentViewWidget::showStitches);
    connect(m_stitch_editor, &StitchEditor::previewChanged, this, &GarmentViewWidget::showStitchPreview);
    connect(m_fold_editor, &FoldEditor::foldsEdited, this, &GarmentViewWidget::saveFolds);
    connect(m_fold_editor, &FoldEditor::foldingChanged, this, &GarmentViewWidget::updateActions);
    connect(m_fold_editor, &FoldEditor::linesChanged, this, &GarmentViewWidget::showLines);
    connect(m_doc, &VAbstractPattern::foldsChanged, this, &GarmentViewWidget::updateFolds);
    connect(m_elastic_editor, &ElasticEditor::elasticsEdited, this, &GarmentViewWidget::saveElastics);
    connect(m_elastic_editor, &ElasticEditor::editingChanged, this, &GarmentViewWidget::updateActions);
    connect(m_elastic_editor, &ElasticEditor::linesChanged, this, &GarmentViewWidget::showLines);
    connect(m_doc, &VAbstractPattern::elasticsChanged, this, &GarmentViewWidget::updateElastics);
    connect(m_scene_model, &GarmentSceneModel::placeRequested, this, &GarmentViewWidget::placePiece);
    connect(m_scene_model, &GarmentSceneModel::placePointRequested, this, &GarmentViewWidget::placePieceAtPoint);
    connect(m_scene_model, &GarmentSceneModel::previewRequested, this, &GarmentViewWidget::previewArrangement);
    connect(m_scene_model, &GarmentSceneModel::previewLeft, this, &GarmentViewWidget::clearArrangementPreview);
    connect(m_scene_model, &GarmentSceneModel::selectedPieceChanged, m_scene_model, &GarmentSceneModel::clearPreview);
    connect(m_scene_model, &GarmentSceneModel::pieceMenuRequested, this, &GarmentViewWidget::showPieceMenu);
    connect(m_scene_model, &GarmentSceneModel::gizmoGrabRequested, this, &GarmentViewWidget::grabGizmo);
    connect(m_scene_model, &GarmentSceneModel::gizmoDragRequested, this, &GarmentViewWidget::dragGizmo);
    connect(m_scene_model, &GarmentSceneModel::gizmoDropRequested, this, &GarmentViewWidget::dropGizmo);
    connect(m_scene_model, &GarmentSceneModel::gizmoHovered, this, &GarmentViewWidget::hoverGizmo);
    connect(m_scene_model, &GarmentSceneModel::selectedPieceChanged, this, &GarmentViewWidget::updateGizmo);
    connect(m_scene_model, &GarmentSceneModel::grabRequested, this, &GarmentViewWidget::grabPiece);
    connect(m_scene_model, &GarmentSceneModel::dragRequested, this, &GarmentViewWidget::dragPiece);
    connect(m_scene_model, &GarmentSceneModel::dropRequested, this, &GarmentViewWidget::dropPiece);
    connect(m_scene_model, &GarmentSceneModel::pullRequested, this, &GarmentViewWidget::pullCloth);
    connect(m_scene_model, &GarmentSceneModel::pinPullRequested, this, &GarmentViewWidget::pullPin);
    connect(m_scene_model, &GarmentSceneModel::pullMoved, this, &GarmentViewWidget::movePull);
    connect(m_scene_model, &GarmentSceneModel::pullReleased, this, &GarmentViewWidget::releasePull);
    connect(m_scene_model, &GarmentSceneModel::pinRequested, this, &GarmentViewWidget::pinCloth);
    connect(m_scene_model, &GarmentSceneModel::unpinRequested, this, &GarmentViewWidget::unpin);
    connect(m_scene_model, &GarmentSceneModel::selectedPieceChanged, this, &GarmentViewWidget::updateActions);
    connect(m_scene_model, &GarmentSceneModel::avatarChanged, this, &GarmentViewWidget::updateActions);
    connect(m_runner, &DrapeRunner::frameReady, this, &GarmentViewWidget::drapeFrame);
    connect(m_runner, &DrapeRunner::settled, this, &GarmentViewWidget::drapeSettled);
    connect(m_runner, &DrapeRunner::woke, this, &GarmentViewWidget::drapeWoke);
    connect(m_runner, &DrapeRunner::computing, this, &GarmentViewWidget::showComputing);
}

//---------------------------------------------------------------------------------------------------------------------
// The drape's thread has to be done before the solver's view of the scene goes away.
GarmentViewWidget::~GarmentViewWidget()
{
    m_runner->stop();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Who the avatar is: gender from 0 (female) to 1 (male), 0.5 when unknown, and age in years, 0 when unknown.
/// Takes effect with the next updatePieces(), which follows anyway when measurements are (re)loaded.
void GarmentViewWidget::setWearer(qreal gender, qreal age_years)
{
    m_wearer_gender = qBound(0.0, gender, 1.0);
    m_wearer_age = (age_years > 0 && age_years < oldest_age) ? age_years : default_age;
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
    m_drape_taken_in = false;  // nothing more goes into the pattern being closed
    m_cloths_taken_in.clear();
    m_pattern_drape = VGarmentDrape();
    m_avatar_fingerprint.clear();
    m_simulate_action->setChecked(false);
    m_arrange_action->setChecked(false);
    m_runner->stop();
    m_draped.clear();
    m_pins.clear();
    m_arrangements.clear();
    m_wrap.reset();
    m_collider = BodyCollider();
    m_scene_model->setBody(m_collider);
    m_rebuild_timer->stop();
    m_rebuild_pending = false;
    m_mesh_cache.clear();
    m_has_avatar_request = false;
    ++m_avatar_generation;  // a fit still running belongs to what was cleared
    m_scene_model->clear();
    m_seam_editor->setSewing(false);
    m_seam_editor->setSeams(QVector<VSeam>());
    m_seam_editor->setPieces(QVector<ShownPiece>());
    m_stitch_editor->setStitching(false);
    m_stitch_editor->setPieces(QVector<ShownPiece>(), VTopstitches());
    m_fold_editor->setFolding(false);
    m_fold_editor->setPieces(QVector<ShownPiece>(), QVector<VFold>());
    m_elastic_editor->setEditing(false);
    m_elastic_editor->setPieces(QVector<ShownPiece>(), QVector<VElastic>());
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_quick_view == nullptr)
    {
        createScene();
    }
    if (m_rebuild_pending)
    {
        m_rebuild_timer->start();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A new theme gives the icons new colors.
void GarmentViewWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateIcons();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Nobody watches a hidden drape, so it stops; unless the dock was only floated or docked, which hides it for a moment.
void GarmentViewWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    QTimer::singleShot(0, this, [this]()
    {
        if (!isVisible())
        {
            m_simulate_action->setChecked(false);
        }
    });
}

//---------------------------------------------------------------------------------------------------------------------
// While the view has the focus, the keys go to the scene's window rather than to this widget, so Delete and Esc are
// taken from there, ahead of the main window's shortcuts.
bool GarmentViewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_quick_view && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress))
    {
        const QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        QAction* action = nullptr;
        if (key_event->matches(QKeySequence::Delete))
        {
            action = m_remove_action;
        }
        else if (key_event->key() == Qt::Key_Escape && key_event->modifiers() == Qt::NoModifier)
        {
            action = m_cancel_action;
        }

        if (action != nullptr && action->isEnabled())
        {
            if (event->type() == QEvent::KeyPress)
            {
                action->trigger();
            }
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Meshes the pieces included in the layout and hands them to the scene. Pieces whose seam line didn't change
/// keep their mesh; pieces arranged on the avatar are put there, or where the drape took them. A drape running
/// starts over with the changed pattern.
void GarmentViewWidget::rebuildScene()
{
    m_rebuild_pending = false;
    const bool simulating = m_runner->isRunning();
    m_runner->stop();
    readArrangements();

    const QHash<quint32, VPiece>* pieces = m_data->DataPieces();
    QList<quint32> ids = pieces->keys();
    std::sort(ids.begin(), ids.end());

    // All meshes first: where a piece goes can depend on the pieces it is sewn to. A piece edited after it was draped
    // starts out where it hung, as long as it is cut the same way, and the garment drapes again from there.
    QHash<quint32, CachedMesh> mesh_cache;
    bool carried = false;
    for (const quint32 id : ids)
    {
        const VPiece& piece = pieces->constFind(id).value();
        if (piece.isInLayout())
        {
            try
            {
                const PieceOutline outline = PieceOutline::fromPiece(piece, m_data);
                const PieceSymmetry wanted = symmetryOf(piece);
                CachedMesh cached = m_mesh_cache.value(id);
                if (cached.outline != outline || cached.wanted != wanted || cached.mesh.isEmpty()
                    || !qFuzzyCompare(cached.edge_length, m_mesher.edgeLength()))
                {
                    const CachedMesh before = cached;
                    cached = garmentMeshes(id, outline, wanted);
                    const bool cut_the_same = before.symmetry == cached.symmetry;
                    carried = carryDrape(id, cut_the_same ? before.garment_mesh : GarmentMesh(), cached.garment_mesh)
                              || carried;
                    carried = carryDrape(PieceOutline::mirrorId(id), cut_the_same ? before.mirror_mesh : GarmentMesh(),
                                         cached.mirror_mesh) || carried;
                }
                mesh_cache.insert(id, cached);
            }
            catch (const VException&)
            {
                // The piece scene already shows what is wrong with a piece whose points can't be found.
            }
        }
    }

    m_mesh_cache = mesh_cache;
    for (auto draped = m_draped.begin(); draped != m_draped.end();)
    {
        draped = m_mesh_cache.contains(patternPiece(draped.key())) ? std::next(draped) : m_draped.erase(draped);
    }
    carried = dressTakenIn() || carried;
    m_turned_pairs.clear();  // turnedPairs() places body pieces, and those are never turned
    m_turned_pairs = turnedPairs(false);
    m_turned_pairs = turnedPairs(true);  // then pieces sewn only to pieces on arms or legs follow those

    QVector<GarmentSceneModel::Piece> scene_pieces;
    QVector<ShownPiece> shown_pieces;
    m_garment_pieces.clear();
    const VGarmentFabrics fabrics = m_doc->getFabrics();
    for (const quint32 id : ids)
    {
        const CachedMesh cached = m_mesh_cache.value(id);
        if (!cached.mesh.isEmpty())
        {
            const VPiece& piece = pieces->constFind(id).value();
            const QColor color(piece.getColor());

            GarmentSceneModel::Piece scene_piece;
            scene_piece.id = id;
            scene_piece.name = piece.GetName();
            scene_piece.color = color.isValid() ? color : QColor(Qt::white);
            const qreal grain_angle = grainAngle(piece);
            scene_piece.grain_angle = grain_angle;
            scene_piece.thickness = namedFabric(fabrics, fabrics.of(id)).thickness / millimetres_per_cm;
            const VFabricTexture texture = fabrics.textureOf(id);
            scene_piece.texture = texture.image;
            scene_piece.texture_width = texture.width;

            // What the seams and the topstitching are drawn on: the piece as drafted, and the meshes shown of it.
            ShownPiece shown_piece;
            shown_piece.id = id;
            shown_piece.outline = cached.outline;
            shown_piece.symmetry = cached.symmetry;
            if (cached.symmetry == PieceSymmetry::Fold)
            {
                shown_piece.fold_start = cached.fold_start;
                shown_piece.fold_end = cached.fold_end;
            }
            shown_piece.paths = stitchedPaths(piece);

            const bool placed = !m_wrap.isNull() && m_arrangements.contains(id) && !cached.garment_mesh.isEmpty();
            if (placed)
            {
                const bool unfolded = cached.symmetry == PieceSymmetry::Fold;
                shown_piece.shown.append({id, cached.garment_mesh,
                                          unfolded ? PieceLayout::Unfolded : PieceLayout::Drafted, true});
                scene_piece.mesh = cached.garment_mesh;
                scene_piece.positions = piecePositions(id, cached.garment_mesh);
                scene_pieces.append(scene_piece);
                m_garment_pieces.append({id, cached.garment_mesh, grain_angle});

                if (cached.symmetry == PieceSymmetry::Pair)
                {
                    GarmentSceneModel::Piece mirror_piece = scene_piece;
                    mirror_piece.id = PieceOutline::mirrorId(id);
                    mirror_piece.mesh = cached.mirror_mesh;
                    mirror_piece.positions = piecePositions(mirror_piece.id, cached.mirror_mesh);
                    mirror_piece.grain_angle = 180.0 - grain_angle;
                    scene_pieces.append(mirror_piece);
                    shown_piece.shown.append({mirror_piece.id, cached.mirror_mesh, PieceLayout::Mirrored, true});
                    m_garment_pieces.append({mirror_piece.id, cached.mirror_mesh, 180.0 - grain_angle});
                }
            }
            else
            {
                // Pieces lie on the board as drafted.
                scene_piece.mesh = cached.mesh;
                scene_pieces.append(scene_piece);
                shown_piece.shown.append({id, cached.mesh, PieceLayout::Drafted, false});
            }
            shown_pieces.append(shown_piece);
        }
    }

    // The topstitching goes onto the new meshes with them.
    const VTopstitches topstitches = m_doc->getTopstitches();
    m_stitch_editor->setPieces(shown_pieces, topstitches);
    m_scene_model->setThreadColor(QColor(topstitches.color));
    const QHash<quint32, QVector<ThreadStitch>> stitches = m_stitch_editor->stitches();
    const QHash<quint32, QVector<ThreadStitch>> preview = m_stitch_editor->preview();
    m_fold_editor->setPieces(shown_pieces, m_doc->getFolds());
    m_elastic_editor->setPieces(shown_pieces, m_doc->getElastics());
    const QHash<quint32, QVector<DrawnLine>> fold_lines = m_fold_editor->lines();
    const QHash<quint32, QVector<DrawnLine>> elastic_lines = m_elastic_editor->lines();
    for (GarmentSceneModel::Piece& scene_piece : scene_pieces)
    {
        scene_piece.stitches = stitches.value(scene_piece.id);
        scene_piece.preview = preview.value(scene_piece.id);
        scene_piece.lines = fold_lines.value(scene_piece.id) + elastic_lines.value(scene_piece.id);
    }

    m_scene_model->setPieces(scene_pieces);
    m_scene_model->setArrangementPoints(m_wrap.isNull() ? QVector<ArrangementPoint>() : m_wrap->points());
    m_scene_model->clearPreview();
    m_seam_editor->setSeams(m_doc->getSeams());
    m_seam_editor->setUnit(qApp->patternUnit());
    m_seam_editor->setPieces(shown_pieces);

    // Pins only hold pieces on the avatar.
    m_pins.erase(std::remove_if(m_pins.begin(), m_pins.end(), [this](const ClothPin& pin)
    {
        return !m_scene_model->isPlaced(pin.piece);
    }), m_pins.end());
    updateHolds();
    showSeamsOnAvatar();

    updateAvatar();
    updateGizmo();

    if (simulating || carried)
    {
        const QSignalBlocker blocker(m_simulate_action);
        m_simulate_action->setChecked(true);
        startSimulation();
    }
    updateActions();
    keepDrape();
}

//---------------------------------------------------------------------------------------------------------------------
// The piece's mesh changed: where it was draped goes over to the new mesh, if the old one is known. Says whether there
// was a drape to carry over.
bool GarmentViewWidget::carryDrape(quint32 id, const GarmentMesh& before, const GarmentMesh& after)
{
    const QVector<QVector3D> carried = before.carry(m_draped.take(id), after);
    if (!carried.isEmpty())
    {
        m_draped.insert(id, carried);
    }
    return !carried.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
// Takes in the drape the pattern was saved with, if it was draped on an avatar fitted to what this one is: the pieces
// are meshed as finely as they were then, their cloth goes onto the meshes when the scene is built next, and the pins
// hold it again.
void GarmentViewWidget::takeInDrape()
{
    m_cloths_taken_in.clear();
    const VGarmentDrape drape = m_doc->getDrape();
    if (drape.isNull() || drape.avatar != m_avatar_fingerprint)
    {
        return;
    }

    if (drape.edge_length > 0 && !qFuzzyCompare(drape.edge_length, m_mesher.edgeLength()))
    {
        m_mesher.setEdgeLength(drape.edge_length);
        const QSignalBlocker blocker(m_fine_action);
        m_fine_action->setChecked(m_mesher.edgeLength() < PieceMesher::defaultEdgeLength());
    }
    m_cloths_taken_in = drape.cloths;
    for (const VClothPin& pin : drape.pins)
    {
        m_pins.append({pin.copy ? PieceOutline::mirrorId(pin.piece_id) : pin.piece_id, QPointF(pin.x, pin.y),
                       QVector3D(static_cast<float>(pin.at_x), static_cast<float>(pin.at_y),
                                 static_cast<float>(pin.at_z))});
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Puts the cloth taken in from the pattern onto the pieces' meshes, those of pieces still on the avatar. Onto a piece
// meshed differently since, edited or meshed finer or coarser, the cloth carries over; says whether any did.
bool GarmentViewWidget::dressTakenIn()
{
    bool carried = false;
    for (const VDrapedCloth& cloth : std::as_const(m_cloths_taken_in))
    {
        const CachedMesh cached = m_mesh_cache.value(cloth.piece_id);
        const GarmentMesh& mesh = cloth.copy ? cached.mirror_mesh : cached.garment_mesh;
        if (mesh.isEmpty() || !m_arrangements.contains(cloth.piece_id))
        {
            continue;
        }

        QVector<QPointF> rest;
        QVector<QVector3D> positions;
        const int count = qMin(cloth.rest.size() / 2, cloth.positions.size() / 3);
        for (int i = 0; i < count; ++i)
        {
            rest.append(QPointF(static_cast<qreal>(cloth.rest.at(2 * i)), static_cast<qreal>(cloth.rest.at(2 * i + 1))));
            positions.append(QVector3D(cloth.positions.at(3 * i), cloth.positions.at(3 * i + 1),
                                       cloth.positions.at(3 * i + 2)));
        }

        const quint32 id = cloth.copy ? PieceOutline::mirrorId(cloth.piece_id) : cloth.piece_id;
        if (sameVertices(rest, mesh))
        {
            m_draped.insert(id, positions);
        }
        else
        {
            const QVector<QVector3D> moved = PieceMesher::meshPoints(rest).carry(positions, mesh);
            if (!moved.isEmpty())
            {
                m_draped.insert(id, moved);
                carried = true;
            }
        }
    }
    m_cloths_taken_in.clear();
    return carried;
}

//---------------------------------------------------------------------------------------------------------------------
// The drape as it is now, for the pattern to keep: the cloth of each piece on the avatar that has draped, where its
// mesh's vertices lie in the flat and where they hang, and the pins.
VGarmentDrape GarmentViewWidget::currentDrape() const
{
    VGarmentDrape drape;
    for (const GarmentPiece& garment_piece : m_garment_pieces)
    {
        const QVector<QVector3D> positions = m_draped.value(garment_piece.id);
        if (!positions.isEmpty() && positions.size() == garment_piece.mesh.vertexCount())
        {
            VDrapedCloth cloth;
            cloth.piece_id = patternPiece(garment_piece.id);
            cloth.copy = PieceOutline::isMirrorId(garment_piece.id);
            cloth.rest.reserve(2 * positions.size());
            cloth.positions.reserve(3 * positions.size());
            for (int i = 0; i < positions.size(); ++i)
            {
                const QPointF& rest = garment_piece.mesh.rest_positions.at(i);
                const QVector3D& position = positions.at(i);
                cloth.rest << static_cast<float>(rest.x()) << static_cast<float>(rest.y());
                cloth.positions << position.x() << position.y() << position.z();
            }
            drape.cloths.append(cloth);
        }
    }
    for (const ClothPin& pin : m_pins)
    {
        drape.pins.append({patternPiece(pin.piece), PieceOutline::isMirrorId(pin.piece), pin.rest.x(), pin.rest.y(),
                           pin.position.x(), pin.position.y(), pin.position.z()});
    }

    if (!drape.isNull())
    {
        drape.avatar = m_avatar_fingerprint;
        drape.edge_length = m_mesher.edgeLength();
    }
    return drape;
}

//---------------------------------------------------------------------------------------------------------------------
// Keeps the drape in the pattern while the cloth is still, so the pattern opens with the garment hanging as it does
// now. Until the pattern's own drape was taken in for the avatar shown, that one is left as it is.
void GarmentViewWidget::keepDrape()
{
    if (m_drape_taken_in && !isDraping())
    {
        const VGarmentDrape drape = currentDrape();
        if (!(drape == m_pattern_drape))
        {
            m_pattern_drape = drape;
            m_doc->setDrape(drape);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Starts fitting the avatar if the measurements, the wearer or the avatar chosen for a pattern without measurements
// changed. A change during a fit is picked up when the fit finishes.
void GarmentViewWidget::updateAvatar()
{
    const AvatarRequest request = wantedAvatar();
    if (!m_has_avatar_request || !(request == m_avatar_request))
    {
        m_avatar_request = request;
        m_has_avatar_request = true;
        updateActions();

        if (!request.hasMeasurements())
        {
            m_scene_model->clearAvatar();
            if (!m_wrap.isNull())
            {
                // Without an avatar, arranged pieces go back on the board; the pattern keeps its drape for when
                // there is one again.
                m_simulate_action->setChecked(false);
                m_drape_taken_in = false;
                m_draped.clear();
                m_wrap.reset();
                m_collider = BodyCollider();
                m_scene_model->setBody(m_collider);
                rebuildScene();
            }
        }
        else if (!m_fit_watcher->isRunning())
        {
            if (m_body_model.isNull())
            {
                m_body_model.reset(new BodyModel());
            }

            const BodyModel model = *m_body_model;
            const quint64 generation = m_avatar_generation;
            m_fit_watcher->setFuture(QtConcurrent::run([model, request, generation]()
            {
                AvatarFit result;
                result.generation = generation;
                result.request = request;
                result.fit = BodyFitter(model).fit(request.wanted, request.gender, request.age);
                result.positions = model.evaluate(result.fit.shape);
                return result;
            }));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Shows a finished fit if it is still what is wanted; if the measurements changed meanwhile, fits again.
void GarmentViewWidget::avatarFitted()
{
    const AvatarFit result = m_fit_watcher->result();
    if (result.generation == m_avatar_generation)
    {
        if (!(result.request == m_avatar_request))
        {
            m_has_avatar_request = false;
            updateAvatar();
        }
        else if (m_body_model->isValid() && !result.positions.isEmpty())
        {
            m_scene_model->setAvatar(result.positions, m_body_model->triangles(), m_body_model->skinVertexCount(),
                                     avatarNote(result));

            // A new body takes the drape and the pins with it; arranged pieces are put on it afresh, or back where
            // the pattern was draped, on an avatar fitted to the same. That is what the pattern has then, until the
            // cloth moves.
            m_simulate_action->setChecked(false);
            m_drape_taken_in = false;
            m_draped.clear();
            m_pins.clear();
            m_wrap.reset(new BodyWrap(*m_body_model, result.positions));
            m_collider = BodyCollider(result.positions.mid(0, m_body_model->skinVertexCount()),
                                      m_body_model->triangles());
            m_scene_model->setBody(m_collider);
            m_avatar_fingerprint = result.request.fingerprint();
            takeInDrape();
            rebuildScene();
            m_pattern_drape = currentDrape();
            m_drape_taken_in = true;
        }
    }
    else if (m_has_avatar_request)
    {
        // Cleared while fitting, and something new was asked for since.
        m_has_avatar_request = false;
        updateAvatar();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// What the avatar is fitted to: the pattern's measurements, or without any, those of the avatar chosen for it.
GarmentViewWidget::AvatarRequest GarmentViewWidget::wantedAvatar() const
{
    AvatarRequest request = measuredAvatar();
    if (!request.hasMeasurements())
    {
        const VGarmentAvatar chosen = chosenAvatar();
        request.wanted.height = chosen.height;
        request.wanted.bust = chosen.bust;
        request.wanted.waist = chosen.waist;
        request.wanted.hip = chosen.hip;
        request.gender = chosen.male ? 1.0 : 0.0;
    }
    return request;
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's measurements in cm, the ones the avatar can be fitted to; measurements that aren't there stay 0.
GarmentViewWidget::AvatarRequest GarmentViewWidget::measuredAvatar() const
{
    // Looked up by internal name: DataMeasurements() is keyed by the names shown to the user, which can be translated.
    const QHash<QString, QSharedPointer<VInternalVariable>>* variables = m_data->DataVariables();
    const Unit unit = qApp->patternUnit();
    auto value_cm = [variables, unit](const QString& name)
    {
        const QSharedPointer<const VInternalVariable> variable = variables->value(name);
        return (variable.isNull() || variable->GetType() != VarType::Measurement)
               ? 0.0 : UnitConvertor(variable->GetValue(), unit, Unit::Cm);
    };

    AvatarRequest request;
    request.wanted.height = value_cm(height_M);
    request.wanted.bust = value_cm(bustCirc_M);
    request.wanted.waist = value_cm(waistCirc_M);
    request.wanted.hip = value_cm(hipCirc_M);
    request.wanted.neck = value_cm(neckMidCirc_M);
    request.wanted.upper_arm = value_cm(armShoulderTipToElbow_M);
    request.wanted.lower_arm = value_cm(armElbowToWrist_M);
    request.wanted.arm = value_cm(armShoulderTipToWrist_M);
    request.wanted.crotch = value_cm(legCrotchToFloor_M);
    request.wanted.knee_height = value_cm(heightKnee_M);
    request.wanted.knee = value_cm(legKneeCirc_M);
    request.wanted.calf = value_cm(legCalfCirc_M);
    request.gender = m_wearer_gender;
    request.age = BodyShape::ageFromYears(m_wearer_age);
    return request;
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar chosen for the pattern, for when it has no measurements; until one is chosen, a woman, or a man if the
// measurements' file says so, of the middle size.
VGarmentAvatar GarmentViewWidget::chosenAvatar() const
{
    VGarmentAvatar avatar = m_doc->getAvatar();
    if (avatar.isNull())
    {
        avatar.male = m_wearer_gender > 0.5;
        const StandardSize size = StandardSizes::of(avatar.male, StandardSizes::defaultSize(avatar.male));
        avatar.size = size.size;
        avatar.height = size.measurements.height;
        avatar.bust = size.measurements.bust;
        avatar.waist = size.measurements.waist;
        avatar.hip = size.measurements.hip;
    }
    return avatar;
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar chosen for the pattern changed, here or by undo and redo.
void GarmentViewWidget::updateChosenAvatar()
{
    if (isVisible())
    {
        updateAvatar();
    }
    else
    {
        m_rebuild_pending = true;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar of a pattern without measurements was asked for: a woman or a man of a size, as tall and round as wanted.
void GarmentViewWidget::chooseAvatar()
{
    AvatarDialog dialog(chosenAvatar(), qApp->patternUnit(), this);
    if (dialog.exec() == QDialog::Accepted)
    {
        const VGarmentAvatar before = m_doc->getAvatar();
        const VGarmentAvatar after = dialog.avatar();
        if (!(after == before))
        {
            qApp->getUndoStack()->push(new SaveAvatar(tr("change avatar"), before, after, m_doc));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Says which measurements the avatar couldn't reach, in the pattern's unit, or nothing if it reached them all.
QString GarmentViewWidget::avatarNote(const AvatarFit& result) const
{
    const Unit unit = qApp->patternUnit();
    const QString unit_name = UnitsToStr(unit, true);
    auto in_unit = [unit](qreal cm)
    {
        return QString::number(UnitConvertor(cm, Unit::Cm, unit), 'f', unit == Unit::Mm ? 0 : 1);
    };

    const BodyMeasurements& wanted = result.request.wanted;
    const BodyMeasurements& got = result.fit.measured;
    const QVector<std::tuple<QString, qreal, qreal>> checks = {
        std::make_tuple(tr("height"), wanted.height, got.height),
        std::make_tuple(tr("bust"), wanted.bust, got.bust),
        std::make_tuple(tr("waist"), wanted.waist, got.waist),
        std::make_tuple(tr("hip"), wanted.hip, got.hip),
        std::make_tuple(tr("neck"), wanted.neck, got.neck),
        std::make_tuple(tr("shoulder tip to elbow"), wanted.upper_arm, got.upper_arm),
        std::make_tuple(tr("elbow to wrist"), wanted.lower_arm, got.lower_arm),
        std::make_tuple(tr("shoulder tip to wrist"), wanted.arm, got.arm),
        std::make_tuple(tr("crotch height"), wanted.crotch, got.crotch),
        std::make_tuple(tr("knee height"), wanted.knee_height, got.knee_height),
        std::make_tuple(tr("knee"), wanted.knee, got.knee),
        std::make_tuple(tr("calf"), wanted.calf, got.calf)};

    QStringList misses;
    for (const auto& check : checks)
    {
        if (std::get<1>(check) > 0 && qAbs(std::get<2>(check) - std::get<1>(check)) > note_tolerance)
        {
            misses.append(tr("%1 %2 %3 instead of %4").arg(std::get<0>(check), in_unit(std::get<2>(check)),
                                                           unit_name, in_unit(std::get<1>(check))));
        }
    }
    return misses.isEmpty() ? QString()
                            : tr("The avatar comes closest with %1.").arg(misses.join(QStringLiteral(", ")));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::scenePicked(quint32 id)
{
    if (id != 0)
    {
        emit pieceSelected(patternPiece(id));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's seams changed, by sewing here or by undo and redo. A drape going on goes on with them: sewn, flipped,
// taken out or holding the pieces at another angle.
void GarmentViewWidget::updateSeams()
{
    m_seam_editor->setSeams(m_doc->getSeams());
    if (m_runner->isRunning())
    {
        startSimulation();
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's arrangements changed, by placing pieces here or by undo and redo.
void GarmentViewWidget::updateArrangements()
{
    rebuildScene();
}

//---------------------------------------------------------------------------------------------------------------------
// Takes in the pattern's arrangements; a piece put at an arrangement point goes where the point is on this avatar. A
// piece moved to another spot, or turned another way, starts its drape over.
void GarmentViewWidget::readArrangements()
{
    QHash<quint32, PieceArrangement> arrangements;
    for (const VPieceArrangement& stored : m_doc->getArrangements())
    {
        PieceArrangement arrangement;
        arrangement.part = BodyWrap::partFromName(stored.part);
        arrangement.angle = stored.angle;
        arrangement.height = stored.height;
        arrangement.rotation = stored.rotation;
        arrangement.turned_over = stored.turned_over;
        arrangement.point = stored.point;
        arrangement.distance = stored.distance;
        arrangement.lean = stored.lean;
        arrangement.swing = stored.swing;
        arrangements.insert(stored.piece_id, m_wrap.isNull() ? arrangement : m_wrap->resolved(arrangement));
    }

    for (auto draped = m_draped.begin(); draped != m_draped.end();)
    {
        const quint32 piece = patternPiece(draped.key());
        const bool same = arrangements.contains(piece)
                          && sameArrangement(m_arrangements.value(piece), arrangements.value(piece));
        draped = same ? std::next(draped) : m_draped.erase(draped);
    }
    m_arrangements = arrangements;
}

//---------------------------------------------------------------------------------------------------------------------
// Where a piece of the garment is shown: where the drape took it, else where it is arranged on the avatar, a
// mirrored copy mirrored to the other side of the body, else nowhere in particular, which puts it on the board. Of a
// pair turned to its seams, the mirrored copy goes where the piece is arranged and the piece to the other side.
QVector<QVector3D> GarmentViewWidget::piecePositions(quint32 id, const GarmentMesh& mesh) const
{
    QVector<QVector3D> positions;
    const quint32 piece = patternPiece(id);
    if (!m_wrap.isNull() && m_arrangements.contains(piece))
    {
        positions = m_draped.value(id);
        if (positions.size() != mesh.vertexCount())
        {
            positions = placedAt(id, mesh, m_arrangements.value(piece), m_turned_pairs.contains(piece));
        }
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// Where a piece of the garment starts out arranged so: the piece where the arrangement puts it, a mirrored copy
// mirrored to the other side of the body. Of a pair turned to its seams, the mirrored copy goes where the arrangement
// puts the piece, and the piece to the other side. Further out by `out` cm for a preview.
QVector<QVector3D> GarmentViewWidget::placedAt(quint32 id, const GarmentMesh& mesh,
                                               const PieceArrangement& arrangement, bool turned, qreal out) const
{
    QVector<QVector3D> positions;
    if (PieceOutline::isMirrorId(id) != turned)
    {
        const CachedMesh& cached = m_mesh_cache.value(patternPiece(id));
        const GarmentMesh& other = PieceOutline::isMirrorId(id) ? cached.garment_mesh : cached.mirror_mesh;
        positions = wrapped(other, patternPiece(id), arrangement, out);
        for (QVector3D& position : positions)
        {
            position = m_wrap->mirrored(position);
        }
    }
    else
    {
        positions = wrapped(mesh, patternPiece(id), arrangement, out);
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// A mesh of a piece wrapped around the avatar as arranged, with the part beyond each fold onto itself already folded
// over, as CLO's fold arrangement does: a hem turned up, a facing turned in. Draping couldn't fold it there, as the
// hem of a leg would have to stand out from it on the way. The smaller side of a fold's line, which runs from edge to
// edge, is mirrored across it in the flat and lies where the cloth it folds onto is, a gap off it on the side it folds
// to: the right side, or the wrong side, which faces in unless the piece is turned over.
QVector<QVector3D> GarmentViewWidget::wrapped(const GarmentMesh& mesh, quint32 piece,
                                              const PieceArrangement& arrangement, qreal out) const
{
    QVector<QVector3D> positions = m_wrap->place(mesh, arrangement, out);

    GarmentMesh folded = mesh;
    QVector<int> layer(mesh.vertexCount(), 0);  // 1 folded over to the right side, -1 to the wrong side
    bool any = false;
    for (const ClothFold& fold : m_fold_editor->clothFolds(piece, mesh, fold_strength))
    {
        const bool onto_itself = fold.angle <= folded_over || fold.angle >= 360.0 - folded_over;
        if (!onto_itself || fold.vertices.size() < 2 || !mesh.boundary.contains(fold.vertices.first())
            || !mesh.boundary.contains(fold.vertices.last()))
        {
            continue;
        }
        const QPointF a = mesh.rest_positions.at(static_cast<int>(fold.vertices.first()));
        const QPointF b = mesh.rest_positions.at(static_cast<int>(fold.vertices.last()));
        const qreal length = QLineF(a, b).length();
        if (length <= on_fold_line)
        {
            continue;
        }

        QVector<int> side(mesh.vertexCount(), 0);
        int left = 0;
        int right = 0;
        for (int i = 0; i < mesh.vertexCount(); ++i)
        {
            const QPointF p = mesh.rest_positions.at(i) - a;
            const qreal across = ((b.x() - a.x()) * p.y() - (b.y() - a.y()) * p.x()) / length;
            side[i] = across > on_fold_line ? 1 : across < -on_fold_line ? -1 : 0;
            left += side.at(i) > 0 ? 1 : 0;
            right += side.at(i) < 0 ? 1 : 0;
        }
        const int over = left <= right ? 1 : -1;
        for (int i = 0; i < mesh.vertexCount(); ++i)
        {
            if (side.at(i) == over)
            {
                folded.rest_positions[i] = reflected(mesh.rest_positions.at(i), a, b);
                layer[i] = fold.angle < 180.0 ? 1 : -1;
                any = true;
            }
        }
    }

    if (any)
    {
        const qreal right_side_out = arrangement.turned_over ? -1.0 : 1.0;
        for (const int side : {1, -1})
        {
            const QVector<QVector3D> laid = mesh.carry(m_wrap->place(mesh, arrangement,
                                                                     out + side * right_side_out * fold_gap), folded);
            for (int i = 0; i < positions.size() && i < laid.size(); ++i)
            {
                if (layer.at(i) == side)
                {
                    positions[i] = laid.at(i);
                }
            }
        }
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// The pieces whose pairs are turned to their seams, as arranged; with limbs, also by the pieces on arms or legs they
// are sewn to, as those are turned now.
QSet<quint32> GarmentViewWidget::turnedPairs(bool limbs) const
{
    QSet<quint32> turned;
    for (auto arranged = m_arrangements.constBegin(); arranged != m_arrangements.constEnd(); ++arranged)
    {
        if (isTurnedPair(arranged.key(), arranged.value(), limbs))
        {
            turned.insert(arranged.key());
        }
    }
    return turned;
}

//---------------------------------------------------------------------------------------------------------------------
// Whether a piece cut twice, arranged so on an arm or a leg, is sewn mostly to body pieces on the other side of the
// body: it is drafted for the other side. Turned, the mirrored copy goes where the piece was put, so a sleeve goes to
// the armhole it is sewn to whichever arm it was put on. With limbs, a piece sewn to no body piece goes by the pieces
// on arms or legs it is sewn to instead, so a cuff goes with its sleeve.
bool GarmentViewWidget::isTurnedPair(quint32 piece, const PieceArrangement& arrangement, bool limbs) const
{
    int votes = 0;
    const CachedMesh& own = m_mesh_cache.value(piece);
    if (!m_wrap.isNull() && arrangement.part != BodyPart::Body && own.symmetry == PieceSymmetry::Pair)
    {
        auto onPart = [this, piece](quint32 partner, bool body)
        {
            return partner != piece && m_arrangements.contains(partner)
                   && (m_arrangements.value(partner).part == BodyPart::Body) == body
                   && !m_mesh_cache.value(partner).garment_mesh.isEmpty();
        };

        const QVector<QVector3D> placed = m_wrap->place(own.garment_mesh, arrangement);
        for (const bool body : {true, false})
        {
            for (const VSeam& seam : m_doc->getSeams())
            {
                for (const auto& sides : {std::make_pair(seam.firstSide(), seam.secondSide()),
                                          std::make_pair(seam.secondSide(), seam.firstSide())})
                {
                    for (const VSeamSide& own_side : sides.first)
                    {
                        for (const VSeamSide& other_side : sides.second)
                        {
                            const quint32 partner = other_side.piece_id;
                            if (own_side.piece_id == piece && onPart(partner, body)
                                && (body || (limbs && votes == 0)))
                            {
                                const CachedMesh& other = m_mesh_cache.value(partner);
                                const qreal own_across = acrossBody(own.garment_mesh, placed, own_side);
                                const qreal other_across = acrossBody(other.garment_mesh,
                                                                      piecePositions(partner, other.garment_mesh),
                                                                      other_side);
                                votes += own_across * other_across < 0 ? 1 : -1;
                            }
                        }
                    }
                }
            }
            if (votes != 0)
            {
                break;
            }
        }
    }
    return votes > 0;
}

//---------------------------------------------------------------------------------------------------------------------
// How far a seam side lies to the left of the middle of the body (+x), on average.
qreal GarmentViewWidget::acrossBody(const GarmentMesh& mesh, const QVector<QVector3D>& positions,
                                    const VSeamSide& side) const
{
    qreal across = 0;
    const QVector<quint32> vertices = mesh.stretch(side.start_node, side.end_node).vertices();
    for (const quint32 vertex : vertices)
    {
        const QVector3D position = positions.value(static_cast<int>(vertex));
        across += (position.x() - m_wrap->mirrored(position).x()) / 2.0;
    }
    return vertices.isEmpty() ? 0 : across / vertices.size();
}

//---------------------------------------------------------------------------------------------------------------------
// Meshes a piece for the board and for the garment. A piece cut on the fold without a straight side to fold along
// is taken as it is.
GarmentViewWidget::CachedMesh GarmentViewWidget::garmentMeshes(quint32 id, const PieceOutline& outline,
                                                               PieceSymmetry wanted) const
{
    CachedMesh cached;
    cached.outline = outline;
    cached.wanted = wanted;
    cached.edge_length = m_mesher.edgeLength();
    cached.mesh = m_mesher.meshOutline(outline);
    cached.mesh.piece_id = id;
    cached.symmetry = wanted;
    cached.garment_mesh = cached.mesh;

    if (wanted == PieceSymmetry::Fold)
    {
        if (outline.findFoldLine(&cached.fold_start, &cached.fold_end))
        {
            cached.garment_mesh = m_mesher.meshOutline(outline.unfolded(cached.fold_start, cached.fold_end));
            cached.garment_mesh.piece_id = id;
        }
        else
        {
            cached.symmetry = PieceSymmetry::Single;
        }
    }
    else if (wanted == PieceSymmetry::Pair)
    {
        cached.mirror_mesh = cached.mesh.mirrored();
        cached.mirror_mesh.piece_id = PieceOutline::mirrorId(id);
    }
    return cached;
}

//---------------------------------------------------------------------------------------------------------------------
// Adds the seam sewn on the board and selects it, so a twisted one can be flipped right away.
void GarmentViewWidget::sewSeam(const VSeam& seam)
{
    QVector<VSeam> seams = m_doc->getSeams();
    seams.append(seam);
    saveSeams(tr("sew pieces"), seams);
    m_seam_editor->setSelectedSeam(static_cast<int>(seams.size()) - 1);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::flipSeam()
{
    const int index = m_seam_editor->selectedSeam();
    QVector<VSeam> seams = m_doc->getSeams();
    if (index >= 0 && index < seams.size())
    {
        seams[index].reverse = !seams.at(index).reverse;
        saveSeams(tr("flip seam"), seams);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A seam type was chosen in the Sew menu for the selected seam: the angle it holds the pieces at, or none.
void GarmentViewWidget::chooseSeamType(QAction* action)
{
    const int index = m_seam_editor->selectedSeam();
    QVector<VSeam> seams = m_doc->getSeams();
    if (index < 0 || index >= seams.size())
    {
        updateActions();
        return;
    }

    qreal angle = action->data().toDouble();
    if (action == m_other_seam_angle_action)
    {
        bool chosen = false;
        angle = QInputDialog::getDouble(this, tr("Seam Angle"),
                                        tr("Angle the seam holds the pieces at, on the right side of the piece it is "
                                           "sewn from, in degrees: 180 flat, 360 turned, the pieces wrong sides "
                                           "together, 0 right sides together."),
                                        seams.at(index).angle >= 0 ? seams.at(index).angle : 180.0, 0.0, 360.0, 1,
                                        &chosen);
        if (!chosen)
        {
            updateActions();
            return;
        }
    }
    if (!qFuzzyCompare(1.0 + angle, 1.0 + seams.at(index).angle))
    {
        seams[index].angle = angle;
        saveSeams(tr("change seam type"), seams);
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// Takes out the selected seam, or while arranging puts the selected piece back on the board.
void GarmentViewWidget::removeSelected()
{
    const int index = m_seam_editor->selectedSeam();
    QVector<VSeam> seams = m_doc->getSeams();
    const quint32 piece = m_scene_model->selectedPiece();
    if (index >= 0 && index < seams.size())
    {
        seams.removeAt(index);
        m_seam_editor->setSelectedSeam(-1);
        saveSeams(tr("remove seam"), seams);
    }
    else if (m_scene_model->isArranging() && m_scene_model->isPlaced(piece))
    {
        takePieceOff();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Puts the selected piece on the avatar back on the board.
void GarmentViewWidget::takePieceOff()
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (m_scene_model->isPlaced(piece))
    {
        QVector<VPieceArrangement> arrangements = m_doc->getArrangements();
        arrangements.erase(std::remove_if(arrangements.begin(), arrangements.end(),
                                          [piece](const VPieceArrangement& arrangement)
        {
            return arrangement.piece_id == piece;
        }), arrangements.end());
        saveArrangements(tr("take piece off the avatar"), arrangements);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Esc steps back: out of dragging a piece, out of arranging, or out of what sewing or a selected seam was doing.
void GarmentViewWidget::cancel()
{
    if (m_drag.piece != 0)
    {
        callOffDrag();
    }
    else if (m_scene_model->isArranging())
    {
        m_arrange_action->setChecked(false);
    }
    else if (m_stitch_editor->isStitching())
    {
        m_stitch_editor->cancel();
    }
    else if (m_fold_editor->isFolding())
    {
        m_fold_editor->cancel();
    }
    else if (m_elastic_editor->isEditing())
    {
        m_elastic_editor->cancel();
    }
    else
    {
        m_seam_editor->cancel();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While arranging, a click on a piece picks it and a click on the avatar puts it there. Sewing, topstitching, folding,
// sewing elastic and arranging take turns.
void GarmentViewWidget::setArranging(bool arranging)
{
    if (arranging)
    {
        m_sew_action->setChecked(false);
        m_topstitch_action->setChecked(false);
        m_fold_action->setChecked(false);
        m_elastic_action->setChecked(false);
    }
    m_scene_model->setArranging(arranging);
    if (!arranging)
    {
        m_scene_model->clearPreview();
    }
    updateGizmo();
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar was clicked while arranging: the selected piece goes there, the way round it is.
void GarmentViewWidget::placePiece(const QVector3D& point)
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (piece != 0 && !m_wrap.isNull())
    {
        storeArrangement(piece, sameWayRound(piece, m_wrap->arrangementAt(point)), tr("place piece"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// An arrangement point was clicked while arranging: the selected piece goes there, the way round it is, and goes
// there on any avatar.
void GarmentViewWidget::placePieceAtPoint(int index)
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (piece != 0 && !m_wrap.isNull() && index >= 0 && index < m_wrap->points().size())
    {
        storeArrangement(piece, sameWayRound(piece, m_wrap->points().at(index).arrangement), tr("place piece"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While arranging, the mouse is over an arrangement point, or over the avatar somewhere else: shows where the selected
// piece would go if it were put there, unless it is there already, and over a point what it is called.
void GarmentViewWidget::previewArrangement(int index, const QVector3D& point)
{
    const quint32 piece = m_scene_model->selectedPiece();
    const CachedMesh cached = m_mesh_cache.value(piece);
    const bool at_point = !m_wrap.isNull() && index >= 0 && index < m_wrap->points().size();
    PieceArrangement wanted;
    bool shown = piece != 0 && !m_wrap.isNull() && m_drag.piece == 0 && !cached.garment_mesh.isEmpty();
    if (shown)
    {
        wanted = sameWayRound(piece, at_point ? m_wrap->points().at(index).arrangement : m_wrap->arrangementAt(point));
        shown = !m_scene_model->isPlaced(piece) || !sameArrangement(m_arrangements.value(piece), wanted);
    }

    if (shown)
    {
        // The piece, and its copy for a piece cut twice, as one mesh.
        const bool turned = isTurnedPair(piece, wanted);
        GarmentMesh preview;
        QVector<QVector3D> positions;
        for (const quint32 copy : {piece, PieceOutline::mirrorId(piece)})
        {
            const GarmentMesh& mesh = copy == piece ? cached.garment_mesh : cached.mirror_mesh;
            if (!mesh.isEmpty())
            {
                const quint32 offset = static_cast<quint32>(preview.rest_positions.size());
                preview.rest_positions += mesh.rest_positions;
                for (const quint32 vertex : mesh.indices)
                {
                    preview.indices.append(vertex + offset);
                }
                positions += placedAt(copy, mesh, wanted, turned, preview_out);
            }
        }
        m_scene_model->setPreview(preview, positions);
    }
    else
    {
        m_scene_model->clearPreview();
    }

    if (at_point && piece != 0)
    {
        m_scene_model->setHint(tr("%1: click to put the piece there.").arg(pointTitle(m_wrap->points().at(index))));
    }
    else
    {
        updateHint();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The mouse left the avatar while arranging.
void GarmentViewWidget::clearArrangementPreview()
{
    m_scene_model->clearPreview();
    updateHint();
}

//---------------------------------------------------------------------------------------------------------------------
// Turns the selected piece on the avatar a quarter of the way around clockwise, as seen from outside.
void GarmentViewWidget::rotatePieceClockwise()
{
    rotatePiece(90);
}

//---------------------------------------------------------------------------------------------------------------------
// Turns the selected piece on the avatar a quarter of the way around anticlockwise, as seen from outside.
void GarmentViewWidget::rotatePieceCounterclockwise()
{
    rotatePiece(-90);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::rotatePiece(qreal degrees)
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (m_scene_model->isPlaced(piece))
    {
        PieceArrangement arrangement = m_arrangements.value(piece);
        arrangement.rotation = std::fmod(arrangement.rotation + degrees + 360.0, 360.0);
        storeArrangement(piece, arrangement, tr("rotate piece"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Lays the selected piece on the piece on the avatar it is sewn to the most, as CLO's Superimpose: over it, under it,
// or beside it, its sides of their seams on that piece's, as one step. On a pair turned to its seams, whose copy is
// where it is arranged, a piece cut twice is laid with its copy on that copy, and goes with it.
void GarmentViewWidget::superimposePiece(Superimpose how)
{
    const quint32 piece = m_scene_model->selectedPiece();
    QVector<SewnSides> seams;
    const quint32 partner = superimposePartner(piece, &seams);
    if (partner != 0 && !m_wrap.isNull() && m_mesh_cache.contains(piece))
    {
        const CachedMesh own = m_mesh_cache.value(piece);
        const CachedMesh other = m_mesh_cache.value(partner);
        const bool turned = m_turned_pairs.contains(partner);
        const bool copies = turned && !own.mirror_mesh.isEmpty();
        const PieceArrangement laid = m_wrap->superimposed(copies ? own.mirror_mesh : own.garment_mesh,
                                                           turned ? other.mirror_mesh : other.garment_mesh,
                                                           m_arrangements.value(partner), seams, how);
        storeArrangement(piece, laid, tr("superimpose piece"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The piece on the avatar a piece is sewn to the most, by the length of their seams, and the sides of those seams,
// each in the order in which they meet; 0 if it is sewn to none on the avatar. Seams going on over several stretches
// don't count, as the piece meets only part of the other side.
quint32 GarmentViewWidget::superimposePartner(quint32 piece, QVector<SewnSides>* seams) const
{
    QMap<quint32, qreal> sewn;
    QVector<VSeam> all_seams = m_doc->getSeams();
    all_seams.erase(std::remove_if(all_seams.begin(), all_seams.end(), [](const VSeam& seam)
    {
        return !seam.first_more.isEmpty() || !seam.second_more.isEmpty();
    }), all_seams.end());
    for (const VSeam& seam : all_seams)
    {
        for (const auto& sides : {std::make_pair(seam.first, seam.second), std::make_pair(seam.second, seam.first)})
        {
            const quint32 partner = sides.second.piece_id;
            if (sides.first.piece_id == piece && partner != piece && m_scene_model->isPlaced(partner)
                && m_mesh_cache.contains(partner))
            {
                sewn[partner] += m_mesh_cache.value(partner).garment_mesh.stretch(sides.second.start_node,
                                                                                  sides.second.end_node).length();
            }
        }
    }

    quint32 best = 0;
    qreal most = 0;
    for (auto partner = sewn.constBegin(); partner != sewn.constEnd(); ++partner)
    {
        if (partner.value() > most)
        {
            best = partner.key();
            most = partner.value();
        }
    }

    if (best != 0 && seams != nullptr && m_mesh_cache.contains(piece))
    {
        const GarmentMesh& own = m_mesh_cache.value(piece).garment_mesh;
        const GarmentMesh& other = m_mesh_cache.value(best).garment_mesh;
        for (const VSeam& seam : all_seams)
        {
            const bool first = seam.first.piece_id == piece && seam.second.piece_id == best;
            const bool second = seam.second.piece_id == piece && seam.first.piece_id == best;
            if (first || second)
            {
                const VSeamSide& own_side = first ? seam.first : seam.second;
                const VSeamSide& other_side = first ? seam.second : seam.first;
                SeamStretch own_stretch = own.stretch(own_side.start_node, own_side.end_node);
                SeamStretch other_stretch = other.stretch(other_side.start_node, other_side.end_node);

                // The second side of a seam turned around runs back from the end the first side starts at.
                if (seam.reverse)
                {
                    if (first)
                    {
                        other_stretch = other_stretch.reversed();
                    }
                    else
                    {
                        own_stretch = own_stretch.reversed();
                    }
                }
                seams->append({own_stretch.vertices(), other_stretch.vertices()});
            }
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
// Turns the selected piece on the avatar over, its other side out, as if it were cut from the cloth turned over.
void GarmentViewWidget::turnPieceOver()
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (m_scene_model->isPlaced(piece))
    {
        PieceArrangement arrangement = m_arrangements.value(piece);
        arrangement.turned_over = !arrangement.turned_over;
        storeArrangement(piece, arrangement, tr("turn piece over"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A layer was chosen for the selected piece, as CLO's: it is worn over the pieces of lower layers where it lies on them.
void GarmentViewWidget::chooseLayer(QAction* action)
{
    const quint32 piece = m_scene_model->selectedPiece();
    const int layer = action->data().toInt();
    if (piece != 0 && layer != layerOf(piece))
    {
        const QVector<VPieceLayer> before = m_doc->getLayers();
        QVector<VPieceLayer> after = before;
        auto worn = std::find_if(after.begin(), after.end(), [piece](const VPieceLayer& kept)
        {
            return kept.piece_id == piece;
        });
        if (worn != after.end())
        {
            worn->layer = layer;
        }
        else
        {
            after.append({piece, layer});
        }
        qApp->getUndoStack()->push(new SaveLayers(tr("change piece layer"), before, after, m_doc));
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's layers changed, by choosing one here or by undo and redo. A drape going on goes on with them.
void GarmentViewWidget::updateLayers()
{
    if (m_runner->isRunning())
    {
        startSimulation();
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The layer a pattern piece is worn in: 0, next to the body, unless the pattern says otherwise.
int GarmentViewWidget::layerOf(quint32 piece) const
{
    for (const VPieceLayer& layer : m_doc->getLayers())
    {
        if (layer.piece_id == piece)
        {
            return layer.layer;
        }
    }
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------
// A piece on the avatar was right-clicked, and with that picked, at a point of the view: what can be done with it.
void GarmentViewWidget::showPieceMenu(const QPointF& at)
{
    updateActions();
    m_piece_menu->popup(m_view_container->mapToGlobal(at.toPoint()));
}

//---------------------------------------------------------------------------------------------------------------------
// A place for a piece, the way round the piece is on the avatar, if it is: turned, leaning and swinging as it is, as
// far out from the body.
PieceArrangement GarmentViewWidget::sameWayRound(quint32 piece, PieceArrangement wanted) const
{
    if (m_arrangements.contains(piece))
    {
        const PieceArrangement& now = m_arrangements.value(piece);
        wanted.rotation = now.rotation;
        wanted.turned_over = now.turned_over;
        wanted.distance = now.distance;
        wanted.lean = now.lean;
        wanted.swing = now.swing;
    }
    return wanted;
}

//---------------------------------------------------------------------------------------------------------------------
// What an arrangement point is called: the part of the body, the side of it and the level, left and right being the
// avatar's own.
QString GarmentViewWidget::pointTitle(const ArrangementPoint& point) const
{
    QString part;
    switch (point.arrangement.part)
    {
        case BodyPart::LeftLeg:
            part = tr("Left leg");
            break;
        case BodyPart::RightLeg:
            part = tr("Right leg");
            break;
        case BodyPart::LeftArm:
            part = tr("Left arm");
            break;
        case BodyPart::RightArm:
            part = tr("Right arm");
            break;
        case BodyPart::Body:
        default:
            part = tr("Body");
            break;
    }

    const QHash<QString, QString> sides = {
        {QStringLiteral("front"), tr("front")},
        {QStringLiteral("frontLeft"), tr("front left")},
        {QStringLiteral("left"), tr("left side")},
        {QStringLiteral("backLeft"), tr("back left")},
        {QStringLiteral("back"), tr("back")},
        {QStringLiteral("backRight"), tr("back right")},
        {QStringLiteral("right"), tr("right side")},
        {QStringLiteral("frontRight"), tr("front right")},
        {QStringLiteral("outside"), tr("outside")},
        {QStringLiteral("inside"), tr("inside")}};
    const QHash<QString, QString> levels = {
        {QStringLiteral("neck"), tr("at the neck")},
        {QStringLiteral("bust"), tr("at the bust")},
        {QStringLiteral("waist"), tr("at the waist")},
        {QStringLiteral("hip"), tr("at the hip")},
        {QStringLiteral("thigh"), tr("at the thigh")},
        {QStringLiteral("knee"), tr("at the knee")},
        {QStringLiteral("calf"), tr("at the calf")},
        {QStringLiteral("upperArm"), tr("at the upper arm")},
        {QStringLiteral("elbow"), tr("at the elbow")},
        {QStringLiteral("wrist"), tr("at the wrist")}};
    return tr("%1, %2, %3").arg(part, sides.value(point.side, point.side), levels.value(point.level, point.level));
}

//---------------------------------------------------------------------------------------------------------------------
// A placed piece was pressed on while arranging: until it is let go, it slides around the part of the body it is on
// with the mouse, keeping where the mouse took hold of it. A drape going on stops.
void GarmentViewWidget::grabPiece(quint32 id, const QVector3D& point)
{
    const quint32 piece = patternPiece(id);
    if (beginDrag(piece))
    {
        m_drag.mirrored = PieceOutline::isMirrorId(id) != m_turned_pairs.contains(piece);
        m_drag.grabbed = m_wrap->arrangementOn(m_drag.start.part, m_drag.mirrored ? m_wrap->mirrored(point) : point);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Takes hold of a piece on the avatar, to move it: a drape going on stops, and the piece's drape is kept, back if the
// move is called off. Says whether the piece is on the avatar.
bool GarmentViewWidget::beginDrag(quint32 piece)
{
    const bool placed = !m_wrap.isNull() && m_arrangements.contains(piece);
    if (placed)
    {
        m_simulate_action->setChecked(false);

        m_drag = PieceDrag();
        m_drag.piece = piece;
        m_drag.start = m_arrangements.value(piece);
        m_drag.current = m_drag.start;
        for (const quint32 copy : {piece, PieceOutline::mirrorId(piece)})
        {
            if (m_draped.contains(copy))
            {
                m_drag.draped.insert(copy, m_draped.value(copy));
            }
        }
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
// The mouse moved on with a piece held: the piece goes as far around and up or down its part as the mouse did.
void GarmentViewWidget::dragPiece(const QVector3D& point)
{
    if (m_drag.piece != 0 && !m_wrap.isNull())
    {
        const PieceArrangement at = m_wrap->arrangementOn(m_drag.start.part,
                                                          m_drag.mirrored ? m_wrap->mirrored(point) : point);
        m_drag.current.angle = std::remainder(m_drag.start.angle + at.angle - m_drag.grabbed.angle, 360.0);
        m_drag.current.height = m_drag.start.height + at.height - m_drag.grabbed.height;
        m_drag.current.point.clear();
        showArrangement(m_drag.piece, m_drag.current);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The held piece was let go.
void GarmentViewWidget::dropPiece()
{
    finishDrag(tr("move piece"));
}

//---------------------------------------------------------------------------------------------------------------------
// Where the held piece was moved or turned to is stored as one step, or, if it didn't move, it stays as it was.
void GarmentViewWidget::finishDrag(const QString& text)
{
    if (m_drag.piece != 0)
    {
        if (!sameArrangement(m_drag.current, m_drag.start))
        {
            const quint32 piece = m_drag.piece;
            const PieceArrangement current = m_drag.current;
            m_drag = PieceDrag();
            storeArrangement(piece, current, text);
        }
        else
        {
            callOffDrag();
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A part of the gizmo of the selected piece was pressed on: until it is let go, the piece moves along that arrow or
// turns around that ring with the mouse. A drape going on stops.
void GarmentViewWidget::grabGizmo(int part)
{
    const quint32 piece = m_scene_model->selectedPiece();
    PieceFrame frame;
    QVector3D origin;
    if (gizmoFrame(piece, &frame, &origin) && beginDrag(piece))
    {
        m_drag.gizmo = part;
        m_drag.frame = frame;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The mouse moved on with a part of the gizmo held, by an amount in cm along its arrow or degrees around its ring: the
// piece moves around its part of the body, up or down it, or out from it, or rotates, leans or swings, that much from
// where it was. Moved around or up or down, it leaves its arrangement point.
void GarmentViewWidget::dragGizmo(qreal amount)
{
    if (m_drag.piece == 0 || m_drag.gizmo < 0 || m_wrap.isNull())
    {
        return;
    }

    PieceArrangement current = m_drag.start;
    const PieceFrame& frame = m_drag.frame;
    const float along = static_cast<float>(amount);
    switch (m_drag.gizmo)
    {
        case GarmentSceneModel::MoveAcross:
            current.angle = m_wrap->arrangementOn(current.part, frame.middle + frame.across * along).angle;
            current.point.clear();
            break;
        case GarmentSceneModel::MoveUp:
            current.height = m_wrap->arrangementOn(current.part, frame.middle + frame.up * along).height;
            current.point.clear();
            break;
        case GarmentSceneModel::MoveOut:
            current.distance = qMax(m_drag.start.distance + amount, nearest_distance);
            break;
        case GarmentSceneModel::Rotate:
            current.rotation = std::fmod(snappedAngle(m_drag.start.rotation + amount) + 720.0, 360.0);
            break;
        case GarmentSceneModel::Lean:
            current.lean = qBound(-steepest_turn, snappedAngle(m_drag.start.lean + amount), steepest_turn);
            break;
        case GarmentSceneModel::Swing:
        default:
            current.swing = qBound(-steepest_turn, snappedAngle(m_drag.start.swing + amount), steepest_turn);
            break;
    }
    m_drag.current = current;
    showArrangement(m_drag.piece, current);
}

//---------------------------------------------------------------------------------------------------------------------
// The gizmo was let go.
void GarmentViewWidget::dropGizmo()
{
    const int part = m_drag.gizmo;
    finishDrag(part == GarmentSceneModel::Rotate ? tr("rotate piece")
               : part == GarmentSceneModel::Lean ? tr("lean piece")
               : part == GarmentSceneModel::Swing ? tr("swing piece")
                                                  : tr("move piece"));
}

//---------------------------------------------------------------------------------------------------------------------
// The mouse came over a part of the gizmo, or left it: what dragging it does.
void GarmentViewWidget::hoverGizmo(int part)
{
    QString hint;
    switch (part)
    {
        case GarmentSceneModel::MoveAcross:
            hint = tr("Drag to move the piece around the body, or around the leg or the arm it is on.");
            break;
        case GarmentSceneModel::MoveUp:
            hint = tr("Drag to move the piece up or down.");
            break;
        case GarmentSceneModel::MoveOut:
            hint = tr("Drag to move the piece further out from the body, or closer to it.");
            break;
        case GarmentSceneModel::Rotate:
            hint = tr("Drag to rotate the piece.");
            break;
        case GarmentSceneModel::Lean:
            hint = tr("Drag to lean the piece, its top out or in.");
            break;
        case GarmentSceneModel::Swing:
            hint = tr("Drag to swing the piece, one side out.");
            break;
        default:
            break;
    }
    if (hint.isEmpty())
    {
        updateHint();
    }
    else
    {
        m_scene_model->clearPreview();
        m_scene_model->setHint(hint);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While arranging, the selected piece on the avatar has its gizmo; otherwise there is none.
void GarmentViewWidget::updateGizmo()
{
    QVariantMap gizmo;
    PieceFrame frame;
    QVector3D origin;
    if (m_scene_model->isArranging() && gizmoFrame(m_scene_model->selectedPiece(), &frame, &origin))
    {
        gizmo.insert(QStringLiteral("origin"), origin);
        gizmo.insert(QStringLiteral("across"), frame.across);
        gizmo.insert(QStringLiteral("up"), frame.up);
        gizmo.insert(QStringLiteral("out"), frame.out);
    }
    m_scene_model->setGizmo(gizmo);
}

//---------------------------------------------------------------------------------------------------------------------
// Where a piece's gizmo goes, if the piece is on the avatar: facing as the piece does at its middle where its
// arrangement puts it, of a pair turned to its seams the copy, and on the piece as it is shown, draped or not, at the
// point of it nearest that middle.
bool GarmentViewWidget::gizmoFrame(quint32 piece, PieceFrame* frame, QVector3D* origin) const
{
    const bool placed = !m_wrap.isNull() && m_arrangements.contains(piece) && m_scene_model->isPlaced(piece);
    if (placed)
    {
        const bool turned = m_turned_pairs.contains(piece);
        const CachedMesh cached = m_mesh_cache.value(piece);
        const GarmentMesh& mesh = turned ? cached.mirror_mesh : cached.garment_mesh;
        *frame = m_wrap->frameOf(mesh, m_arrangements.value(piece));

        const QVector<QVector3D> shown = piecePositions(turned ? PieceOutline::mirrorId(piece) : piece, mesh);
        const QPointF middle = mesh.bounds().center();
        int nearest = -1;
        qreal nearest_distance_to_middle = std::numeric_limits<qreal>::infinity();
        for (int i = 0; i < mesh.rest_positions.size(); ++i)
        {
            const qreal apart = QLineF(mesh.rest_positions.at(i), middle).length();
            if (apart < nearest_distance_to_middle)
            {
                nearest = i;
                nearest_distance_to_middle = apart;
            }
        }
        *origin = nearest >= 0 && nearest < shown.size() ? shown.at(nearest) : frame->middle;
    }
    return placed;
}

//---------------------------------------------------------------------------------------------------------------------
// Puts a dragged piece back where it was, drape and all.
void GarmentViewWidget::callOffDrag()
{
    const PieceDrag drag = m_drag;
    m_drag = PieceDrag();
    if (drag.piece != 0)
    {
        m_arrangements.insert(drag.piece, drag.start);
        for (auto draped = drag.draped.constBegin(); draped != drag.draped.constEnd(); ++draped)
        {
            m_draped.insert(draped.key(), draped.value());
        }
        showPlaced(drag.piece);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Shows a piece and its copy where an arrangement puts them, while the piece is dragged there; the pattern only gets
// it when the piece is let go.
void GarmentViewWidget::showArrangement(quint32 piece, const PieceArrangement& arrangement)
{
    m_arrangements.insert(piece, arrangement);
    m_draped.remove(piece);
    m_draped.remove(PieceOutline::mirrorId(piece));
    showPlaced(piece);
}

//---------------------------------------------------------------------------------------------------------------------
// Moves a placed piece and its copy in the scene to where they are now, draped or arranged.
void GarmentViewWidget::showPlaced(quint32 piece)
{
    const CachedMesh cached = m_mesh_cache.value(piece);
    for (const quint32 copy : {piece, PieceOutline::mirrorId(piece)})
    {
        const GarmentMesh& mesh = copy == piece ? cached.garment_mesh : cached.mirror_mesh;
        if (!mesh.isEmpty())
        {
            m_scene_model->setPiecePositions(copy, piecePositions(copy, mesh));
        }
    }
    showSeamsOnAvatar();
    updateGizmo();
}

//---------------------------------------------------------------------------------------------------------------------
// Stores where a piece is put on the avatar in the pattern, as one undo step.
void GarmentViewWidget::storeArrangement(quint32 piece, const PieceArrangement& wanted, const QString& text)
{
    VPieceArrangement arrangement;
    arrangement.piece_id = piece;
    arrangement.part = BodyWrap::partName(wanted.part);
    arrangement.angle = wanted.angle;
    arrangement.height = wanted.height;
    arrangement.rotation = wanted.rotation;
    arrangement.turned_over = wanted.turned_over;
    arrangement.point = wanted.point;
    arrangement.distance = wanted.distance;
    arrangement.lean = wanted.lean;
    arrangement.swing = wanted.swing;

    QVector<VPieceArrangement> arrangements = m_doc->getArrangements();
    auto existing = std::find_if(arrangements.begin(), arrangements.end(), [piece](const VPieceArrangement& other)
    {
        return other.piece_id == piece;
    });
    if (existing != arrangements.end())
    {
        *existing = arrangement;
    }
    else
    {
        arrangements.append(arrangement);
    }
    saveArrangements(text, arrangements);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::setSimulating(bool simulating)
{
    if (simulating)
    {
        startSimulation();
    }
    else
    {
        m_runner->stop();
        keepDrape();
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// Puts the pieces from the arrangement into a solver, each in its layer, the seams between them as stitches and the
// avatar as what they drape over, and sets it going. Pieces already draped go on from where they are.
void GarmentViewWidget::startSimulation()
{
    m_runner->stop();
    m_drape_pieces.clear();

    QSharedPointer<ClothSolver> solver(new ClothSolver());
    QHash<quint32, quint32> offsets;
    QHash<quint32, GarmentMesh> meshes;
    GarmentSymmetry symmetry;
    const VGarmentFabrics fabrics = m_doc->getFabrics();
    QHash<quint32, int> layers;
    for (const VPieceLayer& layer : m_doc->getLayers())
    {
        layers.insert(layer.piece_id, layer.layer);
    }
    for (const GarmentPiece& garment_piece : m_garment_pieces)
    {
        const QVector<QVector3D> positions = piecePositions(garment_piece.id, garment_piece.mesh);
        if (!positions.isEmpty())
        {
            DrapePiece drape_piece;
            drape_piece.id = garment_piece.id;
            const quint32 piece = patternPiece(garment_piece.id);
            Fabric fabric = namedFabric(fabrics, fabrics.of(piece));
            const VFabricShrinkage shrinkage = fabrics.shrinkageOf(piece);
            if (!shrinkage.isNull())
            {
                fabric.shrinkage_weft = shrinkage.weft;
                fabric.shrinkage_warp = shrinkage.warp;
            }
            ClothLayer layer;
            layer.number = layers.value(piece, 0);
            layer.turned_over = m_arrangements.value(piece).turned_over;
            drape_piece.offset = static_cast<int>(solver->addMesh(garment_piece.mesh, positions, fabric,
                                                                  garment_piece.grain_angle,
                                                                  m_fold_editor->clothFolds(piece, garment_piece.mesh,
                                                                                            fold_strength),
                                                                  layer));
            drape_piece.count = garment_piece.mesh.vertexCount();
            for (const ClothElastic& elastic : m_elastic_editor->clothElastics(piece, garment_piece.mesh,
                                                                              static_cast<quint32>(drape_piece.offset)))
            {
                solver->addElastic(elastic);
            }
            m_drape_pieces.append(drape_piece);
            offsets.insert(garment_piece.id, static_cast<quint32>(drape_piece.offset));
            meshes.insert(garment_piece.id, garment_piece.mesh);

            const CachedMesh& cached = m_mesh_cache.value(patternPiece(garment_piece.id));
            symmetry.setPiece(patternPiece(garment_piece.id), cached.symmetry, cached.fold_start, cached.fold_end);
        }
    }

    if (m_drape_pieces.isEmpty())
    {
        const QSignalBlocker blocker(m_simulate_action);
        m_simulate_action->setChecked(false);
        m_scene_model->setHint(tr("Put pieces on the avatar first: Arrange, click a piece, then the avatar."));
        return;
    }

    // The pattern's seams, and their twins on the other side of the garment. A side going on over several stretches
    // is sewn as one, each stretch after the one before.
    QVector<GarmentSeam> seams;
    for (const VSeam& stored : m_doc->getSeams())
    {
        seams.append(toGarmentSeam(stored));
    }
    auto draped_side = [&meshes, &offsets](const QVector<GarmentSeamSide>& side)
    {
        QVector<SeamStretch> stretches;
        for (const GarmentSeamSide& stretch : side)
        {
            if (!offsets.contains(stretch.piece))
            {
                return SeamStretch();
            }
            const SeamStretch along = meshes.value(stretch.piece).stretch(stretch.start_node, stretch.end_node,
                                                                          offsets.value(stretch.piece));
            stretches.append(stretch.backward ? along.reversed() : along);
        }
        return SeamStretch::joined(stretches);
    };
    for (const GarmentSeam& seam : symmetry.madeUp(seams))
    {
        const SeamStretch first = draped_side(seam.firstSide());
        SeamStretch second = draped_side(seam.secondSide());
        if (!first.isEmpty() && !second.isEmpty())
        {
            if (seam.reverse)
            {
                second = second.reversed();
            }
            solver->addStitches(SeamStretch::stitches(first, second));
            if (seam.angle >= 0)
            {
                solver->addSeamFold(first, second, seam.angle, fold_strength);
            }
        }
    }

    solver->setCollider(m_collider);
    updateHolds();
    m_resting = false;
    m_runner->start(solver);
}

//---------------------------------------------------------------------------------------------------------------------
// Back to the pieces as arranged, before any draping.
void GarmentViewWidget::resetDrape()
{
    m_simulate_action->setChecked(false);
    m_draped.clear();
    m_pins.clear();
    rebuildScene();
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::drapeFrame(int generation, const QVector<QVector3D>& positions)
{
    if (generation == m_runner->generation())
    {
        for (const DrapePiece& drape_piece : m_drape_pieces)
        {
            if (drape_piece.offset + drape_piece.count <= positions.size())
            {
                const QVector<QVector3D> piece_positions = positions.mid(drape_piece.offset, drape_piece.count);
                m_draped.insert(drape_piece.id, piece_positions);
                m_scene_model->setPiecePositions(drape_piece.id, piece_positions);
            }
        }
        showSeamsOnAvatar();
        if (m_scene_model->isArranging())
        {
            updateGizmo();
        }
        m_runner->frameShown();
        m_reset_action->setEnabled(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the cloth is moving: the drape is on and hasn't come to rest.
bool GarmentViewWidget::isDraping() const
{
    return m_runner->isRunning() && !m_resting;
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth came to rest. The drape stays on, working out nothing, until the cloth is pulled or pinned somewhere else.
void GarmentViewWidget::drapeSettled(int generation)
{
    if (generation == m_runner->generation())
    {
        m_resting = true;
        updateHint();
        keepDrape();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth at rest was pulled or pinned somewhere else, so it drapes again.
void GarmentViewWidget::drapeWoke(int generation)
{
    if (generation == m_runner->generation())
    {
        m_resting = false;
        updateHint();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::updateActions()
{
    const QSignalBlocker sew_blocker(m_sew_action);
    m_sew_action->setChecked(m_seam_editor->isSewing());
    if (m_seam_editor->isSewing() && m_scene_model->isArranging())
    {
        m_arrange_action->setChecked(false);
    }
    if (m_seam_editor->isSewing() && m_stitch_editor->isStitching())
    {
        m_stitch_editor->setStitching(false);
    }
    if (m_seam_editor->isSewing() && m_fold_editor->isFolding())
    {
        m_fold_editor->setFolding(false);
    }
    if (m_seam_editor->isSewing() && m_elastic_editor->isEditing())
    {
        m_elastic_editor->setEditing(false);
    }
    const QSignalBlocker stitch_blocker(m_topstitch_action);
    m_topstitch_action->setChecked(m_stitch_editor->isStitching());
    const QSignalBlocker fold_blocker(m_fold_action);
    m_fold_action->setChecked(m_fold_editor->isFolding());
    bool known_angle = false;
    for (QAction* angle : m_fold_angles->actions())
    {
        const bool chosen = angle != m_other_angle_action
                            && qFuzzyCompare(1.0 + angle->data().toDouble(), 1.0 + m_fold_editor->angle());
        angle->setChecked(chosen);
        known_angle = known_angle || chosen;
    }
    m_other_angle_action->setChecked(!known_angle);
    const QSignalBlocker elastic_blocker(m_elastic_action);
    m_elastic_action->setChecked(m_elastic_editor->isEditing());
    bool known_ratio = false;
    for (QAction* ratio : m_elastic_ratios->actions())
    {
        const bool chosen = ratio != m_other_ratio_action
                            && qFuzzyCompare(1.0 + ratio->data().toDouble(), 1.0 + m_elastic_editor->ratio());
        ratio->setChecked(chosen);
        known_ratio = known_ratio || chosen;
    }
    m_other_ratio_action->setChecked(!known_ratio);
    const VTopstitches topstitches = m_doc->getTopstitches();
    m_every_edge_action->setChecked(topstitches.all);
    const QString chosen_style = TopstitchStyle::preset(topstitches.style).name;
    for (QAction* style : m_stitch_styles->actions())
    {
        style->setChecked(style->data().toString() == chosen_style);
    }
    const QColor thread(topstitches.color);
    bool known_thread = false;
    for (QAction* color : m_threads->actions())
    {
        const bool same = color->data().toString() == (thread.isValid() ? thread.name() : QString());
        color->setChecked(same && color != m_other_thread_action);
        known_thread = known_thread || (same && color != m_other_thread_action);
    }
    m_other_thread_action->setChecked(!known_thread);

    const bool seam_selected = m_seam_editor->selectedSeam() >= 0;
    const qreal seam_angle = seam_selected ? m_doc->getSeams().value(m_seam_editor->selectedSeam()).angle : -1.0;
    bool known_seam_angle = false;
    for (QAction* type : m_seam_types->actions())
    {
        const qreal angle = type->data().toDouble();
        const bool chosen = seam_selected && type != m_other_seam_angle_action
                            && (angle < 0 ? seam_angle < 0 : qFuzzyCompare(1.0 + angle, 1.0 + seam_angle));
        type->setEnabled(seam_selected);
        type->setChecked(chosen);
        known_seam_angle = known_seam_angle || chosen;
    }
    m_other_seam_angle_action->setChecked(seam_selected && !known_seam_angle);
    const bool placed_selected = m_scene_model->isArranging()
                                 && m_scene_model->isPlaced(m_scene_model->selectedPiece());
    m_flip_action->setEnabled(seam_selected);
    m_remove_action->setEnabled(seam_selected || placed_selected);
    const bool on_avatar = m_scene_model->isPlaced(m_scene_model->selectedPiece());
    m_rotate_clockwise_action->setEnabled(on_avatar);
    m_rotate_counterclockwise_action->setEnabled(on_avatar);
    m_turn_over_action->setEnabled(on_avatar);
    m_take_off_action->setEnabled(on_avatar);
    const bool sewn_to_placed = !m_wrap.isNull() && superimposePartner(m_scene_model->selectedPiece()) != 0;
    m_superimpose_over_action->setEnabled(sewn_to_placed);
    m_superimpose_under_action->setEnabled(sewn_to_placed);
    m_superimpose_side_action->setEnabled(sewn_to_placed);
    const quint32 selected_piece = m_scene_model->selectedPiece();
    const int selected_layer = layerOf(selected_piece);
    m_layer_menu->menuAction()->setEnabled(selected_piece != 0);
    for (QAction* layer : m_piece_layers->actions())
    {
        layer->setChecked(selected_piece != 0 && layer->data().toInt() == selected_layer);
    }

    const bool has_avatar = m_scene_model->hasAvatar();
    m_avatar_action->setEnabled(!measuredAvatar().hasMeasurements());
    m_arrange_action->setEnabled(has_avatar);
    m_simulate_action->setEnabled(has_avatar);
    m_reset_action->setEnabled(!m_draped.isEmpty());
    m_export_action->setEnabled(has_avatar && !m_scene_model->placedPieces().isEmpty());
    m_snapshot_action->setEnabled(m_scene_model->pieceCount() > 0 || has_avatar);
    m_remove_pins_action->setEnabled(!m_pins.isEmpty());
    m_scene_model->setSimulating(m_runner->isRunning());
    m_hide_piece_action->setEnabled(m_scene_model->selectedPiece() != 0);
    m_show_pieces_action->setEnabled(!m_scene_model->hiddenPieces().isEmpty());

    // With nothing to step back from, Esc is left to the main window.
    const bool stitching = m_stitch_editor->isStitching() || m_fold_editor->isFolding()
                           || m_elastic_editor->isEditing();
    m_cancel_action->setEnabled(m_seam_editor->isSewing() || seam_selected || m_scene_model->isArranging()
                                || stitching);

    if ((m_seam_editor->isSewing() || m_scene_model->isArranging() || stitching) && m_view_container != nullptr)
    {
        m_view_container->setFocus();
    }
    updateHint();

    // The fabric shown is the selected piece's, or the garment's; only one of the pattern's own can be edited or
    // added to the fabric library.
    const VGarmentFabrics fabrics = m_doc->getFabrics();
    updateFabricBox(fabrics);
    const quint32 selected = m_scene_model->selectedPiece();
    const QString fabric = selected != 0 ? fabrics.of(selected) : fabrics.garment;
    const int index = m_fabric_box->findData(fabric.isEmpty() ? Fabric::defaultName() : fabric);
    m_fabric_box->setCurrentIndex(qMax(index, 0));
    if (auto* items = qobject_cast<QStandardItemModel*>(m_fabric_box->model()))
    {
        for (const int command : {edit_fabric, add_to_library})
        {
            if (QStandardItem* item = items->item(m_fabric_box->findData(command, fabric_command_role)))
            {
                item->setEnabled(!fabrics.customFabric(fabric).name.isEmpty());
            }
        }
    }

    const bool own_image = !ownFabricImage().isNull();
    m_image_width_action->setEnabled(own_image);
    m_remove_image_action->setEnabled(own_image);
}

//---------------------------------------------------------------------------------------------------------------------
// The fabrics were changed, here or by undo. The pieces show how thick theirs are, and a drape running starts over in
// them.
void GarmentViewWidget::updateFabrics()
{
    updateActions();
    m_rebuild_timer->start();
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric was picked: for the selected piece, or for the whole garment when no piece is selected, one of the fabric
// library's taken into the pattern first; or a new fabric of the pattern's own asked for, the one shown to be edited
// or added to the library, or the library's folder to be opened.
void GarmentViewWidget::chooseFabric(int index)
{
    const QString library_file = m_fabric_box->itemData(index, fabric_file_role).toString();
    const int command = m_fabric_box->itemData(index, fabric_command_role).toInt();
    if (!library_file.isEmpty() || command != 0)
    {
        switch (command)
        {
            case new_fabric:
                editFabric(true);
                break;
            case edit_fabric:
                editFabric(false);
                break;
            case add_to_library:
                addFabricToLibrary();
                break;
            case open_library:
                openFabricLibrary();
                break;
            default:
                takeLibraryFabric(library_file);
                break;
        }
        updateActions();  // the box shows the fabric again
        return;
    }

    const VGarmentFabrics before = m_doc->getFabrics();
    const VGarmentFabrics after = cutFrom(before, m_fabric_box->itemData(index).toString());
    if (!(after == before))
    {
        qApp->getUndoStack()->push(new SaveFabrics(tr("change fabric"), before, after, m_doc));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric of the pattern's own was asked for, as CLO's fabric properties: a new one, starting from the fabric shown,
// which the selected piece, or the whole garment when no piece is selected, is then cut from; or the one shown,
// changed, the pieces cut from it following it to a new name, or deleted, what was cut from it cut from the garment's
// fabric again, the garment from the default. Each is one step to undo.
void GarmentViewWidget::editFabric(bool make_new)
{
    const VGarmentFabrics before = m_doc->getFabrics();
    const quint32 selected = m_scene_model->selectedPiece();
    const QString shown = selected != 0 ? before.of(selected) : before.garment;
    const VCustomFabric kept = before.customFabric(shown);
    if (!make_new && kept.name.isEmpty())
    {
        return;
    }

    const QStringList taken = takenFabricNames(before, make_new ? QString() : kept.name);
    QVector<FabricDialog::Start> starts;
    for (const Fabric& fabric : Fabric::presets())
    {
        starts.append({fabricTitle(fabric.name), customFabric(fabric, QString())});
    }
    VCustomFabric start = kept;
    if (make_new)
    {
        int number = 1;
        auto is_taken = [&taken](const QString& name)
        {
            return taken.contains(name, Qt::CaseInsensitive);
        };
        while (is_taken(tr("Fabric %1").arg(number)))
        {
            ++number;
        }
        start = customFabric(namedFabric(before, shown.isEmpty() ? Fabric::defaultName() : shown),
                             tr("Fabric %1").arg(number));
    }

    FabricDialog dialog(start, starts, taken, !make_new, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    VGarmentFabrics after = before;
    QString text;
    auto same_name = [&kept](const VCustomFabric& fabric)
    {
        return fabric.name == kept.name;
    };
    if (make_new)
    {
        after.custom.append(dialog.fabric());
        after = cutFrom(after, dialog.fabric().name);
        text = tr("new fabric");
    }
    else if (dialog.isDeleted())
    {
        after.custom.erase(std::remove_if(after.custom.begin(), after.custom.end(), same_name), after.custom.end());
        after.garment = after.garment == kept.name ? QString() : after.garment;
        for (VPieceFabric& piece : after.pieces)
        {
            piece.fabric = piece.fabric == kept.name ? QString() : piece.fabric;
        }
        after.pieces.erase(std::remove_if(after.pieces.begin(), after.pieces.end(), [](const VPieceFabric& piece)
        {
            return piece.fabric.isEmpty() && piece.texture.isNull() && piece.shrinkage.isNull();
        }), after.pieces.end());
        text = tr("delete fabric");
    }
    else
    {
        const VCustomFabric changed = dialog.fabric();
        std::replace_if(after.custom.begin(), after.custom.end(), same_name, changed);
        after.garment = after.garment == kept.name ? changed.name : after.garment;
        for (VPieceFabric& piece : after.pieces)
        {
            piece.fabric = piece.fabric == kept.name ? changed.name : piece.fabric;
        }
        text = tr("edit fabric");
    }

    if (!(after == before))
    {
        qApp->getUndoStack()->push(new SaveFabrics(text, before, after, m_doc));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric of the library was picked: it becomes one of the pattern's own, which the selected piece, or the whole
// garment when no piece is selected, is then cut from, as one step to undo. Named as one of the 3D View's fabrics, or,
// but for its case, as one of the pattern's own, it gets a number.
void GarmentViewWidget::takeLibraryFabric(const QString& path)
{
    VCustomFabric fabric;
    for (const FabricLibrary::Entry& entry : m_fabric_library->entries())
    {
        fabric = entry.path == path ? entry.fabric : fabric;
    }
    if (fabric.name.isEmpty())
    {
        return;
    }

    const VGarmentFabrics before = m_doc->getFabrics();
    const QStringList taken = takenFabricNames(before, QString());
    const QString name = fabric.name;
    for (int number = 2; taken.contains(fabric.name, Qt::CaseInsensitive); ++number)
    {
        fabric.name = QStringLiteral("%1 %2").arg(name).arg(number);
    }
    VGarmentFabrics after = before;
    after.custom.append(fabric);
    after = cutFrom(after, fabric.name);
    qApp->getUndoStack()->push(new SaveFabrics(tr("take fabric from library"), before, after, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's own fabric shown goes into the fabric library, for other patterns to take: over the library's fabric
// of its name, if asked to when that one differs.
void GarmentViewWidget::addFabricToLibrary()
{
    const VGarmentFabrics fabrics = m_doc->getFabrics();
    const quint32 selected = m_scene_model->selectedPiece();
    const VCustomFabric fabric = fabrics.customFabric(selected != 0 ? fabrics.of(selected) : fabrics.garment);
    if (fabric.name.isEmpty())
    {
        return;
    }

    const FabricLibrary::Entry kept = m_fabric_library->entry(fabric.name);
    if (!kept.path.isEmpty())
    {
        if (kept.fabric == fabric)
        {
            return;
        }
        const QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Fabric Library"),
                                  tr("The fabric library already has a fabric called %1. Replace it with this one?")
                                      .arg(kept.fabric.name));
        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    QString error;
    if (!m_fabric_library->add(fabric, &error))
    {
        QMessageBox::warning(this, tr("Fabric Library"),
                             tr("The fabric couldn't be added to the fabric library. %1").arg(error));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The library's folder is opened, to take fabric files out of it, or to put in ones others made; it is made if it
// isn't there yet.
void GarmentViewWidget::openFabricLibrary()
{
    QDir().mkpath(m_fabric_library->folder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_fabric_library->folder()));
}

//---------------------------------------------------------------------------------------------------------------------
// The names a fabric of the pattern's own can't be given: the 3D View's fabrics', as patterns store them and as they
// are shown, and those of the pattern's own but the one given.
QStringList GarmentViewWidget::takenFabricNames(const VGarmentFabrics& fabrics, const QString& except) const
{
    QStringList taken;
    for (const Fabric& fabric : Fabric::presets())
    {
        taken << fabric.name << fabricTitle(fabric.name);
    }
    for (const VCustomFabric& fabric : fabrics.custom)
    {
        if (fabric.name != except)
        {
            taken << fabric.name;
        }
    }
    return taken;
}

//---------------------------------------------------------------------------------------------------------------------
// The fabrics with the selected piece, or the whole garment when no piece is selected, cut from that fabric. A piece
// cut from the garment's fabric doesn't keep a fabric of its own, but keeps an image of its own and its shrinkage.
VGarmentFabrics GarmentViewWidget::cutFrom(VGarmentFabrics fabrics, const QString& fabric) const
{
    const quint32 selected = m_scene_model->selectedPiece();
    if (selected == 0)
    {
        fabrics.garment = fabric == Fabric::defaultName() ? QString() : fabric;
        return fabrics;
    }

    const QString garment = fabrics.garment.isEmpty() ? Fabric::defaultName() : fabrics.garment;
    const QString own_fabric = fabric != garment ? fabric : QString();
    auto own = std::find_if(fabrics.pieces.begin(), fabrics.pieces.end(), [selected](const VPieceFabric& piece)
    {
        return piece.piece_id == selected;
    });
    if (own == fabrics.pieces.end())
    {
        if (!own_fabric.isEmpty())
        {
            fabrics.pieces.append({selected, own_fabric, VFabricTexture()});
        }
    }
    else if (own_fabric.isEmpty() && own->texture.isNull() && own->shrinkage.isNull())
    {
        fabrics.pieces.erase(own);
    }
    else
    {
        own->fabric = own_fabric;
    }
    return fabrics;
}

//---------------------------------------------------------------------------------------------------------------------
// An image of the fabric was asked for: for the selected piece, or for the whole garment when no piece is selected.
// It is kept in the pattern, and repeats as often across the grain as the cloth it shows is said to be wide.
void GarmentViewWidget::chooseFabricImage()
{
    const QString pattern = qApp->getFilePath();
    const QString folder = pattern.isEmpty() ? QDir::homePath() : QFileInfo(pattern).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Fabric Image"), folder,
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp)"), nullptr,
                                                      qApp->Settings()->getUseNativeFileDialogs());
    if (path.isEmpty())
    {
        return;
    }

    VFabricTexture texture = readFabricImage(path);
    if (texture.isNull())
    {
        QMessageBox::warning(this, tr("Fabric Image"),
                             tr("%1 could not be read as an image.").arg(QDir::toNativeSeparators(path)));
        return;
    }

    const VFabricTexture own = ownFabricImage();
    bool chosen = false;
    texture.width = QInputDialog::getDouble(this, tr("Fabric Image"), tr("How wide the cloth in the image is, in cm:"),
                                            own.isNull() ? default_image_width : own.width, narrowest_image_width,
                                            widest_image_width, 1, &chosen);
    if (chosen)
    {
        saveFabricImage(texture, tr("change fabric image"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// How wide the cloth in the image of the fabric is was asked for: the selected piece's image, or the garment's.
void GarmentViewWidget::changeFabricImageWidth()
{
    VFabricTexture texture = ownFabricImage();
    if (texture.isNull())
    {
        return;
    }

    bool chosen = false;
    const qreal width = QInputDialog::getDouble(this, tr("Fabric Image"),
                                                tr("How wide the cloth in the image is, in cm:"), texture.width,
                                                narrowest_image_width, widest_image_width, 1, &chosen);
    if (chosen)
    {
        texture.width = width;
        saveFabricImage(texture, tr("change fabric image"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::removeFabricImage()
{
    saveFabricImage(VFabricTexture(), tr("remove fabric image"));
}

//---------------------------------------------------------------------------------------------------------------------
// How much the fabric shrinks was asked for, as CLO's shrinkage: the selected piece's, or the whole garment's when no
// piece is selected, across the grain and along it, as a percentage of the drafted size. A piece shrinking as it
// would anyway, as the garment's fabric does or not at all, keeps no shrinkage of its own.
void GarmentViewWidget::chooseShrinkage()
{
    const VGarmentFabrics before = m_doc->getFabrics();
    const quint32 selected = m_scene_model->selectedPiece();
    VFabricShrinkage inherited;
    if (selected != 0)
    {
        inherited = before.of(selected) == before.garment ? before.shrinkage : VFabricShrinkage();
    }
    const VFabricShrinkage none{1.0, 1.0};
    auto shown = [&none](const VFabricShrinkage& shrinkage)
    {
        return shrinkage.isNull() ? none : shrinkage;
    };
    const VFabricShrinkage now = shown(selected != 0 ? before.shrinkageOf(selected) : before.shrinkage);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Shrinkage"));
    auto* layout = new QFormLayout(&dialog);
    auto* note = new QLabel(selected != 0
                            ? tr("How much the fabric of the selected piece shrinks, as a share of its drafted size: "
                                 "less than 100% shrinks it, as a rib knit worn snug does, more stretches it out.")
                            : tr("How much the fabric of the whole garment shrinks, as a share of its drafted size: "
                                 "less than 100% shrinks it, as a rib knit worn snug does, more stretches it out."),
                            &dialog);
    note->setWordWrap(true);
    layout->addRow(note);
    auto percent_box = [&dialog](const QString& name, qreal share)
    {
        auto* box = new QDoubleSpinBox(&dialog);
        box->setObjectName(name);
        box->setRange(least_shrinkage * 100.0, most_shrinkage * 100.0);
        box->setDecimals(0);
        box->setSuffix(QStringLiteral("%"));
        box->setValue(share * 100.0);
        return box;
    };
    QDoubleSpinBox* weft = percent_box(QStringLiteral("shrinkageWeft"), now.weft);
    QDoubleSpinBox* warp = percent_box(QStringLiteral("shrinkageWarp"), now.warp);
    layout->addRow(tr("Across the grain:"), weft);
    layout->addRow(tr("Along the grain:"), warp);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    VFabricShrinkage chosen{weft->value() / 100.0, warp->value() / 100.0};
    if (chosen == shown(inherited))
    {
        chosen = VFabricShrinkage();
    }
    VGarmentFabrics after = before;
    if (selected == 0)
    {
        after.shrinkage = chosen;
    }
    else
    {
        auto own = std::find_if(after.pieces.begin(), after.pieces.end(), [selected](const VPieceFabric& piece)
        {
            return piece.piece_id == selected;
        });
        if (own == after.pieces.end())
        {
            if (!chosen.isNull())
            {
                after.pieces.append({selected, QString(), VFabricTexture(), chosen});
            }
        }
        else if (own->fabric.isEmpty() && own->texture.isNull() && chosen.isNull())
        {
            after.pieces.erase(own);
        }
        else
        {
            own->shrinkage = chosen;
        }
    }

    if (!(after == before))
    {
        qApp->getUndoStack()->push(new SaveFabrics(tr("change shrinkage"), before, after, m_doc));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The image of the fabric the selected piece has of its own, or with no piece selected the garment's; null for none.
VFabricTexture GarmentViewWidget::ownFabricImage() const
{
    const VGarmentFabrics fabrics = m_doc->getFabrics();
    const quint32 selected = m_scene_model->selectedPiece();
    if (selected == 0)
    {
        return fabrics.texture;
    }
    for (const VPieceFabric& piece : fabrics.pieces)
    {
        if (piece.piece_id == selected)
        {
            return piece.texture;
        }
    }
    return VFabricTexture();
}

//---------------------------------------------------------------------------------------------------------------------
// Gives the selected piece an image of its fabric of its own, or with no piece selected the garment; a null image
// takes it away. A piece left with neither a fabric nor an image of its own is cut from the garment's.
void GarmentViewWidget::saveFabricImage(const VFabricTexture& texture, const QString& text)
{
    const VGarmentFabrics before = m_doc->getFabrics();
    VGarmentFabrics after = before;
    const quint32 selected = m_scene_model->selectedPiece();
    if (selected == 0)
    {
        after.texture = texture;
    }
    else
    {
        auto own = std::find_if(after.pieces.begin(), after.pieces.end(), [selected](const VPieceFabric& piece)
        {
            return piece.piece_id == selected;
        });
        if (own == after.pieces.end())
        {
            if (!texture.isNull())
            {
                after.pieces.append({selected, QString(), texture});
            }
        }
        else if (own->fabric.isEmpty() && texture.isNull() && own->shrinkage.isNull())
        {
            after.pieces.erase(own);
        }
        else
        {
            own->texture = texture;
        }
    }

    if (!(after == before))
    {
        qApp->getUndoStack()->push(new SaveFabrics(text, before, after, m_doc));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Which way the piece's grain runs, in degrees anticlockwise from the piece scene's x axis: along its grainline, from
// the grainline's bottom anchor point to its top one if it has them; straight up and down if the grainline's rotation
// can't be worked out.
qreal GarmentViewWidget::grainAngle(const VPiece& piece) const
{
    const VGrainlineData& grainline = piece.GetGrainlineGeometry();
    qreal angle = 90.0;
    try
    {
        if (grainline.topAnchorPoint() != NULL_ID && grainline.bottomAnchorPoint() != NULL_ID)
        {
            const QPointF top(*m_data->GeometricObject<VPointF>(grainline.topAnchorPoint()));
            const QPointF bottom(*m_data->GeometricObject<VPointF>(grainline.bottomAnchorPoint()));
            angle = QLineF(bottom, top).angle();
        }
        else
        {
            Calculator calculator;
            angle = calculator.EvalFormula(m_data->DataVariables(), grainline.getRotation());
        }
    }
    catch (const VExceptionBadId&)
    {
        angle = 90.0;
    }
    catch (const qmu::QmuParserError&)
    {
        angle = 90.0;
    }
    return angle;
}

//---------------------------------------------------------------------------------------------------------------------
// Colors the cloth by the fit map chosen in the Fit Map menu, or by the pieces' own colors again.
void GarmentViewWidget::showFitMap(bool shown)
{
    const QAction* chosen = m_fit_maps->checkedAction();
    const auto map = static_cast<GarmentSceneModel::FitMap>(chosen != nullptr ? chosen->data().toInt() : 0);
    m_scene_model->setFitMap(shown ? map : GarmentSceneModel::FitMap::None);
}

//---------------------------------------------------------------------------------------------------------------------
// A fit map was chosen in the menu: it shows right away.
void GarmentViewWidget::chooseFitMap()
{
    m_fit_action->setChecked(true);
    showFitMap(true);
}

//---------------------------------------------------------------------------------------------------------------------
// Meshes the pieces finer for a final drape, or coarse again for editing. Every piece is meshed again, and a drape
// carries over onto the new meshes and goes on from there.
void GarmentViewWidget::setFine(bool fine)
{
    m_mesher.setEdgeLength(fine ? fine_edge_length : PieceMesher::defaultEdgeLength());
    rebuildScene();
}

//---------------------------------------------------------------------------------------------------------------------
// Drapes on the graphics card or on the processor; a drape going on goes on on the other.
void GarmentViewWidget::setOnDevice(bool on_device)
{
    m_runner->setOnDevice(on_device);
    if (m_runner->isRunning())
    {
        startSimulation();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Says in the switch's tip what the drape is worked out on: the graphics card named, or the processor.
void GarmentViewWidget::showComputing(int generation, const QString& device)
{
    if (generation == m_runner->generation())
    {
        if (!device.isEmpty())
        {
            m_device_action->setToolTip(tr("Working out the drape on the graphics card: %1").arg(device));
        }
        else if (m_device_action->isChecked())
        {
            m_device_action->setToolTip(tr("No graphics card here can work out the drape, so the processor does"));
        }
        else
        {
            m_device_action->setToolTip(tr("Working out the drape on the processor"));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While topstitching, a click near a piece's edge stitches it or takes its stitches out. Topstitching, sewing and
// arranging take turns.
void GarmentViewWidget::setStitching(bool stitching)
{
    if (stitching)
    {
        m_sew_action->setChecked(false);
        m_arrange_action->setChecked(false);
        m_fold_action->setChecked(false);
        m_elastic_action->setChecked(false);
    }
    m_stitch_editor->setStitching(stitching);
}

//---------------------------------------------------------------------------------------------------------------------
// While folding, the pieces' internal paths show, and a click near one folds the piece along it or unfolds it. Folding,
// sewing, topstitching and arranging take turns.
void GarmentViewWidget::setFolding(bool folding)
{
    if (folding)
    {
        m_sew_action->setChecked(false);
        m_arrange_action->setChecked(false);
        m_topstitch_action->setChecked(false);
        m_elastic_action->setChecked(false);
    }
    m_fold_editor->setFolding(folding);
}

//---------------------------------------------------------------------------------------------------------------------
// An angle was chosen in the Fold menu, the one paths clicked are folded at: one of those offered, or any other.
void GarmentViewWidget::chooseFoldAngle(QAction* action)
{
    if (action == m_other_angle_action)
    {
        bool chosen = false;
        const qreal angle = QInputDialog::getDouble(this, tr("Fold Angle"),
                                                    tr("Angle the cloth makes across a fold on its right side, in "
                                                       "degrees: 180 lies flat, less folds the right side in and more "
                                                       "the wrong side; 0 and 360 fold it onto itself."),
                                                    m_fold_editor->angle(), 0.0, 360.0, 1, &chosen);
        if (chosen)
        {
            m_fold_editor->setAngle(angle);
        }
    }
    else
    {
        m_fold_editor->setAngle(action->data().toDouble());
    }
    m_fold_action->setChecked(true);
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's folds changed, by folding here or by undo and redo. Pieces on the avatar fold, or unfold, right away:
// the drape goes on, or starts if it was off.
void GarmentViewWidget::updateFolds()
{
    const QVector<VFold> before = m_fold_editor->folds();
    const QVector<VFold> after = m_doc->getFolds();
    m_fold_editor->setFolds(after);

    QSet<quint32> changed;
    for (const QVector<VFold>* folds : {&before, &after})
    {
        const QVector<VFold>& others = folds == &before ? after : before;
        for (const VFold& fold : *folds)
        {
            if (!others.contains(fold))
            {
                changed.insert(fold.piece_id);
            }
        }
    }
    // A piece folded differently starts out from where it is arranged, folded over as it is folded now.
    bool placed = false;
    for (const quint32 piece : changed)
    {
        if (!m_wrap.isNull() && m_arrangements.contains(piece))
        {
            placed = true;
            m_draped.remove(piece);
            m_draped.remove(PieceOutline::mirrorId(piece));
            showPlaced(piece);
        }
    }
    if (placed && m_runner->isRunning())
    {
        startSimulation();
        updateActions();
    }
    else if (placed && m_simulate_action->isEnabled())
    {
        m_simulate_action->setChecked(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While sewing elastic, the elastics and the pieces' internal paths show, and a click near an edge or a path sews
// elastic along it or takes it out. Sewing elastic, folding, sewing, topstitching and arranging take turns.
void GarmentViewWidget::setElasticEditing(bool editing)
{
    if (editing)
    {
        m_sew_action->setChecked(false);
        m_arrange_action->setChecked(false);
        m_topstitch_action->setChecked(false);
        m_fold_action->setChecked(false);
    }
    m_elastic_editor->setEditing(editing);
}

//---------------------------------------------------------------------------------------------------------------------
// A length was chosen in the Elastic menu, the share of their lines' length elastics clicked get: one of those
// offered, or any other.
void GarmentViewWidget::chooseElasticRatio(QAction* action)
{
    if (action == m_other_ratio_action)
    {
        bool chosen = false;
        const double percent = QInputDialog::getDouble(this, tr("Elastic Length"),
                                                       tr("How long the elastic is, as a percentage of the length of "
                                                          "the edge or line it is sewn along: the cloth gathers to "
                                                          "that."),
                                                       m_elastic_editor->ratio() * 100.0, 10.0, 100.0, 0, &chosen);
        if (chosen)
        {
            m_elastic_editor->setRatio(percent / 100.0);
        }
    }
    else
    {
        m_elastic_editor->setRatio(action->data().toDouble());
    }
    m_elastic_action->setChecked(true);
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's elastics changed, by sewing elastic here or by undo and redo. Pieces on the avatar gather, or let go,
// right away: the drape goes on with them, or starts if it was off.
void GarmentViewWidget::updateElastics()
{
    const QVector<VElastic> before = m_elastic_editor->elastics();
    const QVector<VElastic> after = m_doc->getElastics();
    m_elastic_editor->setElastics(after);

    bool placed = false;
    for (const QVector<VElastic>* elastics : {&before, &after})
    {
        const QVector<VElastic>& others = elastics == &before ? after : before;
        for (const VElastic& elastic : *elastics)
        {
            placed = placed || (!others.contains(elastic) && m_arrangements.contains(elastic.piece_id));
        }
    }
    if (placed && m_runner->isRunning())
    {
        startSimulation();
        updateActions();
    }
    else if (placed && m_simulate_action->isEnabled())
    {
        m_simulate_action->setChecked(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The lines the editors draw on the pieces: the folds and internal paths while folding, the elastics while sewing them.
void GarmentViewWidget::showLines()
{
    QHash<quint32, QVector<DrawnLine>> lines = m_fold_editor->lines();
    const QHash<quint32, QVector<DrawnLine>> elastic_lines = m_elastic_editor->lines();
    for (auto piece = elastic_lines.constBegin(); piece != elastic_lines.constEnd(); ++piece)
    {
        lines[piece.key()] += piece.value();
    }
    m_scene_model->setLines(lines);
}

//---------------------------------------------------------------------------------------------------------------------
// Topstitches the whole garment along every edge but folds, or none but those stitched one by one; edges clicked to
// differ from the rest keep doing so.
void GarmentViewWidget::stitchEveryEdge(bool every)
{
    VTopstitches topstitches = m_doc->getTopstitches();
    if (topstitches.all != every)
    {
        topstitches.all = every;
        saveTopstitches(topstitches, every ? tr("topstitch every edge") : tr("stop topstitching every edge"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A style was chosen in the Topstitch menu: edges clicked from now on are stitched in it, and so is the garment where
// it is stitched all over; edges stitched one by one keep their style.
void GarmentViewWidget::chooseStitchStyle(QAction* action)
{
    VTopstitches topstitches = m_doc->getTopstitches();
    const QString chosen = action->data().toString();
    const QString style = chosen == TopstitchStyle::defaultName() ? QString() : chosen;
    if (style != topstitches.style)
    {
        topstitches.style = style;
        saveTopstitches(topstitches, tr("change topstitch style"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A thread was chosen in the Topstitch menu: one matching the cloth, one of the colors offered, or any other.
void GarmentViewWidget::chooseThread(QAction* action)
{
    VTopstitches topstitches = m_doc->getTopstitches();
    QString color = action->data().toString();
    if (action == m_other_thread_action)
    {
        const QColor current(topstitches.color);
        const QColor picked = QColorDialog::getColor(current.isValid() ? current : QColor(Qt::white), this,
                                                     tr("Thread Color"),
                                                     qApp->Settings()->getUseNativeColorDialogs());
        color = picked.isValid() ? picked.name() : topstitches.color;
    }
    if (color != topstitches.color)
    {
        topstitches.color = color;
        saveTopstitches(topstitches, tr("change thread color"));
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The topstitching was changed, here or by undo.
void GarmentViewWidget::updateTopstitches()
{
    const VTopstitches topstitches = m_doc->getTopstitches();
    m_stitch_editor->setTopstitches(topstitches);
    m_scene_model->setThreadColor(QColor(topstitches.color));
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showStitches()
{
    m_scene_model->setStitches(m_stitch_editor->stitches());
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showStitchPreview()
{
    m_scene_model->setStitchPreview(m_stitch_editor->preview());
}

//---------------------------------------------------------------------------------------------------------------------
// The seams on the avatar follow the pieces there, as they are arranged or draped.
void GarmentViewWidget::showSeamsOnAvatar()
{
    QHash<quint32, QVector<QVector3D>> positions;
    for (const GarmentSceneModel::Piece& piece : m_scene_model->placedPieces())
    {
        positions.insert(piece.id, piece.positions);
    }
    m_seam_editor->setPositions(positions);
}

//---------------------------------------------------------------------------------------------------------------------
// The piece's internal paths drawn dashed or dotted, which stand for stitching drawn on the pattern, in cm at the
// piece's place in the piece scene, as its outline is. Paths that reach to the cutting line stop at the seam line.
QVector<QVector<QPointF>> GarmentViewWidget::stitchedPaths(const VPiece& piece) const
{
    QVector<QVector<QPointF>> paths;
    try
    {
        const QVector<QPointF> seam_line = piece.mainPathPoints(m_data);
        for (const quint32 path_id : piece.getInternalPaths())
        {
            const VPiecePath path = m_data->getPiecePath(path_id);
            const Qt::PenStyle style = path.getLineType();
            if (path.getType() != PiecePathType::InternalPath || path.isCutPath() || style == Qt::SolidLine
                || style == Qt::NoPen)
            {
                continue;
            }

            QVector<QPointF> points;
            for (const QPointF& point : path.PathPoints(m_data, seam_line))
            {
                points.append(QPointF(FromPixel(point.x() + piece.GetMx(), Unit::Cm),
                                      FromPixel(point.y() + piece.GetMy(), Unit::Cm)));
            }
            if (points.size() > 1)
            {
                paths.append(points);
            }
        }
    }
    catch (const VException&)
    {
        // The piece scene already shows what is wrong with a path whose points can't be found.
    }
    return paths;
}

//---------------------------------------------------------------------------------------------------------------------
// Saves the pieces on the avatar as the scene shows them, draped or as they were arranged, and the avatar, in a file
// other 3D programs open: binary glTF, or OBJ with its materials in an MTL file next to it.
void GarmentViewWidget::exportDrape()
{
    const QString gltf_filter = tr("glTF binary (*.glb)");
    const QString obj_filter = tr("Wavefront OBJ (*.obj)");
    const QFileInfo pattern(qApp->getFilePath());
    const QString folder = qApp->getFilePath().isEmpty() ? QDir::homePath() : pattern.absolutePath();
    const QString name = qApp->getFilePath().isEmpty() ? tr("drape") : pattern.completeBaseName();

    QString filter = gltf_filter;
    QString path = QFileDialog::getSaveFileName(this, tr("Export Drape"),
                                                QDir(folder).filePath(name + QStringLiteral(".glb")),
                                                gltf_filter + QStringLiteral(";;") + obj_filter, &filter,
                                                qApp->Settings()->getUseNativeFileDialogs());
    if (path.isEmpty())
    {
        return;
    }

    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QLatin1String("glb") && suffix != QLatin1String("obj"))
    {
        suffix = filter == obj_filter ? QStringLiteral("obj") : QStringLiteral("glb");
        path += QLatin1Char('.') + suffix;
    }

    QString error;
    const QVector<ExportMesh> meshes = exportMeshes();
    const bool written = suffix == QLatin1String("obj") ? GarmentExport::writeObj(path, meshes, &error)
                                                        : GarmentExport::writeGlb(path, meshes, &error);
    if (!written)
    {
        QMessageBox::warning(this, tr("Export Drape"), tr("The drape could not be saved as %1.\n%2")
                                                           .arg(QDir::toNativeSeparators(path), error));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The 3D view as it is now, saved as an image, without the hints over it.
void GarmentViewWidget::saveSnapshot()
{
    if (m_quick_view == nullptr || m_quick_view->rootObject() == nullptr)
    {
        return;
    }

    const QString png_filter = tr("PNG image (*.png)");
    const QString jpg_filter = tr("JPEG image (*.jpg)");
    const QFileInfo pattern(qApp->getFilePath());
    const QString folder = qApp->getFilePath().isEmpty() ? QDir::homePath() : pattern.absolutePath();
    const QString name = qApp->getFilePath().isEmpty() ? tr("garment") : pattern.completeBaseName();

    QString filter = png_filter;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Snapshot"),
                                                QDir(folder).filePath(name + QStringLiteral(".png")),
                                                png_filter + QStringLiteral(";;") + jpg_filter, &filter,
                                                qApp->Settings()->getUseNativeFileDialogs());
    if (path.isEmpty())
    {
        return;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QLatin1String("png") && suffix != QLatin1String("jpg") && suffix != QLatin1String("jpeg"))
    {
        path += filter == jpg_filter ? QStringLiteral(".jpg") : QStringLiteral(".png");
    }

    QQuickItem* scene = m_quick_view->rootObject();
    scene->setProperty("capturing", true);
    const QImage image = m_quick_view->grabWindow();
    scene->setProperty("capturing", false);
    if (image.isNull() || !image.save(path))
    {
        QMessageBox::warning(this, tr("Save Snapshot"),
                             tr("The snapshot could not be saved as %1.").arg(QDir::toNativeSeparators(path)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth of a piece on the avatar was taken hold of while draping, at a point in scene coordinates: it is held
// there, then pulled after the mouse.
void GarmentViewWidget::pullCloth(quint32 piece, const QVector3D& point)
{
    const QVariant rest = m_scene_model->restPoint(static_cast<int>(piece), point);
    if (rest.isValid())
    {
        m_pull = ClothPull();
        m_pull.active = true;
        m_pull.piece = piece;
        m_pull.rest = rest.toPointF();
        m_pull.target = point;
        m_runner->setPulling(true);
        updateHolds();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A pin was taken hold of while draping: it follows the mouse, the cloth with it.
void GarmentViewWidget::pullPin(int index)
{
    if (index >= 0 && index < m_pins.size())
    {
        m_pull = ClothPull();
        m_pull.active = true;
        m_pull.pin = index;
        m_runner->setPulling(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The mouse holding the cloth or a pin moved to a point in scene coordinates.
void GarmentViewWidget::movePull(const QVector3D& point)
{
    if (m_pull.active)
    {
        if (m_pull.pin >= 0 && m_pull.pin < m_pins.size())
        {
            m_pins[m_pull.pin].position = point;
        }
        else
        {
            m_pull.target = point;
        }
        updateHolds();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth or the pin held is let go: the cloth falls back, a pin stays where it was put.
void GarmentViewWidget::releasePull()
{
    m_pull = ClothPull();
    updateHolds();
    m_runner->setPulling(false);
}

//---------------------------------------------------------------------------------------------------------------------
// A piece on the avatar was clicked to pin it, at a point in scene coordinates: the cloth there is held where it is.
void GarmentViewWidget::pinCloth(quint32 piece, const QVector3D& point)
{
    const QVariant rest = m_scene_model->restPoint(static_cast<int>(piece), point);
    if (rest.isValid())
    {
        m_pins.append({piece, rest.toPointF(), point});
        updateHolds();
        updateActions();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::unpin(int index)
{
    if (index >= 0 && index < m_pins.size())
    {
        if (m_pull.pin >= 0)
        {
            m_pull = ClothPull();
            m_runner->setPulling(false);
        }
        m_pins.remove(index);
        updateHolds();
        updateActions();
        keepDrape();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::removeAllPins()
{
    m_pins.clear();
    if (m_pull.pin >= 0)
    {
        m_pull = ClothPull();
        m_runner->setPulling(false);
    }
    updateHolds();
    updateActions();
    keepDrape();
}

//---------------------------------------------------------------------------------------------------------------------
// The vertex of the drape being simulated nearest to a place in a flat piece on the avatar; -1 if the piece isn't
// draping.
int GarmentViewWidget::drapeVertex(quint32 piece, const QPointF& rest) const
{
    for (const DrapePiece& drape_piece : m_drape_pieces)
    {
        if (drape_piece.id != piece)
        {
            continue;
        }
        for (const GarmentPiece& garment_piece : m_garment_pieces)
        {
            if (garment_piece.id == piece && garment_piece.mesh.vertexCount() == drape_piece.count)
            {
                int nearest = -1;
                qreal shortest = std::numeric_limits<qreal>::infinity();
                for (int i = 0; i < garment_piece.mesh.vertexCount(); ++i)
                {
                    const qreal distance = QLineF(garment_piece.mesh.rest_positions.at(i), rest).length();
                    if (distance < shortest)
                    {
                        shortest = distance;
                        nearest = i;
                    }
                }
                return nearest < 0 ? -1 : drape_piece.offset + nearest;
            }
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
// Tells the drape where the cloth is held, by the pins and by the mouse, and the scene where the pins are.
void GarmentViewWidget::updateHolds()
{
    QHash<int, QVector3D> holds;
    QVector<QVector3D> pins;
    for (const ClothPin& pin : m_pins)
    {
        const int vertex = drapeVertex(pin.piece, pin.rest);
        if (vertex >= 0)
        {
            holds.insert(vertex, pin.position);
        }
        pins.append(pin.position);
    }
    if (m_pull.active && m_pull.pin < 0)
    {
        const int vertex = drapeVertex(m_pull.piece, m_pull.rest);
        if (vertex >= 0)
        {
            holds.insert(vertex, m_pull.target);
        }
    }
    m_runner->setHolds(holds);
    m_scene_model->setPins(pins);
}

//---------------------------------------------------------------------------------------------------------------------
// Hides the selected piece, and its copy, in the scene; they still drape.
void GarmentViewWidget::hideSelectedPiece()
{
    const quint32 selected = m_scene_model->selectedPiece();
    if (selected != 0)
    {
        QSet<quint32> hidden = m_scene_model->hiddenPieces();
        hidden.insert(selected);
        m_scene_model->setHiddenPieces(hidden);
        updateActions();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showAllPieces()
{
    m_scene_model->setHiddenPieces(QSet<quint32>());
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The garment to export: each piece on the avatar, and the copy of a piece cut twice, where the scene shows it, with
// its color and its flat shape, and the image of its fabric laid on it as the scene lays it; and the avatar, in the
// grey the scene draws it in.
QVector<ExportMesh> GarmentViewWidget::exportMeshes() const
{
    QVector<ExportMesh> meshes;
    for (const GarmentSceneModel::Piece& piece : m_scene_model->placedPieces())
    {
        ExportMesh mesh;
        mesh.name = piece.name;
        mesh.color = piece.color;
        mesh.positions = piece.positions;
        mesh.flat = piece.mesh.rest_positions;
        mesh.indices = piece.mesh.indices;

        QByteArray image = piece.texture;
        QBuffer image_buffer(&image);
        image_buffer.open(QIODevice::ReadOnly);
        const QSize pixels = QImageReader(&image_buffer).size();
        if (piece.texture_width > 0 && pixels.width() > 0 && pixels.height() > 0)
        {
            const qreal height = piece.texture_width * pixels.height() / pixels.width();
            mesh.image = piece.texture;
            for (const QPointF& in_fabric : PieceGeometry::grainPositions(piece.mesh, piece.grain_angle))
            {
                mesh.image_uv.append(QPointF(in_fabric.x() / piece.texture_width, -in_fabric.y() / height));
            }
        }
        meshes.append(mesh);

        if (!piece.stitches.isEmpty())
        {
            const ThreadMesh thread = Topstitching::threadMesh(
                piece.stitches, piece.positions, PieceGeometry::vertexNormals(piece.mesh, piece.positions));
            ExportMesh stitches;
            stitches.name = tr("%1 topstitching").arg(piece.name);
            stitches.color = m_scene_model->threadColor(m_scene_model->clothColor(piece.id));
            stitches.positions = thread.positions;
            stitches.indices = thread.indices;
            meshes.append(stitches);
        }
    }
    if (!m_collider.isEmpty())
    {
        ExportMesh avatar;
        avatar.name = tr("Avatar");
        avatar.color = QColor(avatar_color);
        avatar.positions = m_collider.positions();
        avatar.indices = m_collider.triangles();
        meshes.append(avatar);
    }
    return meshes;
}

//---------------------------------------------------------------------------------------------------------------------
QString GarmentViewWidget::fabricTitle(const QString& fabric) const
{
    const QHash<QString, QString> titles = {{QStringLiteral("cottonShirting"), tr("Cotton shirting")},
                                            {QStringLiteral("cottonJersey"), tr("Cotton jersey")},
                                            {QStringLiteral("denim"), tr("Denim")},
                                            {QStringLiteral("woolSuiting"), tr("Wool suiting")},
                                            {QStringLiteral("chiffon"), tr("Chiffon")}};
    return titles.value(fabric, fabric);
}

//---------------------------------------------------------------------------------------------------------------------
// The fabric box lists the 3D View's fabrics, the pattern's own, and the fabric library's but those the pattern has,
// each under a heading; then making a new one, editing the one shown, adding it to the library and opening the
// library's folder. It is filled again only when the pattern's own or the library's change.
void GarmentViewWidget::updateFabricBox(const VGarmentFabrics& fabrics)
{
    m_fabric_library->update();
    QStringList custom;
    for (const VCustomFabric& fabric : fabrics.custom)
    {
        custom.append(fabric.name);
    }
    QVector<FabricLibrary::Entry> library;
    QStringList library_listed;
    for (const FabricLibrary::Entry& entry : m_fabric_library->entries())
    {
        if (!custom.contains(entry.fabric.name, Qt::CaseInsensitive))
        {
            library.append(entry);
            library_listed << entry.path << entry.fabric.name;
        }
    }
    if (m_fabric_box->count() > 0 && custom == m_custom_fabrics_listed && library_listed == m_library_fabrics_listed)
    {
        return;
    }
    m_custom_fabrics_listed = custom;
    m_library_fabrics_listed = library_listed;

    const QSignalBlocker blocker(m_fabric_box);
    m_fabric_box->clear();
    auto* items = qobject_cast<QStandardItemModel*>(m_fabric_box->model());
    auto add_heading = [this, items](const QString& heading)
    {
        m_fabric_box->insertSeparator(m_fabric_box->count());
        m_fabric_box->addItem(heading);
        if (QStandardItem* item = items != nullptr ? items->item(m_fabric_box->count() - 1) : nullptr)
        {
            item->setFlags(Qt::NoItemFlags);
        }
    };
    auto add_command = [this](const QString& text, int command, const QString& tip)
    {
        m_fabric_box->addItem(text);
        m_fabric_box->setItemData(m_fabric_box->count() - 1, command, fabric_command_role);
        m_fabric_box->setItemData(m_fabric_box->count() - 1, tip, Qt::ToolTipRole);
    };

    for (const Fabric& fabric : Fabric::presets())
    {
        m_fabric_box->addItem(fabricTitle(fabric.name), fabric.name);
    }
    if (!custom.isEmpty())
    {
        add_heading(tr("This Pattern"));
        for (const QString& name : custom)
        {
            m_fabric_box->addItem(name, name);
        }
    }
    if (!library.isEmpty())
    {
        add_heading(tr("Fabric Library"));
        for (const FabricLibrary::Entry& entry : library)
        {
            m_fabric_box->addItem(entry.fabric.name);
            m_fabric_box->setItemData(m_fabric_box->count() - 1, entry.path, fabric_file_role);
            m_fabric_box->setItemData(m_fabric_box->count() - 1, QDir::toNativeSeparators(entry.path),
                                      Qt::ToolTipRole);
        }
    }
    m_fabric_box->insertSeparator(m_fabric_box->count());
    add_command(tr("New Fabric..."), new_fabric,
                tr("A fabric of the pattern's own, starting from the one shown, as fabric tests describe it"));
    add_command(tr("Edit Fabric..."), edit_fabric, tr("Change or delete the fabric of the pattern's own shown"));
    add_command(tr("Add Fabric to Library"), add_to_library,
                tr("Keep the fabric of the pattern's own shown in the fabric library, for other patterns to take"));
    add_command(tr("Open Fabric Library"), open_library,
                tr("Open the fabric library's folder, set in the preferences: its fabric files can be shared, or "
                   "taken out"));
}

//---------------------------------------------------------------------------------------------------------------------
QString GarmentViewWidget::stitchStyleTitle(const TopstitchStyle& style) const
{
    const QHash<QString, QString> titles = {{QStringLiteral("single"), tr("Single")},
                                            {QStringLiteral("edge"), tr("Edge Stitch")},
                                            {QStringLiteral("double"), tr("Edge and Single")},
                                            {QStringLiteral("twinNeedle"), tr("Twin Needle")},
                                            {QStringLiteral("jeans"), tr("Jeans, Heavy Thread")}};
    return titles.value(style.name, style.name);
}

//---------------------------------------------------------------------------------------------------------------------
// What to do next while arranging or draping, for the scene to show.
void GarmentViewWidget::updateHint()
{
    QString hint;
    if (m_runner->isRunning())
    {
        hint = m_resting ? tr("At rest. Drag the cloth to pull it, Shift+click to pin it there or to take a pin out. "
                              "Simulate stops draping, Reset puts the pieces back where they were arranged.")
                         : tr("Draping. Drag the cloth to pull it, Shift+click to pin it there or to take a pin out. "
                              "Simulate stops draping, Reset puts the pieces back where they were arranged.");
    }
    else if (m_scene_model->isArranging())
    {
        hint = m_scene_model->selectedPiece() == 0
               ? tr("Click a piece, then a point or any spot on the avatar where it goes, or drag a placed piece "
                    "around. Esc stops arranging.")
               : m_scene_model->isPlaced(m_scene_model->selectedPiece())
                 ? tr("Click a point or any spot on the avatar to put the piece there. Drag its arrows to move it, its "
                      "rings to rotate, lean or swing it; right-click it to turn it over.")
                 : tr("Click a point or any spot on the avatar to put the piece there, or drag a placed piece "
                      "around.");
    }
    m_scene_model->setHint(hint);
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::createScene()
{
    // The scene has a window of its own, in a container: a Qt Quick widget loses its 3D content, or crashes, when it
    // moves to another top-level window, as it does when the dock is floated or docked again.
    m_quick_view = new QQuickView();
    m_quick_view->setResizeMode(QQuickView::SizeRootObjectToView);

    connect(m_quick_view, &QQuickView::statusChanged, this, [this](QQuickView::Status status)
    {
        if (status == QQuickView::Error)
        {
            QStringList errors;
            for (const QQmlError& error : m_quick_view->errors())
            {
                errors.append(error.toString());
            }
            showError(errors.join(QLatin1Char('\n')));
        }
    });
    connect(m_quick_view, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& message)
    {
        showError(message);
    });

    // White, the default piece color, needs a background a little darker than the window to stand out. The window
    // shows it too until the scene has loaded.
    const QPalette colors = palette();
    const QColor background = colors.color(QPalette::Window).darker(125);
    m_quick_view->setColor(background);
    QVariantMap properties;
    properties.insert(QStringLiteral("sceneModel"), QVariant::fromValue(m_scene_model));
    properties.insert(QStringLiteral("seamEditor"), QVariant::fromValue(m_seam_editor));
    properties.insert(QStringLiteral("stitchEditor"), QVariant::fromValue(m_stitch_editor));
    properties.insert(QStringLiteral("foldEditor"), QVariant::fromValue(m_fold_editor));
    m_fold_editor->setHighlightColor(colors.color(QPalette::Highlight));
    properties.insert(QStringLiteral("elasticEditor"), QVariant::fromValue(m_elastic_editor));
    m_elastic_editor->setHighlightColor(colors.color(QPalette::Highlight));
    properties.insert(QStringLiteral("emptyText"), tr("Pieces included in the layout show up here."));
    properties.insert(QStringLiteral("hintText"),
                      tr("Drag to turn, Ctrl+drag to move, scroll to zoom, double-click to fit"));
    properties.insert(QStringLiteral("backgroundColor"), background);
    properties.insert(QStringLiteral("textColor"), colors.color(QPalette::WindowText));
    properties.insert(QStringLiteral("highlightColor"), colors.color(QPalette::Highlight));
    m_quick_view->setInitialProperties(properties);
    m_quick_view->setSource(QUrl(QStringLiteral("qrc:/garment3d/garment_scene.qml")));
    m_quick_view->installEventFilter(this);

    // Added to the layout last, after m_quick_view is set: that can show this widget again, which would otherwise call
    // createScene() a second time.
    m_view_container = QWidget::createWindowContainer(m_quick_view);
    m_view_container->setFocusPolicy(Qt::StrongFocus);
    // The scene takes all the height the toolbar's rows leave.
    static_cast<QVBoxLayout*>(layout())->addWidget(m_view_container, 1);
}

//---------------------------------------------------------------------------------------------------------------------
// The tools come in groups: sewing and topstitching, arranging and draping, how the cloth looks, and export. The
// groups sit side by side as far as the dock is wide and wrap into rows where it isn't, so no tool is ever hidden
// behind a toolbar's overflow. Delete and Esc only act while the view has the focus, so they don't get in the way of
// the piece scene's.
void GarmentViewWidget::createToolBar()
{
    QWidget* tool_area = new QWidget(this);
    FlowLayout* tool_layout = new FlowLayout(tool_area, tool_group_spacing);
    tool_layout->setContentsMargins(0, 0, 0, 0);
    auto add_group = [tool_area, tool_layout]()
    {
        QToolBar* group = new QToolBar(tool_area);
        group->setToolButtonStyle(Qt::ToolButtonIconOnly);
        tool_layout->addWidget(group);
        return group;
    };
    QToolBar* tool_bar = add_group();

    m_sew_action = tool_bar->addAction(tr("Sew"));
    m_sew_action->setCheckable(true);
    m_sew_action->setToolTip(tr("Sew pieces together, on the board or on the avatar: click an edge near the end where "
                                "the seam starts, then the edge it is sewn to near the end that meets it"));
    connect(m_sew_action, &QAction::toggled, m_seam_editor, &SeamEditor::setSewing);

    // As CLO's sewing lines: the seams show on the avatar too, unless hidden for a clean look at the garment.
    QMenu* sew_menu = new QMenu(this);
    sew_menu->setToolTipsVisible(true);
    QAction* avatar_seams = sew_menu->addAction(tr("Seams on the Avatar"));
    avatar_seams->setCheckable(true);
    avatar_seams->setChecked(m_seam_editor->isGarmentSeamsShown());
    avatar_seams->setToolTip(tr("Show the seams on the pieces on the avatar, each in its color, with lines between the "
                                "places that meet while the pieces hang apart"));
    connect(avatar_seams, &QAction::toggled, m_seam_editor, &SeamEditor::setGarmentSeamsShown);
    QAction* seam_lengths = sew_menu->addAction(tr("Seam Lengths"));
    seam_lengths->setCheckable(true);
    seam_lengths->setToolTip(tr("Label each seam with how much longer one of its sides is than the other, which "
                                "has to be eased in; = where they are as long as each other"));
    connect(seam_lengths, &QAction::toggled, m_seam_editor, &SeamEditor::setLengthsShown);

    // The selected seam's type, as CLO's fold angle on sewing lines: the angle it holds the pieces at, or none.
    sew_menu->addSection(tr("Selected Seam"));
    m_seam_types = new QActionGroup(this);
    const QStringList seam_titles = {tr("Free"), tr("Flat"), tr("Turned"), tr("Right Sides Together")};
    const QStringList seam_tips = {tr("The pieces bend across the seam as the cloth does around it"),
                                   tr("The pieces lie flat across the seam, as at a seam pressed open: 180 degrees"),
                                   tr("The pieces fold onto each other at the seam, wrong sides together, as at the "
                                      "edge of a collar or of a facing: 360 degrees"),
                                   tr("The pieces fold onto each other at the seam, right sides together: 0 degrees")};
    for (int i = 0; i < seam_titles.size(); ++i)
    {
        QAction* action = sew_menu->addAction(seam_titles.at(i));
        action->setData(seam_angles[i]);
        action->setToolTip(seam_tips.at(i));
        m_seam_types->addAction(action);
    }
    m_other_seam_angle_action = sew_menu->addAction(tr("Other Angle..."));
    m_other_seam_angle_action->setData(180.0);
    m_other_seam_angle_action->setToolTip(tr("Hold the pieces at any angle across the seam, on the right side of the "
                                             "piece it is sewn from, from 0 to 360 degrees; 180 lies flat"));
    m_seam_types->addAction(m_other_seam_angle_action);
    for (QAction* action : m_seam_types->actions())
    {
        action->setCheckable(true);
        action->setEnabled(false);
    }
    connect(m_seam_types, &QActionGroup::triggered, this, &GarmentViewWidget::chooseSeamType);
    m_sew_action->setMenu(sew_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_sew_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_flip_action = tool_bar->addAction(tr("Flip"));
    m_flip_action->setToolTip(tr("Turn the selected seam around, when its lines cross"));
    connect(m_flip_action, &QAction::triggered, this, &GarmentViewWidget::flipSeam);

    m_remove_action = tool_bar->addAction(tr("Remove"));
    m_remove_action->setToolTip(tr("Take the selected seam out, or while arranging put the selected piece back on "
                                   "the board"));
    m_remove_action->setShortcut(QKeySequence::Delete);
    m_remove_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_remove_action, &QAction::triggered, this, &GarmentViewWidget::removeSelected);

    m_topstitch_action = tool_bar->addAction(tr("Topstitch"));
    m_topstitch_action->setCheckable(true);
    m_topstitch_action->setToolTip(tr("Topstitch the garment: click near an edge of a piece, on the board or on the "
                                      "avatar, to stitch along it, or to take its stitches out. Internal paths drawn "
                                      "dashed or dotted show as stitching too."));
    connect(m_topstitch_action, &QAction::toggled, this, &GarmentViewWidget::setStitching);

    QMenu* stitch_menu = new QMenu(this);
    stitch_menu->setToolTipsVisible(true);
    m_every_edge_action = stitch_menu->addAction(tr("Topstitch Every Edge"));
    m_every_edge_action->setCheckable(true);
    m_every_edge_action->setToolTip(tr("Topstitch along every edge of every piece but folds, except edges clicked to "
                                       "take their stitches out"));
    connect(m_every_edge_action, &QAction::triggered, this, &GarmentViewWidget::stitchEveryEdge);

    // The styles, as CLO's topstitch styles: the one chosen is the one edges clicked get.
    stitch_menu->addSection(tr("Style"));
    m_stitch_styles = new QActionGroup(this);
    for (const TopstitchStyle& style : TopstitchStyle::presets())
    {
        QStringList rows;
        for (const qreal distance : style.distances)
        {
            rows.append(QString::number(distance * 10.0));
        }
        QAction* action = stitch_menu->addAction(stitchStyleTitle(style));
        action->setCheckable(true);
        action->setData(style.name);
        action->setToolTip(tr("%n row(s) %1 mm inside the edge, in stitches %2 mm long", "", style.distances.size())
                               .arg(rows.join(QStringLiteral(", "))).arg(style.stitch_length * 10.0));
        m_stitch_styles->addAction(action);
    }
    connect(m_stitch_styles, &QActionGroup::triggered, this, &GarmentViewWidget::chooseStitchStyle);

    QMenu* thread_menu = stitch_menu->addMenu(tr("Thread Color"));
    m_threads = new QActionGroup(this);
    const QStringList thread_names = {tr("White"), tr("Black"), tr("Gold"), tr("Red")};
    QAction* matching = thread_menu->addAction(tr("Matching the Cloth"));
    matching->setData(QString());
    m_threads->addAction(matching);
    for (int i = 0; i < thread_names.size(); ++i)
    {
        QPixmap swatch(icon_size / 2, icon_size / 2);
        swatch.fill(QColor(thread_colors[i]));
        QAction* action = thread_menu->addAction(QIcon(swatch), thread_names.at(i));
        action->setData(QColor(thread_colors[i]).name());
        m_threads->addAction(action);
    }
    m_other_thread_action = thread_menu->addAction(tr("Other..."));
    m_threads->addAction(m_other_thread_action);
    for (QAction* action : m_threads->actions())
    {
        action->setCheckable(true);
    }
    connect(m_threads, &QActionGroup::triggered, this, &GarmentViewWidget::chooseThread);

    m_topstitch_action->setMenu(stitch_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_topstitch_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_fold_action = tool_bar->addAction(tr("Fold"));
    m_fold_action->setCheckable(true);
    m_fold_action->setToolTip(tr("Fold pieces along their internal paths, as hems, facings, collars and pleats fold: "
                                 "click near a path, on the board or on the avatar, to fold the piece along it at the "
                                 "angle chosen in the menu, or to unfold it"));
    connect(m_fold_action, &QAction::toggled, this, &GarmentViewWidget::setFolding);

    // The angles, on the cloth's right side, as CLO's fold angle: the one chosen is the one paths clicked get.
    QMenu* fold_menu = new QMenu(this);
    fold_menu->setToolTipsVisible(true);
    fold_menu->addSection(tr("Angle"));
    m_fold_angles = new QActionGroup(this);
    const QStringList angle_titles = {tr("Wrong Sides Together"), tr("Right Angle, Wrong Side In"),
                                      tr("Right Angle, Right Side In"), tr("Right Sides Together")};
    const QStringList angle_tips = {tr("Fold the cloth flat onto itself, wrong side in, as a hem or a facing turns in: "
                                       "360 degrees"),
                                    tr("Fold the cloth to a right angle, its wrong side inside: 270 degrees"),
                                    tr("Fold the cloth to a right angle, its right side inside: 90 degrees"),
                                    tr("Fold the cloth flat onto itself, right side in: 0 degrees")};
    for (int i = 0; i < angle_titles.size(); ++i)
    {
        QAction* action = fold_menu->addAction(angle_titles.at(i));
        action->setData(fold_angles[i]);
        action->setToolTip(angle_tips.at(i));
        m_fold_angles->addAction(action);
    }
    m_other_angle_action = fold_menu->addAction(tr("Other Angle..."));
    m_other_angle_action->setToolTip(tr("Fold at any angle the cloth makes across the fold on its right side, from 0 "
                                        "to 360 degrees; 180 lies flat"));
    m_fold_angles->addAction(m_other_angle_action);
    for (QAction* action : m_fold_angles->actions())
    {
        action->setCheckable(true);
    }
    connect(m_fold_angles, &QActionGroup::triggered, this, &GarmentViewWidget::chooseFoldAngle);
    m_fold_action->setMenu(fold_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_fold_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_elastic_action = tool_bar->addAction(tr("Elastic"));
    m_elastic_action->setCheckable(true);
    m_elastic_action->setToolTip(tr("Sew elastic along edges of pieces and lines inside them, as at an elastic waist "
                                    "or cuff, or for shirring: click near an edge or a line, on the board or on the "
                                    "avatar, to gather the cloth along it to the length chosen in the menu, or to take "
                                    "the elastic out"));
    connect(m_elastic_action, &QAction::toggled, this, &GarmentViewWidget::setElasticEditing);

    // The elastic's length, as CLO's elastic ratio: the one chosen is the one edges and lines clicked get.
    QMenu* elastic_menu = new QMenu(this);
    elastic_menu->setToolTipsVisible(true);
    elastic_menu->addSection(tr("Length"));
    m_elastic_ratios = new QActionGroup(this);
    for (const qreal ratio : elastic_ratios)
    {
        QAction* action = elastic_menu->addAction(tr("%1% of the Length").arg(qRound(ratio * 100.0)));
        action->setData(ratio);
        action->setToolTip(tr("Gather the cloth along the edge or line to %1% of its length")
                               .arg(qRound(ratio * 100.0)));
        m_elastic_ratios->addAction(action);
    }
    m_other_ratio_action = elastic_menu->addAction(tr("Other Length..."));
    m_other_ratio_action->setToolTip(tr("Gather the cloth to any share of the edge's or line's length, from 10 to 100 "
                                        "percent"));
    m_elastic_ratios->addAction(m_other_ratio_action);
    for (QAction* action : m_elastic_ratios->actions())
    {
        action->setCheckable(true);
    }
    connect(m_elastic_ratios, &QActionGroup::triggered, this, &GarmentViewWidget::chooseElasticRatio);
    m_elastic_action->setMenu(elastic_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_elastic_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    tool_bar = add_group();

    m_avatar_action = tool_bar->addAction(tr("Avatar"));
    m_avatar_action->setToolTip(tr("Choose who wears the garment when the pattern has no measurements: a woman or a "
                                   "man of a European size, as tall and as round as wanted. With measurements, the "
                                   "avatar is fitted to them."));
    connect(m_avatar_action, &QAction::triggered, this, &GarmentViewWidget::chooseAvatar);

    m_arrange_action = tool_bar->addAction(tr("Arrange"));
    m_arrange_action->setCheckable(true);
    m_arrange_action->setToolTip(tr("Put pieces on the avatar: click a piece, then one of the points shown on the "
                                    "avatar or any spot on it; drag a placed piece to move it around"));
    connect(m_arrange_action, &QAction::toggled, this, &GarmentViewWidget::setArranging);

    // As CLO's arrangement: which way round a piece on the avatar is, in the Arrange menu and on a right-click on the
    // piece.
    m_rotate_clockwise_action = new QAction(tr("Rotate Clockwise"), this);
    m_rotate_clockwise_action->setToolTip(tr("Turn the selected piece on the avatar a quarter of the way around "
                                             "clockwise, as seen from outside"));
    connect(m_rotate_clockwise_action, &QAction::triggered, this, &GarmentViewWidget::rotatePieceClockwise);
    m_rotate_counterclockwise_action = new QAction(tr("Rotate Counterclockwise"), this);
    m_rotate_counterclockwise_action->setToolTip(tr("Turn the selected piece on the avatar a quarter of the way "
                                                    "around counterclockwise, as seen from outside"));
    connect(m_rotate_counterclockwise_action, &QAction::triggered, this,
            &GarmentViewWidget::rotatePieceCounterclockwise);
    m_turn_over_action = new QAction(tr("Turn Over"), this);
    m_turn_over_action->setToolTip(tr("Turn the selected piece on the avatar over, its other side out, as if it were "
                                      "cut from the cloth turned over"));
    connect(m_turn_over_action, &QAction::triggered, this, &GarmentViewWidget::turnPieceOver);
    m_take_off_action = new QAction(tr("Take Off the Avatar"), this);
    m_take_off_action->setToolTip(tr("Put the selected piece back on the board"));
    connect(m_take_off_action, &QAction::triggered, this, &GarmentViewWidget::takePieceOff);

    // As CLO's Superimpose: the selected piece laid on the piece on the avatar it is sewn to the most.
    m_superimpose_over_action = new QAction(tr("Superimpose Over"), this);
    m_superimpose_over_action->setToolTip(tr("Lay the selected piece over the piece on the avatar it is sewn to, its "
                                             "sewn edges on that piece's, as a pocket or a patch lies"));
    connect(m_superimpose_over_action, &QAction::triggered, this, [this]()
    {
        superimposePiece(Superimpose::Over);
    });
    m_superimpose_under_action = new QAction(tr("Superimpose Under"), this);
    m_superimpose_under_action->setToolTip(tr("Lay the selected piece under the piece on the avatar it is sewn to, its "
                                              "sewn edges on that piece's, as a facing or a lining lies"));
    connect(m_superimpose_under_action, &QAction::triggered, this, [this]()
    {
        superimposePiece(Superimpose::Under);
    });
    m_superimpose_side_action = new QAction(tr("Superimpose Side"), this);
    m_superimpose_side_action->setToolTip(tr("Put the selected piece beside the piece on the avatar it is sewn to, "
                                             "edge to edge along their longest seam, as a collar or the next panel "
                                             "goes"));
    connect(m_superimpose_side_action, &QAction::triggered, this, [this]()
    {
        superimposePiece(Superimpose::Side);
    });

    // As CLO's layers: the layer the selected piece is worn in, over the pieces of lower layers where it lies on them.
    m_layer_menu = new QMenu(tr("Layer"), this);
    m_layer_menu->setToolTipsVisible(true);
    m_layer_menu->menuAction()->setToolTip(tr("Wear the selected piece over the pieces of lower layers where it lies "
                                              "on them, as a pocket on a front or a shell over its lining"));
    m_piece_layers = new QActionGroup(this);
    for (int layer = 0; layer <= highest_layer; ++layer)
    {
        QAction* action = m_layer_menu->addAction(layer == 0 ? tr("0, Next to the Body") : QString::number(layer));
        action->setData(layer);
        action->setCheckable(true);
        action->setToolTip(layer == 0 ? tr("Wear the selected piece under the pieces of higher layers where it lies on "
                                           "them, as pieces are unless told otherwise")
                                      : tr("Wear the selected piece over the pieces of layers 0 to %1 where it lies on "
                                           "them, and under those of higher layers").arg(layer - 1));
        m_piece_layers->addAction(action);
    }
    connect(m_piece_layers, &QActionGroup::triggered, this, &GarmentViewWidget::chooseLayer);

    QMenu* arrange_menu = new QMenu(this);
    arrange_menu->setToolTipsVisible(true);
    arrange_menu->addActions({m_rotate_clockwise_action, m_rotate_counterclockwise_action, m_turn_over_action});
    arrange_menu->addSeparator();
    arrange_menu->addActions({m_superimpose_over_action, m_superimpose_under_action, m_superimpose_side_action});
    arrange_menu->addSeparator();
    arrange_menu->addMenu(m_layer_menu);
    arrange_menu->addSeparator();
    arrange_menu->addAction(m_take_off_action);
    m_arrange_action->setMenu(arrange_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_arrange_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_piece_menu = new QMenu(this);
    m_piece_menu->setToolTipsVisible(true);
    m_piece_menu->addActions(arrange_menu->actions());

    m_simulate_action = tool_bar->addAction(tr("Simulate"));
    m_simulate_action->setCheckable(true);
    m_simulate_action->setToolTip(tr("Drape the pieces on the avatar, sewn together by their seams; at rest, the "
                                     "cloth can be pulled and pinned until draping is switched off again. Space "
                                     "starts and stops it"));
    m_simulate_action->setShortcut(Qt::Key_Space);
    m_simulate_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_simulate_action);
    connect(m_simulate_action, &QAction::toggled, this, &GarmentViewWidget::setSimulating);

    // How finely the cloth drapes is a setting of the simulation, in its menu.
    QMenu* simulate_menu = new QMenu(this);
    simulate_menu->setToolTipsVisible(true);
    m_fine_action = simulate_menu->addAction(tr("Fine Drape"));
    m_fine_action->setCheckable(true);
    m_fine_action->setToolTip(tr("Drape with smaller triangles, %1 instead of %2 cm: slower, but folds and the fit "
                                 "show in finer detail. A drape goes on from where it hangs.")
                                  .arg(fine_edge_length).arg(PieceMesher::defaultEdgeLength()));
    connect(m_fine_action, &QAction::toggled, this, &GarmentViewWidget::setFine);
    m_device_action = simulate_menu->addAction(tr("Drape on the Graphics Card"));
    m_device_action->setCheckable(true);
    m_device_action->setChecked(true);
    m_device_action->setToolTip(tr("Work out the drape on the graphics card, which is much faster, if it can; "
                                   "otherwise on the processor"));
    connect(m_device_action, &QAction::toggled, this, &GarmentViewWidget::setOnDevice);
    simulate_menu->addSeparator();
    m_remove_pins_action = simulate_menu->addAction(tr("Remove All Pins"));
    m_remove_pins_action->setToolTip(tr("Take out every pin holding the cloth; Shift+click on the cloth pins it, on a pin "
                                        "takes that one out"));
    connect(m_remove_pins_action, &QAction::triggered, this, &GarmentViewWidget::removeAllPins);
    m_simulate_action->setMenu(simulate_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_simulate_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_reset_action = tool_bar->addAction(tr("Reset"));
    m_reset_action->setToolTip(tr("Put the draped pieces back where they were arranged"));
    connect(m_reset_action, &QAction::triggered, this, &GarmentViewWidget::resetDrape);

    tool_bar = add_group();

    // As CLO's fit maps, one at a time: the button shows the map chosen in its menu.
    m_fit_action = tool_bar->addAction(tr("Fit Map"));
    m_fit_action->setCheckable(true);
    m_fit_action->setToolTip(tr("Color the cloth by how the garment fits, as chosen in the menu: how much the cloth is "
                                "stretched, how far it stands off the body, or how hard it presses on it"));
    connect(m_fit_action, &QAction::toggled, this, &GarmentViewWidget::showFitMap);

    QMenu* fit_menu = new QMenu(this);
    fit_menu->setToolTipsVisible(true);
    m_fit_maps = new QActionGroup(this);
    const struct
    {
        GarmentSceneModel::FitMap map;
        QString                   text;
        QString                   tip;
    } fit_maps[] = {
        {GarmentSceneModel::FitMap::Strain, tr("Strain"),
         tr("How much the cloth is stretched: green not at all, red 10% longer than drafted")},
        {GarmentSceneModel::FitMap::Ease, tr("Ease"),
         tr("How far the cloth stands off the body: red where it rests on it, blue 8 cm or more away")},
        {GarmentSceneModel::FitMap::Pressure, tr("Pressure"),
         tr("How hard the cloth presses on the body: green not at all, red 2 kPa or more")}};
    for (const auto& fit_map : fit_maps)
    {
        QAction* action = fit_menu->addAction(fit_map.text);
        action->setCheckable(true);
        action->setChecked(fit_map.map == GarmentSceneModel::FitMap::Strain);
        action->setData(static_cast<int>(fit_map.map));
        action->setToolTip(fit_map.tip);
        m_fit_maps->addAction(action);
    }
    connect(m_fit_maps, &QActionGroup::triggered, this, &GarmentViewWidget::chooseFitMap);
    m_fit_action->setMenu(fit_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_fit_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    m_checks_action = tool_bar->addAction(tr("Checks"));
    m_checks_action->setCheckable(true);
    m_checks_action->setToolTip(tr("Show the pieces in checks that run along their grainlines, the wider stripe along "
                                   "the grain: how each piece is cut, and whether the checks meet at its seams"));
    connect(m_checks_action, &QAction::toggled, m_scene_model, &GarmentSceneModel::setChecksShown);

    m_fabric_box = new QComboBox(tool_bar);
    updateFabricBox(m_doc->getFabrics());
    m_fabric_box->setToolTip(tr("The fabric the selected piece is cut from, or the whole garment when no piece is "
                                "selected"));
    tool_bar->addWidget(m_fabric_box);
    connect(m_fabric_box, QOverload<int>::of(&QComboBox::activated), this, &GarmentViewWidget::chooseFabric);

    // As CLO's fabric textures: an image of the fabric, kept in the pattern, in place of the piece's color.
    m_image_action = tool_bar->addAction(tr("Fabric Image"));
    m_image_action->setToolTip(tr("Show the fabric of the selected piece, or of the whole garment when no piece is "
                                  "selected, as an image of it, repeating across and along the grain. The image is "
                                  "kept in the pattern."));
    connect(m_image_action, &QAction::triggered, this, &GarmentViewWidget::chooseFabricImage);

    QMenu* image_menu = new QMenu(this);
    image_menu->setToolTipsVisible(true);
    m_image_width_action = image_menu->addAction(tr("Image Width..."));
    m_image_width_action->setToolTip(tr("How wide the cloth in the image is, which is how often it repeats"));
    connect(m_image_width_action, &QAction::triggered, this, &GarmentViewWidget::changeFabricImageWidth);
    m_remove_image_action = image_menu->addAction(tr("Remove Image"));
    m_remove_image_action->setToolTip(tr("Show the fabric in the piece's color again"));
    connect(m_remove_image_action, &QAction::triggered, this, &GarmentViewWidget::removeFabricImage);
    image_menu->addSeparator();
    m_shrinkage_action = image_menu->addAction(tr("Shrinkage..."));
    m_shrinkage_action->setToolTip(tr("How much the fabric of the selected piece, or of the whole garment when no piece "
                                      "is selected, shrinks across the grain and along it, as a rib knit worn snug "
                                      "does"));
    connect(m_shrinkage_action, &QAction::triggered, this, &GarmentViewWidget::chooseShrinkage);
    m_image_action->setMenu(image_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_image_action)))
    {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }

    tool_bar = add_group();

    // Where the garment is looked at from, and what shows; with keys laid out as on a number pad, the front at the
    // bottom.
    m_view_action = tool_bar->addAction(tr("View"));
    m_view_action->setToolTip(tr("Look at the garment from the front, the back, a side or the top, and show or hide "
                                 "the avatar, the cloth's triangles and pieces"));
    QMenu* view_menu = new QMenu(this);
    view_menu->setToolTipsVisible(true);
    const struct
    {
        QString text;
        int     key;
        qreal   pitch;
        qreal   yaw;
    } views[] = {{tr("Front"), Qt::Key_2, 0, 0},
                 {tr("Back"), Qt::Key_8, 0, 180},
                 {tr("Left Side"), Qt::Key_4, 0, 90},
                 {tr("Right Side"), Qt::Key_6, 0, -90},
                 {tr("Top"), Qt::Key_5, -90, 0}};
    for (const auto& view : views)
    {
        QAction* action = view_menu->addAction(view.text);
        action->setShortcut(view.key);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(action);
        const qreal pitch = view.pitch;
        const qreal yaw = view.yaw;
        connect(action, &QAction::triggered, m_scene_model, [this, pitch, yaw]()
        {
            m_scene_model->requestView(pitch, yaw);
        });
    }
    view_menu->addSeparator();
    QAction* avatar_shown = view_menu->addAction(tr("Show Avatar"));
    avatar_shown->setCheckable(true);
    avatar_shown->setChecked(true);
    avatar_shown->setToolTip(tr("Hidden, the cloth still drapes on the avatar, and it shows while arranging"));
    connect(avatar_shown, &QAction::toggled, m_scene_model, &GarmentSceneModel::setAvatarShown);
    QAction* mesh_shown = view_menu->addAction(tr("Show Mesh"));
    mesh_shown->setCheckable(true);
    mesh_shown->setToolTip(tr("Show the triangles the cloth is made of, as the drape works them out"));
    connect(mesh_shown, &QAction::toggled, m_scene_model, &GarmentSceneModel::setMeshShown);
    view_menu->addSeparator();
    m_hide_piece_action = view_menu->addAction(tr("Hide Selected Piece"));
    m_hide_piece_action->setToolTip(tr("Hide the selected piece, and its copy, to see what is under it; it still "
                                       "drapes"));
    m_hide_piece_action->setShortcut(Qt::Key_H);
    m_hide_piece_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_hide_piece_action);
    connect(m_hide_piece_action, &QAction::triggered, this, &GarmentViewWidget::hideSelectedPiece);
    m_show_pieces_action = view_menu->addAction(tr("Show All Pieces"));
    m_show_pieces_action->setShortcut(Qt::SHIFT | Qt::Key_H);
    m_show_pieces_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_show_pieces_action);
    connect(m_show_pieces_action, &QAction::triggered, this, &GarmentViewWidget::showAllPieces);
    m_view_action->setMenu(view_menu);
    if (QToolButton* button = qobject_cast<QToolButton*>(tool_bar->widgetForAction(m_view_action)))
    {
        button->setPopupMode(QToolButton::InstantPopup);
    }

    m_snapshot_action = tool_bar->addAction(tr("Snapshot"));
    m_snapshot_action->setToolTip(tr("Save the 3D view as it is now as an image: PNG or JPG"));
    connect(m_snapshot_action, &QAction::triggered, this, &GarmentViewWidget::saveSnapshot);

    m_export_action = tool_bar->addAction(tr("Export"));
    m_export_action->setToolTip(tr("Save the pieces on the avatar as they hang, and the avatar, for other 3D programs: "
                                   "glTF or OBJ"));
    connect(m_export_action, &QAction::triggered, this, &GarmentViewWidget::exportDrape);

    m_cancel_action = new QAction(this);
    m_cancel_action->setShortcut(Qt::Key_Escape);
    m_cancel_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_cancel_action, &QAction::triggered, this, &GarmentViewWidget::cancel);
    addAction(m_cancel_action);

    layout()->addWidget(tool_area);
    updateIcons();
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::updateIcons()
{
    if (m_sew_action != nullptr)
    {
        m_sew_action->setIcon(toolIcon(QStringLiteral("sew")));
        m_flip_action->setIcon(toolIcon(QStringLiteral("flip")));
        m_remove_action->setIcon(toolIcon(QStringLiteral("remove")));
        m_topstitch_action->setIcon(toolIcon(QStringLiteral("topstitch")));
        m_fold_action->setIcon(toolIcon(QStringLiteral("fold")));
        m_elastic_action->setIcon(toolIcon(QStringLiteral("elastic")));
        m_avatar_action->setIcon(toolIcon(QStringLiteral("avatar")));
        m_arrange_action->setIcon(toolIcon(QStringLiteral("arrange")));
        m_simulate_action->setIcon(toolIcon(QStringLiteral("simulate")));
        m_reset_action->setIcon(toolIcon(QStringLiteral("reset")));
        m_fine_action->setIcon(toolIcon(QStringLiteral("fine")));
        m_fit_action->setIcon(toolIcon(QStringLiteral("strain")));
        m_checks_action->setIcon(toolIcon(QStringLiteral("checks")));
        m_image_action->setIcon(toolIcon(QStringLiteral("fabric_image")));
        m_view_action->setIcon(toolIcon(QStringLiteral("views")));
        m_snapshot_action->setIcon(toolIcon(QStringLiteral("snapshot")));
        m_export_action->setIcon(toolIcon(QStringLiteral("export")));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// One of the toolbar's icons. Their outlines are black, like the rest of Seamly's; on a dark palette they are drawn
// in the color of text instead, so they still show.
QIcon GarmentViewWidget::toolIcon(const QString& name) const
{
    const bool dark = palette().color(QPalette::Window).lightness() < dark_lightness;
    const QColor outline = palette().color(QPalette::WindowText);

    QIcon icon;
    for (const QString& file : {name, name + QStringLiteral("@2x")})
    {
        QImage image(QStringLiteral(":/garment3d/icons/32x32/%1.png").arg(file));
        if (image.isNull())
        {
            continue;
        }
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height() && dark; ++y)
        {
            QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x)
            {
                if (qRed(line[x]) < black_level && qGreen(line[x]) < black_level && qBlue(line[x]) < black_level)
                {
                    line[x] = qRgba(outline.red(), outline.green(), outline.blue(), qAlpha(line[x]));
                }
            }
        }
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(image.width() / static_cast<qreal>(icon_size));
        icon.addPixmap(pixmap);
    }
    return icon;
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::saveSeams(const QString& text, const QVector<VSeam>& seams)
{
    qApp->getUndoStack()->push(new SaveSeams(text, m_doc->getSeams(), seams, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::saveArrangements(const QString& text, const QVector<VPieceArrangement>& arrangements)
{
    qApp->getUndoStack()->push(new SaveArrangements(text, m_doc->getArrangements(), arrangements, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::saveTopstitches(const VTopstitches& topstitches, const QString& text)
{
    qApp->getUndoStack()->push(new SaveTopstitches(text, m_doc->getTopstitches(), topstitches, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::saveFolds(const QVector<VFold>& folds, const QString& text)
{
    qApp->getUndoStack()->push(new SaveFolds(text, m_doc->getFolds(), folds, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::saveElastics(const QVector<VElastic>& elastics, const QString& text)
{
    qApp->getUndoStack()->push(new SaveElastics(text, m_doc->getElastics(), elastics, m_doc));
}

//---------------------------------------------------------------------------------------------------------------------
void GarmentViewWidget::showError(const QString& error)
{
    if (m_view_container != nullptr)
    {
        m_view_container->hide();
    }
    m_message_label->setText(tr("The 3D view could not be started.\n%1").arg(error));
    m_message_label->show();
}
