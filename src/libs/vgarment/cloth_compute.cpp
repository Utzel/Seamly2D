//---------------------------------------------------------------------------------------------------------------------
//  @file   cloth_compute.cpp
//  @author Julius
//  @date   7 Oct, 2026
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

#include "cloth_compute.h"

#include <QByteArray>
#include <QFile>
#include <rhi/qrhi.h>

#include <cstring>

//---------------------------------------------------------------------------------------------------------------------
// The shaders are resources of this static library, which have to be set up by name, outside any namespace.
static void initClothShaders()
{
    Q_INIT_RESOURCE(cloth_shaders);
}

namespace
{
// Invocations per workgroup, as the shaders that work on one vertex or contact each have it.
const int workgroup = 64;

// Invocations per workgroup of the search, and members of each of its clusters, as cloth_search.comp has them; and
// how many numbers each of its workgroups sums.
const int search_lanes = 32;
const int summed_block = 32 * search_lanes;

// The search's phases, as cloth_search.comp's modes.
const int set_search = 0;
const int set_contacts = 1;
const int reach_out = 2;
const int pair_up = 3;
const int count_found = 4;
const int list_found = 5;
const int sum_blocks = 6;
const int sum_sums = 7;
const int add_up = 8;
const int count_roles = 9;
const int list_roles = 10;
const int sort_roles = 11;
const int start_contacts = 12;
const int bound_moves = 13;

// A float's infinity, as the bits the search keeps how far each vertex may move in.
const qint32 unbounded_bits = 0x7f800000;

// Room for at least so many self contacts, and for half again as many as there were once there wasn't room.
const int least_capacity = 4096;

// The sweep's workgroups, one for each vertex, come in rows of at most this many, as cloth_sweep.comp has it; a
// dispatch may have no more side by side.
const int workgroup_row = 65535;

// The Sweep block: where the colour starts and how many vertices it has, the Chebyshev weight and whether to use it.
struct SweepBlock
{
    qint32 first = 0;
    qint32 count = 0;
    float  weight = 0;
    qint32 accelerate = 0;
};

// The search's Phase block, and how many workgroups it takes.
struct Phase
{
    qint32  mode = 0;
    qint32  first = 0;
    qint32  count = 0;
    qint32  value = 0;
    quint32 groups = 0;
};

// No buffer is smaller than this, in bytes; empty ones can't be made.
const int smallest_buffer = 16;

//---------------------------------------------------------------------------------------------------------------------
QShader loadShader(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
}

//---------------------------------------------------------------------------------------------------------------------
quint32 groups(int count)
{
    return static_cast<quint32>((count + workgroup - 1) / workgroup);
}

//---------------------------------------------------------------------------------------------------------------------
quint32 searchGroups(int count)
{
    return static_cast<quint32>((count + search_lanes - 1) / search_lanes);
}

//---------------------------------------------------------------------------------------------------------------------
int summedBlocks(int count)
{
    return (count + summed_block - 1) / summed_block;
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
quint32 bytes(const QVector<T>& values)
{
    return static_cast<quint32>(values.size() * static_cast<int>(sizeof(T)));
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Computes on the device; it must be able to.
ClothCompute::ClothCompute(QRhi* rhi)
    : m_rhi(rhi)
    , m_step()
    , m_sweeps()
    , m_positions()
    , m_motion()
    , m_snapshot()
    , m_earlier()
    , m_stopped()
    , m_topology()
    , m_terms()
    , m_contacts()
    , m_pushes()
    , m_search_block()
    , m_boxes()
    , m_found()
    , m_sweep_bindings()
    , m_accelerate_bindings()
    , m_contact_bindings()
    , m_search_bindings()
    , m_sweep_pipeline()
    , m_accelerate_pipeline()
    , m_contact_pipeline()
    , m_search_pipeline()
    , m_colour_starts()
    , m_search()
    , m_vertex_count(0)
    , m_slot(0)
    , m_capacity(least_capacity)
    , m_found_kept(false)
    , m_ready(false)
{
    initClothShaders();
    if (m_rhi != nullptr && m_rhi->isFeatureSupported(QRhi::Compute))
    {
        m_slot = m_rhi->ubufAligned(static_cast<int>(qMax(sizeof(SweepBlock), 4 * sizeof(qint32))));
        m_step.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Step)));
        m_search_block.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Search)));
        m_ready = m_step->create() && m_search_block->create();
    }
}

//---------------------------------------------------------------------------------------------------------------------
ClothCompute::~ClothCompute() = default;

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether it can compute: the device can, and the cloth, once set, is on it.
bool ClothCompute::isReady() const
{
    return m_ready;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Puts the cloth on the card. False if the card can't take it, and then it computes no more.
bool ClothCompute::setCloth(const Cloth& cloth)
{
    if (!m_ready || cloth.vertex_count <= 0 || cloth.colour_starts.size() < 2)
    {
        m_ready = false;
        return false;
    }
    m_vertex_count = cloth.vertex_count;
    m_colour_starts = cloth.colour_starts;
    m_found_kept = false;

    // The search's own parts of its buffers: after which way the cloth faces around each vertex, the reaches of the
    // cluster members and of the clusters; the pairs of clusters, then the contacts each member found, the sums of
    // their blocks, the roles each vertex has been given and how far it may move.
    const int count = cloth.vertex_count;
    m_search = cloth.search;
    const int clusters = m_search.vertex_clusters + m_search.face_clusters + m_search.edge_clusters;
    const int queries = search_lanes * (m_search.vertex_clusters + m_search.edge_clusters);
    m_search.boxes = count;
    m_search.cluster_boxes = count + 2 * search_lanes * clusters;
    m_search.face_words = (m_search.face_clusters + 31) / 32;
    m_search.edge_words = (m_search.edge_clusters + 31) / 32;
    m_search.pairs = 0;
    m_search.found = m_search.vertex_clusters * m_search.face_words + m_search.edge_clusters * m_search.edge_words;
    m_search.block_sums = m_search.found + queries + 1;
    m_search.cursors = m_search.block_sums + qMax(summedBlocks(queries), summedBlocks(count)) + 1;
    m_search.bounds = m_search.cursors + count;

    const int vector = 4 * static_cast<int>(sizeof(float));
    const int number = static_cast<int>(sizeof(qint32));
    bool remade = false;
    m_ready = fit(&m_positions, count * vector, true, &remade) && fit(&m_motion, 2 * count * vector, true, &remade)
              && fit(&m_snapshot, count * vector, true, &remade) && fit(&m_earlier, count * vector, true, &remade)
              && fit(&m_stopped, count * number, true, &remade)
              && fit(&m_topology, static_cast<int>(bytes(cloth.topology)), true, &remade)
              && fit(&m_terms, static_cast<int>(bytes(cloth.terms)), true, &remade)
              && fit(&m_contacts, (2 * count + 2 + 12 * m_capacity) * number, true, &remade)
              && fit(&m_pushes, 36 * m_capacity * number, true, &remade)
              && fit(&m_boxes, (m_search.cluster_boxes + 2 * clusters) * vector, true, &remade)
              && fit(&m_found, (m_search.bounds + count) * number, true, &remade)
              && fit(&m_sweeps, m_slot, false, &remade) && createBindings();
    if (!m_ready)
    {
        return false;
    }

    QRhiCommandBuffer* commands = nullptr;
    if (m_rhi->beginOffscreenFrame(&commands) != QRhi::FrameOpSuccess)
    {
        m_ready = false;
        return false;
    }
    QRhiResourceUpdateBatch* upload = m_rhi->nextResourceUpdateBatch();
    upload->uploadStaticBuffer(m_topology.get(), 0, bytes(cloth.topology), cloth.topology.constData());
    upload->uploadStaticBuffer(m_terms.get(), 0, bytes(cloth.terms), cloth.terms.constData());
    commands->resourceUpdate(upload);
    m_ready = m_rhi->endOffscreenFrame() == QRhi::FrameOpSuccess;
    return m_ready;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Runs a step from its start: finds the parts of the cloth that may touch each other, if the start asks for it
/// or those found before aren't on the card, starts them for the step and sweeps. Gives back where the vertices went,
/// four numbers each, which the cloth nearby stopped, and whether the parts were found again. False if the card
/// failed, and then it computes no more.
bool ClothCompute::sweep(const Step& step, const Start& start, QVector<float>* positions, QVector<qint32>* stopped,
                         bool* searched)
{
    const int count = m_vertex_count;
    const int colours = static_cast<int>(m_colour_starts.size()) - 1;
    const int sweeps = static_cast<int>(start.weights.size());
    if (!m_ready || step.vertex_count != count || start.positions.size() != 4 * count
        || start.motion.size() != 8 * count || start.body.size() != count
        || (!start.facing.isEmpty() && start.facing.size() != 4 * count))
    {
        return false;
    }

    // Should there be more contacts than room for them, there is room made for them and the step runs again.
    const int number = static_cast<int>(sizeof(qint32));
    bool search = start.self_contact && (start.search || !m_found_kept);
    for (;;)
    {
        // The self contacts follow the body triangles in their buffer, as much room as there is for them.
        Step this_step = step;
        this_step.self_count = count;
        this_step.self_starts = count + 1;
        this_step.self_roles = 2 * count + 2;
        this_step.self_contacts = this_step.self_roles + 4 * m_capacity;
        this_step.self_capacity = m_capacity;
        Search this_search = m_search;
        this_search.pass_through = start.pass_through ? 1 : 0;
        this_search.in_layers = start.facing.isEmpty() ? 0 : 1;

        // Made anew, the buffer has lost the contacts found.
        bool contacts_remade = false;
        bool remade = false;
        if (!fit(&m_contacts, (this_step.self_contacts + 8 * m_capacity) * number, true, &contacts_remade)
            || !fit(&m_pushes, 36 * m_capacity * number, true, &remade))
        {
            m_ready = false;
            return false;
        }
        search = search || (contacts_remade && start.self_contact);

        // The search's phases: finding the contacts again, if it does, and listing each vertex's; then starting them
        // for the step and keeping the vertices' first guesses within how far they may move.
        QVector<Phase> phases;
        auto add = [&phases](int mode, int first, int how_many, int value, quint32 group_count)
        {
            if (group_count > 0)
            {
                phases.append(Phase{mode, first, how_many, value, group_count});
            }
        };
        const int clusters = m_search.vertex_clusters + m_search.face_clusters + m_search.edge_clusters;
        const int queries = search_lanes * (m_search.vertex_clusters + m_search.edge_clusters);
        const int pair_words = m_search.vertex_clusters * m_search.face_words
                               + m_search.edge_clusters * m_search.edge_words;
        if (!start.self_contact)
        {
            add(set_contacts, this_step.self_count, count + 2, 0, searchGroups(count + 2));
        }
        else if (search)
        {
            add(set_contacts, this_step.self_starts, count + 1, 0, searchGroups(count + 1));
            add(set_search, m_search.cursors, count, 0, searchGroups(count));
            add(reach_out, 0, 0, 0, static_cast<quint32>(clusters));
            add(pair_up, 0, pair_words, 0, searchGroups(pair_words));
            add(count_found, 0, 0, 0, static_cast<quint32>(queries / search_lanes));
            add(sum_blocks, m_search.found, queries, 0, static_cast<quint32>(summedBlocks(queries)));
            add(sum_sums, m_search.found, queries, 0, 1);
            add(add_up, m_search.found, queries, 0, static_cast<quint32>(summedBlocks(queries)));
            add(list_found, 0, 0, 0, static_cast<quint32>(queries / search_lanes));
            add(count_roles, 0, 0, 0, searchGroups(m_capacity));
            add(sum_blocks, this_step.self_starts, count, 1, static_cast<quint32>(summedBlocks(count)));
            add(sum_sums, this_step.self_starts, count, 1, 1);
            add(add_up, this_step.self_starts, count, 1, static_cast<quint32>(summedBlocks(count)));
            add(list_roles, 0, 0, 0, searchGroups(m_capacity));
            add(sort_roles, 0, 0, 0, searchGroups(count));
        }
        add(set_search, m_search.bounds, count, unbounded_bits, searchGroups(count));
        if (start.self_contact)
        {
            add(start_contacts, 0, 0, 0, searchGroups(m_capacity));
        }
        add(bound_moves, 0, 0, 0, searchGroups(count));

        // The Sweep blocks, one for each colour, then one for each sweep's acceleration, and the Phase blocks, each in
        // its own slot.
        const int phase_slots = colours + sweeps;
        const int slot_count = phase_slots + static_cast<int>(phases.size());
        if (!fit(&m_sweeps, slot_count * m_slot, false, &remade) || ((remade || contacts_remade) && !createBindings()))
        {
            m_ready = false;
            return false;
        }
        QByteArray blocks(slot_count * m_slot, 0);
        for (int colour = 0; colour < colours; ++colour)
        {
            SweepBlock block;
            block.first = m_colour_starts.at(colour);
            block.count = m_colour_starts.at(colour + 1) - block.first;
            std::memcpy(blocks.data() + colour * m_slot, &block, sizeof(block));
        }
        for (int sweep = 0; sweep < sweeps; ++sweep)
        {
            SweepBlock block;
            block.weight = start.weights.at(sweep);
            block.accelerate = block.weight > 0 ? 1 : 0;
            std::memcpy(blocks.data() + (colours + sweep) * m_slot, &block, sizeof(block));
        }
        for (int phase = 0; phase < phases.size(); ++phase)
        {
            std::memcpy(blocks.data() + (phase_slots + phase) * m_slot, &phases.at(phase), 4 * sizeof(qint32));
        }

        QRhiCommandBuffer* commands = nullptr;
        if (m_rhi->beginOffscreenFrame(&commands) != QRhi::FrameOpSuccess)
        {
            m_ready = false;
            return false;
        }
        const QByteArray none(count * number, 0);
        QRhiResourceUpdateBatch* upload = m_rhi->nextResourceUpdateBatch();
        upload->updateDynamicBuffer(m_step.get(), 0, sizeof(Step), &this_step);
        upload->updateDynamicBuffer(m_search_block.get(), 0, sizeof(Search), &this_search);
        upload->updateDynamicBuffer(m_sweeps.get(), 0, static_cast<quint32>(blocks.size()), blocks.constData());
        upload->uploadStaticBuffer(m_positions.get(), 0, bytes(start.positions), start.positions.constData());
        upload->uploadStaticBuffer(m_motion.get(), 0, bytes(start.motion), start.motion.constData());
        upload->uploadStaticBuffer(m_stopped.get(), 0, static_cast<quint32>(none.size()), none.constData());
        upload->uploadStaticBuffer(m_contacts.get(), 0, bytes(start.body), start.body.constData());
        if (!start.facing.isEmpty())
        {
            upload->uploadStaticBuffer(m_boxes.get(), 0, bytes(start.facing), start.facing.constData());
        }

        commands->beginComputePass(upload);
        commands->setComputePipeline(m_search_pipeline.get());
        for (int phase = 0; phase < phases.size(); ++phase)
        {
            const QRhiCommandBuffer::DynamicOffset offset(1, static_cast<quint32>((phase_slots + phase) * m_slot));
            commands->setShaderResources(m_search_bindings.get(), 1, &offset);
            commands->dispatch(phases.at(phase).groups, 1, 1);
        }
        for (int sweep = 0; sweep < sweeps; ++sweep)
        {
            if (start.self_contact)
            {
                commands->setComputePipeline(m_contact_pipeline.get());
                commands->setShaderResources(m_contact_bindings.get());
                commands->dispatch(groups(m_capacity), 1, 1);
            }
            commands->setComputePipeline(m_sweep_pipeline.get());
            for (int colour = 0; colour < colours; ++colour)
            {
                // A workgroup for each vertex, in rows as long as a dispatch may have.
                const int vertices = m_colour_starts.at(colour + 1) - m_colour_starts.at(colour);
                const QRhiCommandBuffer::DynamicOffset offset(1, static_cast<quint32>(colour * m_slot));
                commands->setShaderResources(m_sweep_bindings.get(), 1, &offset);
                commands->dispatch(static_cast<quint32>(qMin(vertices, workgroup_row)),
                                   static_cast<quint32>((vertices + workgroup_row - 1) / workgroup_row), 1);
            }
            commands->setComputePipeline(m_accelerate_pipeline.get());
            const QRhiCommandBuffer::DynamicOffset offset(1, static_cast<quint32>((colours + sweep) * m_slot));
            commands->setShaderResources(m_accelerate_bindings.get(), 1, &offset);
            commands->dispatch(groups(count), 1, 1);
        }
        QRhiReadbackResult swept;
        QRhiReadbackResult stopped_ones;
        QRhiReadbackResult found;
        QRhiResourceUpdateBatch* readback = m_rhi->nextResourceUpdateBatch();
        readback->readBackBuffer(m_positions.get(), 0, bytes(start.positions), &swept);
        readback->readBackBuffer(m_stopped.get(), 0, static_cast<quint32>(none.size()), &stopped_ones);
        readback->readBackBuffer(m_contacts.get(), static_cast<quint32>(this_step.self_count * number),
                                 static_cast<quint32>(number), &found);
        commands->endComputePass(readback);
        if (m_rhi->endOffscreenFrame() != QRhi::FrameOpSuccess
            || swept.data.size() != static_cast<int>(bytes(start.positions)) || stopped_ones.data.size() != none.size()
            || found.data.size() != number)
        {
            m_ready = false;
            return false;
        }

        qint32 found_count = 0;
        std::memcpy(&found_count, found.data.constData(), sizeof(found_count));
        if (search && found_count > m_capacity)
        {
            m_capacity = found_count + found_count / 2;
            continue;
        }
        m_found_kept = start.self_contact;
        *searched = search;
        positions->resize(4 * count);
        std::memcpy(positions->data(), swept.data.constData(), static_cast<size_t>(swept.data.size()));
        stopped->resize(count);
        std::memcpy(stopped->data(), stopped_ones.data.constData(), static_cast<size_t>(stopped_ones.data.size()));
        return true;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Binds the buffers to the shaders, and makes the pipelines the first time.
bool ClothCompute::createBindings()
{
    const auto stage = QRhiShaderResourceBinding::ComputeStage;
    if (m_sweep_bindings == nullptr)
    {
        m_sweep_bindings.reset(m_rhi->newShaderResourceBindings());
        m_accelerate_bindings.reset(m_rhi->newShaderResourceBindings());
        m_contact_bindings.reset(m_rhi->newShaderResourceBindings());
        m_search_bindings.reset(m_rhi->newShaderResourceBindings());
    }
    m_sweep_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, stage, m_step.get()),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, stage, m_sweeps.get(), sizeof(SweepBlock)),
        QRhiShaderResourceBinding::bufferLoadStore(2, stage, m_positions.get()),
        QRhiShaderResourceBinding::bufferLoad(3, stage, m_motion.get()),
        QRhiShaderResourceBinding::bufferLoad(4, stage, m_pushes.get()),
        QRhiShaderResourceBinding::bufferLoadStore(5, stage, m_stopped.get()),
        QRhiShaderResourceBinding::bufferLoad(6, stage, m_topology.get()),
        QRhiShaderResourceBinding::bufferLoad(7, stage, m_terms.get()),
        QRhiShaderResourceBinding::bufferLoad(8, stage, m_contacts.get())});
    m_accelerate_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, stage, m_step.get()),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, stage, m_sweeps.get(), sizeof(SweepBlock)),
        QRhiShaderResourceBinding::bufferLoadStore(2, stage, m_positions.get()),
        QRhiShaderResourceBinding::bufferLoad(3, stage, m_motion.get()),
        QRhiShaderResourceBinding::bufferLoadStore(4, stage, m_snapshot.get()),
        QRhiShaderResourceBinding::bufferLoadStore(5, stage, m_stopped.get()),
        QRhiShaderResourceBinding::bufferLoadStore(6, stage, m_earlier.get())});
    m_contact_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, stage, m_step.get()),
        QRhiShaderResourceBinding::bufferLoad(2, stage, m_positions.get()),
        QRhiShaderResourceBinding::bufferLoad(3, stage, m_motion.get()),
        QRhiShaderResourceBinding::bufferLoad(8, stage, m_contacts.get()),
        QRhiShaderResourceBinding::bufferLoadStore(9, stage, m_pushes.get())});
    m_search_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, stage, m_step.get()),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, stage, m_sweeps.get(), 4 * sizeof(qint32)),
        QRhiShaderResourceBinding::bufferLoadStore(2, stage, m_positions.get()),
        QRhiShaderResourceBinding::bufferLoadStore(3, stage, m_motion.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, stage, m_search_block.get()),
        QRhiShaderResourceBinding::bufferLoadStore(5, stage, m_snapshot.get()),
        QRhiShaderResourceBinding::bufferLoad(6, stage, m_topology.get()),
        QRhiShaderResourceBinding::bufferLoad(7, stage, m_terms.get()),
        QRhiShaderResourceBinding::bufferLoadStore(8, stage, m_contacts.get()),
        QRhiShaderResourceBinding::bufferLoadStore(10, stage, m_boxes.get()),
        QRhiShaderResourceBinding::bufferLoadStore(11, stage, m_found.get())});
    if (!m_sweep_bindings->create() || !m_accelerate_bindings->create() || !m_contact_bindings->create()
        || !m_search_bindings->create())
    {
        return false;
    }

    if (m_sweep_pipeline == nullptr)
    {
        const QShader sweep = loadShader(QStringLiteral(":/cloth/cloth_sweep.comp.qsb"));
        const QShader accelerate = loadShader(QStringLiteral(":/cloth/cloth_accelerate.comp.qsb"));
        const QShader contacts = loadShader(QStringLiteral(":/cloth/cloth_contacts.comp.qsb"));
        const QShader search = loadShader(QStringLiteral(":/cloth/cloth_search.comp.qsb"));
        if (!sweep.isValid() || !accelerate.isValid() || !contacts.isValid() || !search.isValid())
        {
            return false;
        }
        m_sweep_pipeline.reset(m_rhi->newComputePipeline());
        m_sweep_pipeline->setShaderStage({QRhiShaderStage::Compute, sweep});
        m_sweep_pipeline->setShaderResourceBindings(m_sweep_bindings.get());
        m_accelerate_pipeline.reset(m_rhi->newComputePipeline());
        m_accelerate_pipeline->setShaderStage({QRhiShaderStage::Compute, accelerate});
        m_accelerate_pipeline->setShaderResourceBindings(m_accelerate_bindings.get());
        m_contact_pipeline.reset(m_rhi->newComputePipeline());
        m_contact_pipeline->setShaderStage({QRhiShaderStage::Compute, contacts});
        m_contact_pipeline->setShaderResourceBindings(m_contact_bindings.get());
        m_search_pipeline.reset(m_rhi->newComputePipeline());
        m_search_pipeline->setShaderStage({QRhiShaderStage::Compute, search});
        m_search_pipeline->setShaderResourceBindings(m_search_bindings.get());
        if (!m_sweep_pipeline->create() || !m_accelerate_pipeline->create() || !m_contact_pipeline->create()
            || !m_search_pipeline->create())
        {
            m_sweep_pipeline.reset();
            m_accelerate_pipeline.reset();
            m_contact_pipeline.reset();
            m_search_pipeline.reset();
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Makes the buffer at least as big as asked, a storage buffer on the card or the uniform one of the sweeps; false if it
// couldn't be made. Notes in remade when it was made anew, which the bindings have to follow.
bool ClothCompute::fit(std::unique_ptr<QRhiBuffer>* buffer, int size, bool storage, bool* remade)
{
    const quint32 wanted = static_cast<quint32>(qMax(size, smallest_buffer));
    if (*buffer != nullptr && (*buffer)->size() >= wanted)
    {
        return true;
    }
    *remade = true;
    if (*buffer == nullptr)
    {
        buffer->reset(storage ? m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, wanted)
                              : m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, wanted));
    }
    else
    {
        // Room for more, so it isn't made anew every step that needs a little more.
        (*buffer)->setSize(wanted + wanted / 2);
    }
    return (*buffer)->create();
}
