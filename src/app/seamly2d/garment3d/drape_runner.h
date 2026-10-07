//---------------------------------------------------------------------------------------------------------------------
//  @file   drape_runner.h
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

#ifndef DRAPE_RUNNER_H
#define DRAPE_RUNNER_H

#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVector3D>
#include <QVector>

#include <atomic>
#include <memory>

class ClothSolver;
class ComputeDevice;
class QThread;

/// @brief Runs a cloth simulation on a thread of its own and hands out where the cloth is, a frame at a time.
///
/// The simulation runs as fast as it can, not in real time, so a drape settles quickly. A new frame is only sent
/// once the last one was shown, so a slow view never falls behind. Once the cloth has come to rest the runner
/// stops by itself. Each start counts up the generation, so frames still on their way from an earlier run can be
/// told apart.
///
/// The solver's sweeps run on the graphics card, if there is one that can compute and it isn't switched off; the
/// runner opens it on its thread for each run and closes it again after.
class DrapeRunner : public QObject
{
    Q_OBJECT

public:
    explicit           DrapeRunner(QObject* parent = nullptr);
    virtual           ~DrapeRunner();

    void               start(const QSharedPointer<ClothSolver>& solver);
    void               stop();
    bool               isRunning() const;
    int                generation() const;
    void               frameShown();
    void               setOnDevice(bool on_device);

signals:
    void               frameReady(int generation, const QVector<QVector3D>& positions);
    void               settled(int generation);
    void               computing(int generation, const QString& device);

private:
    Q_DISABLE_COPY(DrapeRunner)

    QThread*           m_thread;
    int                m_generation;
    std::atomic<bool>  m_stopping;
    std::atomic<bool>  m_frame_pending;
    bool               m_on_device;
    std::unique_ptr<ComputeDevice> m_device;

    void               run(QSharedPointer<ClothSolver> solver, int generation, ComputeDevice* device);
};

#endif // DRAPE_RUNNER_H
