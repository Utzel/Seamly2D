//---------------------------------------------------------------------------------------------------------------------
//  @file   drape_runner.cpp
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

#include "drape_runner.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QSet>
#include <QThread>

#include "../vgarment/cloth_solver.h"
#include "../vgarment/compute_device.h"

namespace
{
// Steps per simulated second.
const int steps_per_second = 60;
const qreal time_step = 1.0 / steps_per_second;

// Frames go out at most this often, in ms.
const qint64 frame_interval_ms = 30;

// The cloth has come to rest once hardly any vertex has moved faster than this, in cm/s, on average over a second,
// and not before a second has passed. On average, as the eye sees it: from step to step, the solver's sweeps leave
// vertices a tiny bit to either side of where they settle, and cloth caught on a sharp part of the body twitches back
// and forth.
const float resting_speed = 1.0f;
const int resting_window = steps_per_second;
const int earliest_rest = steps_per_second;

// At rest, one vertex in this many may still move: a few caught on a sharp part of the body, such as the fingers,
// can keep twitching where they are.
const int restless_share = 200;

// The stitches are checked every so many steps.
const int check_steps = steps_per_second / 6;

// Pieces are sewn together first, without gravity, until no stitch is open wider than this, in cm, or for at most ten
// seconds; then they fall onto the body.
const qreal sewn_gap = 1.0;
const int max_sewing_steps = 10 * steps_per_second;

// Held cloth moves to where it is held at most this fast, in cm/s, as a hand would, so a quick jerk of the mouse
// doesn't tear it.
const qreal hold_speed = 300.0;

// At rest, the runner looks this often, in ms, whether the cloth is held somewhere else or pulled.
const unsigned long idle_ms = 15;
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
DrapeRunner::DrapeRunner(QObject* parent)
    : QObject(parent)
    , m_thread(nullptr)
    , m_generation(0)
    , m_stopping(false)
    , m_frame_pending(false)
    , m_on_device(true)
    , m_device()
    , m_holds_mutex()
    , m_holds()
    , m_holds_changes(0)
    , m_pulling(false)
{}

//---------------------------------------------------------------------------------------------------------------------
DrapeRunner::~DrapeRunner()
{
    stop();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Starts simulating; the solver is the runner's alone until it stops. A simulation already running stops.
void DrapeRunner::start(const QSharedPointer<ClothSolver>& solver)
{
    stop();
    m_stopping = false;
    m_frame_pending = false;
    const int generation = ++m_generation;

    // Set up on this thread, as some graphics APIs need, once.
    if (m_on_device && m_device == nullptr)
    {
        m_device.reset(new ComputeDevice());
    }
    ComputeDevice* device = m_on_device ? m_device.get() : nullptr;
    m_thread = QThread::create([this, solver, generation, device]()
    {
        run(solver, generation, device);
    });
    m_thread->start();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Stops simulating and waits until the thread is done.
void DrapeRunner::stop()
{
    if (m_thread != nullptr)
    {
        m_stopping = true;
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;

        // Frames still on their way belong to a drape that is over.
        ++m_generation;
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool DrapeRunner::isRunning() const
{
    return m_thread != nullptr && m_thread->isRunning();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The number of the latest start.
int DrapeRunner::generation() const
{
    return m_generation;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The last frame was shown, the next one may come.
void DrapeRunner::frameShown()
{
    m_frame_pending = false;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the sweeps run on the graphics card, if there is one, from the next start on.
void DrapeRunner::setOnDevice(bool on_device)
{
    m_on_device = on_device;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the cloth is held, by the solver's vertices it is held by; replaces what was held before, also while
/// running. Vertices the solver doesn't have are left out.
void DrapeRunner::setHolds(const QHash<int, QVector3D>& holds)
{
    QMutexLocker locker(&m_holds_mutex);
    if (holds != m_holds)
    {
        m_holds = holds;
        ++m_holds_changes;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the cloth is being pulled; while it is, it isn't taken to be at rest.
void DrapeRunner::setPulling(bool pulling)
{
    m_pulling = pulling;
}

//---------------------------------------------------------------------------------------------------------------------
// The simulation loop, on the runner's thread. Pieces held up in the air would fall past where they belong before
// their seams close, so they are sewn first and only then let go. While being sewn they may pass through each other
// to get to their seams, and they keep no speed from one step to the next: the seams pull hard, and cloth flung
// against the body by them would glance off it and slide away, such as trousers whose crotch is pulled up against
// the body's. Once let go, the cloth keeps from passing through itself. Says what it computes on: the graphics card,
// or with no name the processor, also when the card fails.
void DrapeRunner::run(QSharedPointer<ClothSolver> solver, int generation, ComputeDevice* device)
{
    const bool on_device = device != nullptr && solver->useDevice(device->open());
    emit computing(generation, on_device ? device->name() : QString());

    QElapsedTimer since_frame;
    since_frame.start();
    int steps = 0;
    int falling_steps = 0;
    int moving_steps = 0;  // since the start, or since the cloth was last held somewhere else or pulled
    bool resting = false;
    bool waiting = false;  // at rest, until the cloth is held somewhere else or pulled
    int holds_seen = m_holds_changes;
    bool was_pulling = m_pulling;
    QVector<QVector3D> window_start = solver->positions();

    const QVector3D gravity = solver->settings().gravity;
    const qreal friction = solver->settings().friction;
    const qreal air_damping = solver->settings().air_damping;
    bool sewing = solver->widestStitch() > sewn_gap;

    // The vertices held are pinned and taken to where they are held, as far as a step goes.
    QSet<int> held;
    auto hold = [this, &solver, &held]()
    {
        QHash<int, QVector3D> holds;
        {
            QMutexLocker locker(&m_holds_mutex);
            holds = m_holds;
        }
        for (auto vertex = held.begin(); vertex != held.end();)
        {
            if (holds.contains(*vertex))
            {
                ++vertex;
            }
            else
            {
                solver->setPinned(static_cast<quint32>(*vertex), false);
                vertex = held.erase(vertex);
            }
        }
        const float furthest = static_cast<float>(hold_speed * time_step);
        for (auto where = holds.cbegin(); where != holds.cend(); ++where)
        {
            if (where.key() < 0 || where.key() >= solver->vertexCount())
            {
                continue;
            }
            const quint32 vertex = static_cast<quint32>(where.key());
            if (!held.contains(where.key()))
            {
                solver->setPinned(vertex, true);
                held.insert(where.key());
            }
            const QVector3D at = solver->position(vertex);
            const QVector3D way = where.value() - at;
            solver->moveVertex(vertex, way.length() > furthest ? at + way.normalized() * furthest : where.value());
        }
    };
    if (sewing)
    {
        solver->setGravity(QVector3D());
        solver->setFriction(0);
        solver->setAirDamping(1.0 / time_step);
        solver->setPiecesPassThrough(true);
    }

    while (!m_stopping)
    {
        if (waiting)
        {
            if (!m_pulling && m_holds_changes == holds_seen)
            {
                QThread::msleep(idle_ms);
                continue;
            }
            waiting = false;
            emit woke(generation);
        }

        // Held somewhere else, or pulled or let go: whether the cloth is at rest is found out afresh.
        const bool pulling = m_pulling;
        const int changes = m_holds_changes;
        if (pulling != was_pulling || changes != holds_seen)
        {
            resting = false;
            moving_steps = 0;
            window_start = solver->positions();
        }
        was_pulling = pulling;
        holds_seen = changes;
        hold();
        solver->step(time_step);
        ++steps;
        ++moving_steps;
        falling_steps += sewing ? 0 : 1;

        if (sewing && steps % check_steps == 0 && (solver->widestStitch() <= sewn_gap || steps >= max_sewing_steps))
        {
            sewing = false;
            solver->setGravity(gravity);
            solver->setFriction(friction);
            solver->setAirDamping(air_damping);
            solver->setPiecesPassThrough(false);
        }

        if (moving_steps % resting_window == 0)
        {
            const QVector<QVector3D> positions = solver->positions();
            const float furthest = static_cast<float>(resting_speed * time_step * resting_window);
            int moving = 0;
            for (int i = 0; i < positions.size() && i < window_start.size(); ++i)
            {
                moving += (positions.at(i) - window_start.at(i)).length() >= furthest ? 1 : 0;
            }
            window_start = positions;
            resting = moving <= positions.size() / restless_share;
        }

        const bool at_rest = !sewing && falling_steps >= earliest_rest && moving_steps >= earliest_rest && resting
                             && !m_pulling;
        if (at_rest || (!m_frame_pending && since_frame.elapsed() >= frame_interval_ms))
        {
            m_frame_pending = true;
            since_frame.restart();
            emit frameReady(generation, solver->positions());
        }

        if (at_rest)
        {
            emit settled(generation);
            waiting = true;
        }
    }

    if (on_device && !solver->isOnDevice())
    {
        emit computing(generation, QString());
    }
    solver->useDevice(nullptr);
    if (device != nullptr)
    {
        device->close();
    }
}
