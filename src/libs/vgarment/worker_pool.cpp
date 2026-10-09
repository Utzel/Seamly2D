//---------------------------------------------------------------------------------------------------------------------
//  @file   worker_pool.cpp
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

#include "worker_pool.h"

#include <QFile>
#include <QSet>
#include <QThread>

#include <chrono>

#if defined(Q_PROCESSOR_X86)
#include <immintrin.h>
#endif

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace
{
// How long a thread waits busily for the next part of the work before it sleeps, in microseconds: longer than the
// gaps between the colours of a sweep, shorter than a step's search for contacts.
const qint64 busy_wait_us = 200;

// How many times a busily waiting thread looks before it reads the clock again.
const int looks_per_reading = 64;

//---------------------------------------------------------------------------------------------------------------------
// Lets the other thread of the core run while this one waits busily.
void relax()
{
#if defined(Q_PROCESSOR_X86)
    _mm_pause();
#else
    std::this_thread::yield();
#endif
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
// A pool of as many threads, the calling one among them, or one for each core of the processor.
WorkerPool::WorkerPool(int thread_count)
    : m_threads()
    , m_mutex()
    , m_woken()
    , m_generation(0)
    , m_stopping(false)
    , m_shares()
{
    const int threads = thread_count > 0 ? thread_count : coreCount();
    for (int i = 1; i < threads; ++i)
    {
        m_threads.emplace_back(&WorkerPool::serve, this);
    }
}

//---------------------------------------------------------------------------------------------------------------------
WorkerPool::~WorkerPool()
{
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_stopping.store(true);
        m_generation.fetch_add(1);
    }
    m_woken.notify_all();
    for (std::thread& thread : m_threads)
    {
        thread.join();
    }
}

//---------------------------------------------------------------------------------------------------------------------
int WorkerPool::threadCount() const
{
    return static_cast<int>(m_threads.size()) + 1;
}

//---------------------------------------------------------------------------------------------------------------------
// How many cores the processor has, each of which may run two threads at once; as many as it runs threads at once
// where the system doesn't tell.
int WorkerPool::coreCount()
{
    const int threads = qMax(1, QThread::idealThreadCount());
    int cores = 0;
#if defined(Q_OS_WIN)
    DWORD size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
    if (size > 0)
    {
        QByteArray buffer(static_cast<int>(size), Qt::Uninitialized);
        auto* first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, first, &size))
        {
            for (DWORD offset = 0; offset < size;)
            {
                const auto* core = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.constData()
                                                                                                   + offset);
                ++cores;
                offset += core->Size;
            }
        }
    }
#elif defined(Q_OS_MACOS)
    int physical = 0;
    size_t size = sizeof(physical);
    if (sysctlbyname("hw.physicalcpu", &physical, &size, nullptr, 0) == 0)
    {
        cores = physical;
    }
#else
    // Each core once, by the processor it is on and its number there.
    QFile info(QStringLiteral("/proc/cpuinfo"));
    if (info.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QSet<QString> found;
        QString package;
        for (const QByteArray& line : info.readAll().split('\n'))
        {
            const QString text = QString::fromLatin1(line);
            const QString value = text.section(QLatin1Char(':'), 1).trimmed();
            if (text.startsWith(QLatin1String("physical id")))
            {
                package = value;
            }
            else if (text.startsWith(QLatin1String("core id")))
            {
                found.insert(package + QLatin1Char('/') + value);
            }
        }
        cores = static_cast<int>(found.size());
    }
#endif
    return cores > 0 ? qMin(cores, threads) : threads;
}

//---------------------------------------------------------------------------------------------------------------------
// Calls work(begin, end) for runs of [0, count) at most run_length long, on all the pool's threads, and returns once
// all are done. Which thread takes which run is left to chance, so the work of one run mustn't depend on another's.
void WorkerPool::forRuns(int count, int run_length, const std::function<void(int, int)>& work)
{
    if (count <= 0)
    {
        return;
    }
    run_length = qMax(1, run_length);
    if (m_threads.empty() || count <= run_length)
    {
        work(0, count);
        return;
    }

    // The work goes where the work before last was, once no thread looks at that any more; one that is only about to
    // sees it has been replaced and leaves it.
    const quint64 generation = m_generation.load(std::memory_order_relaxed) + 1;
    Share& share = m_shares[generation % 2];
    while (share.lookers.load() > 0)
    {
        relax();
    }
    share.work = &work;
    share.count = count;
    share.run_length = run_length;
    share.run_count = (count + run_length - 1) / run_length;
    share.next_run.store(0, std::memory_order_relaxed);
    share.done_runs.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_generation.store(generation);
    }
    m_woken.notify_all();

    takeRuns(share);
    while (share.done_runs.load(std::memory_order_acquire) < share.run_count)
    {
        relax();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// What each of the pool's own threads does: waits for work and takes runs of it.
void WorkerPool::serve()
{
    quint64 seen = 0;
    for (;;)
    {
        // Busily for a while, then asleep.
        const auto started = std::chrono::steady_clock::now();
        quint64 generation = m_generation.load();
        for (int looks = 1; generation == seen; ++looks)
        {
            relax();
            if (looks % looks_per_reading == 0
                && std::chrono::steady_clock::now() - started > std::chrono::microseconds(busy_wait_us))
            {
                std::unique_lock<std::mutex> locker(m_mutex);
                m_woken.wait(locker, [this, seen]()
                {
                    return m_generation.load() != seen;
                });
            }
            generation = m_generation.load();
        }
        seen = generation;
        if (m_stopping.load())
        {
            return;
        }

        // The work may have been done and replaced by later work meanwhile; then it is left alone.
        Share& share = m_shares[generation % 2];
        share.lookers.fetch_add(1);
        if (m_generation.load() == generation)
        {
            takeRuns(share);
        }
        share.lookers.fetch_sub(1);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Takes runs of the work until none are left.
void WorkerPool::takeRuns(Share& share)
{
    for (;;)
    {
        const int run = share.next_run.fetch_add(1, std::memory_order_relaxed);
        if (run >= share.run_count)
        {
            return;
        }
        const int begin = run * share.run_length;
        (*share.work)(begin, qMin(share.count, begin + share.run_length));
        share.done_runs.fetch_add(1, std::memory_order_release);
    }
}

