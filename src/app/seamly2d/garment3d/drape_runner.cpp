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
#include <QThread>

#include "../vgarment/cloth_solver.h"

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
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
DrapeRunner::DrapeRunner(QObject* parent)
    : QObject(parent)
    , m_thread(nullptr)
    , m_generation(0)
    , m_stopping(false)
    , m_frame_pending(false)
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
    m_thread = QThread::create([this, solver, generation]()
    {
        run(solver, generation);
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
// The simulation loop, on the runner's thread. Pieces held up in the air would fall past where they belong before
// their seams close, so they are sewn first and only then let go. While being sewn they may pass through each other
// to get to their seams; once let go, the cloth keeps from passing through itself.
void DrapeRunner::run(QSharedPointer<ClothSolver> solver, int generation)
{
    QElapsedTimer since_frame;
    since_frame.start();
    int steps = 0;
    int falling_steps = 0;
    bool resting = false;
    QVector<QVector3D> window_start = solver->positions();

    const QVector3D gravity = solver->settings().gravity;
    const qreal friction = solver->settings().friction;
    const bool self_contact = solver->settings().self_contact;
    bool sewing = solver->widestStitch() > sewn_gap;
    if (sewing)
    {
        solver->setGravity(QVector3D());
        solver->setFriction(0);
        solver->setSelfContact(false);
    }

    while (!m_stopping)
    {
        solver->step(time_step);
        ++steps;
        falling_steps += sewing ? 0 : 1;

        if (sewing && steps % check_steps == 0 && (solver->widestStitch() <= sewn_gap || steps >= max_sewing_steps))
        {
            sewing = false;
            solver->setGravity(gravity);
            solver->setFriction(friction);
            solver->setSelfContact(self_contact);
        }

        if (steps % resting_window == 0)
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

        const bool at_rest = !sewing && falling_steps >= earliest_rest && resting;
        if (at_rest || (!m_frame_pending && since_frame.elapsed() >= frame_interval_ms))
        {
            m_frame_pending = true;
            since_frame.restart();
            emit frameReady(generation, solver->positions());
        }

        if (at_rest)
        {
            emit settled(generation);
            m_stopping = true;
        }
    }
}
