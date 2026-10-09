//---------------------------------------------------------------------------------------------------------------------
//  @file   worker_pool.h
//  @author Julius
//  @date   9 Oct, 2026
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

#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include <QtGlobal>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/// @brief Threads of its own that share out work in runs, the calling thread too, for work that comes in many small
/// parts one after the other, as the solver's sweeps do: a colour of a garment's vertices takes a few dozen
/// microseconds to solve on all threads, about what waking QThreadPool's threads takes. Between parts, the threads
/// wait for the next one busily for a little while, then sleep.
///
/// By default there is a thread for each core of the processor, not for each thread it runs at once: two threads on
/// one core share its arithmetic, and waiting for the slower of them costs more than the second one brings.
///
/// The work is done once all its runs are: a thread the system keeps waiting, as it does when other programs are busy,
/// holds up no more than the run it has taken, if any.
class WorkerPool
{
public:
    explicit           WorkerPool(int thread_count = 0);
                       ~WorkerPool();

    int                threadCount() const;
    void               forRuns(int count, int run_length, const std::function<void(int, int)>& work);

    static int         coreCount();

private:
    Q_DISABLE_COPY(WorkerPool)

    // Work shared out: its runs, which the threads take one after the other, how many are done, and how many threads
    // are looking at it, which it mustn't be replaced while they do.
    struct Share
    {
        const std::function<void(int, int)>* work = nullptr;
        int              count = 0;
        int              run_length = 1;
        int              run_count = 0;
        std::atomic<int> next_run{0};
        std::atomic<int> done_runs{0};
        std::atomic<int> lookers{0};
    };

    std::vector<std::thread> m_threads;
    std::mutex               m_mutex;
    std::condition_variable  m_woken;
    std::atomic<quint64>     m_generation;
    std::atomic<bool>        m_stopping;
    Share                    m_shares[2];  // the latest work and the one before, by its generation

    void               serve();
    static void        takeRuns(Share& share);
};

#endif // WORKER_POOL_H
