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
#include <QColor>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QPalette>
#include <QQmlError>
#include <QQuickView>
#include <QQuickWindow>
#include <QSignalBlocker>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <iterator>
#include <tuple>
#include <utility>

#include "../ifc/exception/vexception.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../vmisc/def.h"
#include "../vmisc/vabstractapplication.h"
#include "../vpatterndb/floatItemData/vpiecelabeldata.h"
#include "../vpatterndb/measurements_def.h"
#include "../vpatterndb/variables/vinternalvariable.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiece.h"
#include "../vgarment/cloth_solver.h"
#include "../vtools/undocommands/save_arrangements.h"
#include "../vtools/undocommands/save_seams.h"
#include "drape_runner.h"
#include "garment_scene_model.h"
#include "seam_editor.h"

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
// The pattern piece a piece of the garment is, or is the mirrored copy of.
quint32 patternPiece(quint32 id)
{
    return PieceOutline::isMirrorId(id) ? PieceOutline::mirrorId(id) : id;
}

// Edits come in bursts (dragging a point sends one per mouse move), so re-mesh once they pause.
const int rebuild_delay_ms = 150;

// Fitted measurements this far off, in cm, are mentioned in the avatar's note.
const qreal note_tolerance = 1.0;

// Birth dates further back than this are taken as not given; SeamlyMe's default is 1800-01-01.
const qreal oldest_age = 110.0;

// Age used when the measurements don't say, MakeHuman's young adult.
const qreal default_age = 25.0;
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
           && qFuzzyCompare(1.0 + gender, 1.0 + other.gender)
           && qFuzzyCompare(1.0 + age, 1.0 + other.age);
}

//---------------------------------------------------------------------------------------------------------------------
bool GarmentViewWidget::AvatarRequest::hasMeasurements() const
{
    return wanted.height > 0 || wanted.bust > 0 || wanted.waist > 0 || wanted.hip > 0 || wanted.neck > 0
           || wanted.upper_arm > 0 || wanted.lower_arm > 0 || wanted.arm > 0;
}

//---------------------------------------------------------------------------------------------------------------------
GarmentViewWidget::GarmentViewWidget(VContainer* data, VAbstractPattern* doc, QWidget* parent)
    : QWidget(parent)
    , m_data(data)
    , m_doc(doc)
    , m_scene_model(new GarmentSceneModel(this))
    , m_seam_editor(new SeamEditor(this))
    , m_sew_action(nullptr)
    , m_flip_action(nullptr)
    , m_remove_action(nullptr)
    , m_cancel_action(nullptr)
    , m_arrange_action(nullptr)
    , m_simulate_action(nullptr)
    , m_reset_action(nullptr)
    , m_strain_action(nullptr)
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
    connect(m_scene_model, &GarmentSceneModel::placeRequested, this, &GarmentViewWidget::placePiece);
    connect(m_scene_model, &GarmentSceneModel::selectedPieceChanged, this, &GarmentViewWidget::updateActions);
    connect(m_scene_model, &GarmentSceneModel::avatarChanged, this, &GarmentViewWidget::updateActions);
    connect(m_runner, &DrapeRunner::frameReady, this, &GarmentViewWidget::drapeFrame);
    connect(m_runner, &DrapeRunner::settled, this, &GarmentViewWidget::drapeSettled);
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
    m_simulate_action->setChecked(false);
    m_arrange_action->setChecked(false);
    m_runner->stop();
    m_draped.clear();
    m_arrangements.clear();
    m_wrap.reset();
    m_collider = BodyCollider();
    m_rebuild_timer->stop();
    m_rebuild_pending = false;
    m_mesh_cache.clear();
    m_has_avatar_request = false;
    ++m_avatar_generation;  // a fit still running belongs to what was cleared
    m_scene_model->clear();
    m_seam_editor->setSewing(false);
    m_seam_editor->setSeams(QVector<VSeam>());
    m_seam_editor->setPieces(QVector<SeamEditor::Piece>());
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

    // All meshes first: where a piece goes can depend on the pieces it is sewn to.
    QHash<quint32, CachedMesh> mesh_cache;
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
                if (cached.outline != outline || cached.wanted != wanted || cached.mesh.isEmpty())
                {
                    cached = garmentMeshes(id, outline, wanted);
                    m_draped.remove(id);
                    m_draped.remove(PieceOutline::mirrorId(id));
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
    m_turned_pairs.clear();  // turnedPairs() places body pieces, and those are never turned
    m_turned_pairs = turnedPairs();

    QVector<GarmentSceneModel::Piece> scene_pieces;
    QVector<SeamEditor::Piece> seam_pieces;
    m_garment_pieces.clear();
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

            const bool placed = !m_wrap.isNull() && m_arrangements.contains(id) && !cached.garment_mesh.isEmpty();
            if (placed)
            {
                scene_piece.mesh = cached.garment_mesh;
                scene_piece.positions = piecePositions(id, cached.garment_mesh);
                scene_pieces.append(scene_piece);
                m_garment_pieces.append({id, cached.garment_mesh});

                if (cached.symmetry == PieceSymmetry::Pair)
                {
                    GarmentSceneModel::Piece mirror_piece = scene_piece;
                    mirror_piece.id = PieceOutline::mirrorId(id);
                    mirror_piece.mesh = cached.mirror_mesh;
                    mirror_piece.positions = piecePositions(mirror_piece.id, cached.mirror_mesh);
                    scene_pieces.append(mirror_piece);
                    m_garment_pieces.append({mirror_piece.id, cached.mirror_mesh});
                }
            }
            else
            {
                // Pieces lie on the board as drafted, and seams are sewn there.
                scene_piece.mesh = cached.mesh;
                scene_pieces.append(scene_piece);

                SeamEditor::Piece seam_piece;
                seam_piece.id = id;
                seam_piece.outline = cached.outline;
                seam_piece.mirrored = cached.symmetry == PieceSymmetry::Pair;
                seam_pieces.append(seam_piece);
            }
        }
    }

    m_scene_model->setPieces(scene_pieces);
    m_seam_editor->setSeams(m_doc->getSeams());
    m_seam_editor->setPieces(seam_pieces);

    updateAvatar();

    if (simulating)
    {
        startSimulation();
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// Starts fitting the avatar if the measurements or the wearer changed. A change during a fit is picked up when the
// fit finishes. A pattern without measurements gets no avatar.
void GarmentViewWidget::updateAvatar()
{
    const AvatarRequest request = wantedAvatar();
    if (!m_has_avatar_request || !(request == m_avatar_request))
    {
        m_avatar_request = request;
        m_has_avatar_request = true;

        if (!request.hasMeasurements())
        {
            m_scene_model->clearAvatar();
            if (!m_wrap.isNull())
            {
                // Without an avatar, arranged pieces go back on the board.
                m_simulate_action->setChecked(false);
                m_draped.clear();
                m_wrap.reset();
                m_collider = BodyCollider();
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

            // A new body takes the drape with it; arranged pieces are put on it afresh.
            m_simulate_action->setChecked(false);
            m_draped.clear();
            m_wrap.reset(new BodyWrap(*m_body_model, result.positions));
            m_collider = BodyCollider(result.positions.mid(0, m_body_model->skinVertexCount()),
                                      m_body_model->triangles());
            rebuildScene();
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
// The pattern's measurements in cm, the ones the avatar can be fitted to; measurements that aren't there stay 0.
GarmentViewWidget::AvatarRequest GarmentViewWidget::wantedAvatar() const
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
    request.gender = m_wearer_gender;
    request.age = BodyShape::ageFromYears(m_wearer_age);
    return request;
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
        std::make_tuple(tr("shoulder tip to wrist"), wanted.arm, got.arm)};

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
// The pattern's seams changed, by sewing here or by undo and redo.
void GarmentViewWidget::updateSeams()
{
    m_seam_editor->setSeams(m_doc->getSeams());
}

//---------------------------------------------------------------------------------------------------------------------
// The pattern's arrangements changed, by placing pieces here or by undo and redo.
void GarmentViewWidget::updateArrangements()
{
    rebuildScene();
}

//---------------------------------------------------------------------------------------------------------------------
// Takes in the pattern's arrangements. A piece moved to another spot starts its drape over.
void GarmentViewWidget::readArrangements()
{
    QHash<quint32, PieceArrangement> arrangements;
    for (const VPieceArrangement& stored : m_doc->getArrangements())
    {
        PieceArrangement arrangement;
        arrangement.part = BodyWrap::partFromName(stored.part);
        arrangement.angle = stored.angle;
        arrangement.height = stored.height;
        arrangements.insert(stored.piece_id, arrangement);
    }

    for (auto draped = m_draped.begin(); draped != m_draped.end();)
    {
        const quint32 piece = patternPiece(draped.key());
        const PieceArrangement before = m_arrangements.value(piece);
        const PieceArrangement after = arrangements.value(piece);
        const bool same = arrangements.contains(piece) && before.part == after.part
                          && qFuzzyCompare(1.0 + before.angle, 1.0 + after.angle)
                          && qFuzzyCompare(1.0 + before.height, 1.0 + after.height);
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
            if (PieceOutline::isMirrorId(id) != m_turned_pairs.contains(piece))
            {
                const CachedMesh& cached = m_mesh_cache.value(piece);
                const GarmentMesh& other = PieceOutline::isMirrorId(id) ? cached.garment_mesh : cached.mirror_mesh;
                positions = m_wrap->place(other, m_arrangements.value(piece));
                for (QVector3D& position : positions)
                {
                    position = m_wrap->mirrored(position);
                }
            }
            else
            {
                positions = m_wrap->place(mesh, m_arrangements.value(piece));
            }
        }
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
// Pieces cut twice that are arranged on an arm or a leg, but sewn mostly to body pieces on the other side of the
// body: they are drafted for the other side. Turned, the mirrored copy goes where the piece was put, so a sleeve goes
// to the armhole it is sewn to whichever arm it was put on.
QSet<quint32> GarmentViewWidget::turnedPairs() const
{
    QHash<quint32, int> votes;
    if (!m_wrap.isNull())
    {
        auto onLimb = [this](quint32 piece)
        {
            return m_arrangements.contains(piece) && m_arrangements.value(piece).part != BodyPart::Body
                   && m_mesh_cache.value(piece).symmetry == PieceSymmetry::Pair;
        };
        auto onBody = [this](quint32 piece)
        {
            return m_arrangements.contains(piece) && m_arrangements.value(piece).part == BodyPart::Body
                   && !m_mesh_cache.value(piece).garment_mesh.isEmpty();
        };

        for (const VSeam& seam : m_doc->getSeams())
        {
            for (const auto& sides : {std::make_pair(seam.first, seam.second), std::make_pair(seam.second, seam.first)})
            {
                const quint32 piece = sides.first.piece_id;
                const quint32 partner = sides.second.piece_id;
                if (onLimb(piece) && onBody(partner))
                {
                    const CachedMesh& own = m_mesh_cache.value(piece);
                    const CachedMesh& other = m_mesh_cache.value(partner);
                    const qreal own_across = acrossBody(own.garment_mesh,
                                                        m_wrap->place(own.garment_mesh, m_arrangements.value(piece)),
                                                        sides.first);
                    const qreal other_across = acrossBody(other.garment_mesh,
                                                          piecePositions(partner, other.garment_mesh), sides.second);
                    votes[piece] += own_across * other_across < 0 ? 1 : -1;
                }
            }
        }
    }

    QSet<quint32> turned;
    for (auto vote = votes.constBegin(); vote != votes.constEnd(); ++vote)
    {
        if (vote.value() > 0)
        {
            turned.insert(vote.key());
        }
    }
    return turned;
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
// Esc steps back: out of arranging, or out of what sewing or a selected seam was doing.
void GarmentViewWidget::cancel()
{
    if (m_scene_model->isArranging())
    {
        m_arrange_action->setChecked(false);
    }
    else
    {
        m_seam_editor->cancel();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// While arranging, a click on a piece picks it and a click on the avatar puts it there. Sewing and arranging take
// turns.
void GarmentViewWidget::setArranging(bool arranging)
{
    if (arranging)
    {
        m_sew_action->setChecked(false);
    }
    m_scene_model->setArranging(arranging);
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar was clicked while arranging: the selected piece goes there.
void GarmentViewWidget::placePiece(const QVector3D& point)
{
    const quint32 piece = m_scene_model->selectedPiece();
    if (piece != 0 && !m_wrap.isNull())
    {
        const PieceArrangement wanted = m_wrap->arrangementAt(point);
        VPieceArrangement arrangement;
        arrangement.piece_id = piece;
        arrangement.part = BodyWrap::partName(wanted.part);
        arrangement.angle = wanted.angle;
        arrangement.height = wanted.height;

        QVector<VPieceArrangement> arrangements = m_doc->getArrangements();
        auto existing = std::find_if(arrangements.begin(), arrangements.end(),
                                     [piece](const VPieceArrangement& other)
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
        saveArrangements(tr("place piece"), arrangements);
    }
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
    }
    updateActions();
}

//---------------------------------------------------------------------------------------------------------------------
// Puts the pieces from the arrangement into a solver, the seams between them as stitches and the avatar as what
// they drape over, and sets it going. Pieces already draped go on from where they are.
void GarmentViewWidget::startSimulation()
{
    m_runner->stop();
    m_drape_pieces.clear();

    QSharedPointer<ClothSolver> solver(new ClothSolver());
    QHash<quint32, quint32> offsets;
    QHash<quint32, GarmentMesh> meshes;
    GarmentSymmetry symmetry;
    for (const GarmentPiece& garment_piece : m_garment_pieces)
    {
        const QVector<QVector3D> positions = piecePositions(garment_piece.id, garment_piece.mesh);
        if (!positions.isEmpty())
        {
            DrapePiece drape_piece;
            drape_piece.id = garment_piece.id;
            drape_piece.offset = static_cast<int>(solver->addMesh(garment_piece.mesh, positions));
            drape_piece.count = garment_piece.mesh.vertexCount();
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

    // The pattern's seams, and their twins on the other side of the garment.
    QVector<GarmentSeam> seams;
    for (const VSeam& stored : m_doc->getSeams())
    {
        GarmentSeam seam;
        seam.first = {stored.first.piece_id, stored.first.start_node, stored.first.end_node};
        seam.second = {stored.second.piece_id, stored.second.start_node, stored.second.end_node};
        seam.reverse = stored.reverse;
        seams.append(seam);
    }
    for (const GarmentSeam& seam : symmetry.madeUp(seams))
    {
        if (offsets.contains(seam.first.piece) && offsets.contains(seam.second.piece))
        {
            const SeamStretch first = meshes.value(seam.first.piece).stretch(seam.first.start_node,
                                                                             seam.first.end_node,
                                                                             offsets.value(seam.first.piece));
            SeamStretch second = meshes.value(seam.second.piece).stretch(seam.second.start_node, seam.second.end_node,
                                                                         offsets.value(seam.second.piece));
            if (seam.reverse)
            {
                second = second.reversed();
            }
            solver->addStitches(SeamStretch::stitches(first, second));
        }
    }

    solver->setCollider(m_collider);
    m_runner->start(solver);
}

//---------------------------------------------------------------------------------------------------------------------
// Back to the pieces as arranged, before any draping.
void GarmentViewWidget::resetDrape()
{
    m_simulate_action->setChecked(false);
    m_draped.clear();
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
        m_runner->frameShown();
        m_reset_action->setEnabled(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The cloth came to rest, so the simulation stopped by itself.
void GarmentViewWidget::drapeSettled(int generation)
{
    if (generation == m_runner->generation())
    {
        m_simulate_action->setChecked(false);
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

    const bool seam_selected = m_seam_editor->selectedSeam() >= 0;
    const bool placed_selected = m_scene_model->isArranging()
                                 && m_scene_model->isPlaced(m_scene_model->selectedPiece());
    m_flip_action->setEnabled(seam_selected);
    m_remove_action->setEnabled(seam_selected || placed_selected);

    const bool has_avatar = m_scene_model->hasAvatar();
    m_arrange_action->setEnabled(has_avatar);
    m_simulate_action->setEnabled(has_avatar);
    m_reset_action->setEnabled(!m_draped.isEmpty());

    // With nothing to step back from, Esc is left to the main window.
    m_cancel_action->setEnabled(m_seam_editor->isSewing() || seam_selected || m_scene_model->isArranging());

    if ((m_seam_editor->isSewing() || m_scene_model->isArranging()) && m_view_container != nullptr)
    {
        m_view_container->setFocus();
    }
    updateHint();
}

//---------------------------------------------------------------------------------------------------------------------
// What to do next while arranging or draping, for the scene to show.
void GarmentViewWidget::updateHint()
{
    QString hint;
    if (m_runner->isRunning())
    {
        hint = tr("Draping. Simulate stops it, Reset puts the pieces back where they were arranged.");
    }
    else if (m_scene_model->isArranging())
    {
        hint = m_scene_model->selectedPiece() == 0
               ? tr("Click a piece, then the spot on the avatar where it goes. Esc stops arranging.")
               : tr("Click the spot on the avatar where the piece goes. Remove puts a placed piece back on the board.");
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
    layout()->addWidget(m_view_container);
}

//---------------------------------------------------------------------------------------------------------------------
// Sewing, flipping and removing seams. Delete and Esc only act while the view has the focus, so they don't get in
// the way of the piece scene's.
void GarmentViewWidget::createToolBar()
{
    QToolBar* tool_bar = new QToolBar(this);
    tool_bar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_sew_action = tool_bar->addAction(tr("Sew"));
    m_sew_action->setCheckable(true);
    m_sew_action->setToolTip(tr("Sew pieces together: click an edge near the end where the seam starts, then the "
                                "edge it is sewn to near the end that meets it"));
    connect(m_sew_action, &QAction::toggled, m_seam_editor, &SeamEditor::setSewing);

    m_flip_action = tool_bar->addAction(tr("Flip"));
    m_flip_action->setToolTip(tr("Turn the selected seam around, when its lines cross"));
    connect(m_flip_action, &QAction::triggered, this, &GarmentViewWidget::flipSeam);

    m_remove_action = tool_bar->addAction(tr("Remove"));
    m_remove_action->setToolTip(tr("Take the selected seam out, or while arranging put the selected piece back on "
                                   "the board"));
    m_remove_action->setShortcut(QKeySequence::Delete);
    m_remove_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_remove_action, &QAction::triggered, this, &GarmentViewWidget::removeSelected);

    tool_bar->addSeparator();

    m_arrange_action = tool_bar->addAction(tr("Arrange"));
    m_arrange_action->setCheckable(true);
    m_arrange_action->setToolTip(tr("Put pieces on the avatar: click a piece, then the spot where it goes"));
    connect(m_arrange_action, &QAction::toggled, this, &GarmentViewWidget::setArranging);

    m_simulate_action = tool_bar->addAction(tr("Simulate"));
    m_simulate_action->setCheckable(true);
    m_simulate_action->setToolTip(tr("Drape the pieces on the avatar, sewn together by their seams"));
    connect(m_simulate_action, &QAction::toggled, this, &GarmentViewWidget::setSimulating);

    m_reset_action = tool_bar->addAction(tr("Reset"));
    m_reset_action->setToolTip(tr("Put the draped pieces back where they were arranged"));
    connect(m_reset_action, &QAction::triggered, this, &GarmentViewWidget::resetDrape);

    tool_bar->addSeparator();

    m_strain_action = tool_bar->addAction(tr("Strain"));
    m_strain_action->setCheckable(true);
    m_strain_action->setToolTip(tr("Color the cloth by how much it is stretched: green not at all, red %1% or more")
                                    .arg(qRound(m_scene_model->fullStrain() * 100)));
    connect(m_strain_action, &QAction::toggled, m_scene_model, &GarmentSceneModel::setStrainShown);

    m_cancel_action = new QAction(this);
    m_cancel_action->setShortcut(Qt::Key_Escape);
    m_cancel_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_cancel_action, &QAction::triggered, this, &GarmentViewWidget::cancel);
    addAction(m_cancel_action);

    layout()->addWidget(tool_bar);
    updateActions();
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
void GarmentViewWidget::showError(const QString& error)
{
    if (m_view_container != nullptr)
    {
        m_view_container->hide();
    }
    m_message_label->setText(tr("The 3D view could not be started.\n%1").arg(error));
    m_message_label->show();
}
