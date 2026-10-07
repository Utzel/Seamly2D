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
    , m_sweep_bindings()
    , m_accelerate_bindings()
    , m_contact_bindings()
    , m_sweep_pipeline()
    , m_accelerate_pipeline()
    , m_contact_pipeline()
    , m_colour_starts()
    , m_vertex_count(0)
    , m_slot(0)
    , m_ready(false)
{
    initClothShaders();
    if (m_rhi != nullptr && m_rhi->isFeatureSupported(QRhi::Compute))
    {
        m_slot = m_rhi->ubufAligned(static_cast<int>(sizeof(SweepBlock)));
        m_step.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Step)));
        m_ready = m_step->create();
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

    const int vector = 4 * static_cast<int>(sizeof(float));
    const int count = cloth.vertex_count;
    bool remade = false;
    m_ready = fit(&m_positions, count * vector, true, &remade) && fit(&m_motion, 2 * count * vector, true, &remade)
              && fit(&m_snapshot, count * vector, true, &remade) && fit(&m_earlier, count * vector, true, &remade)
              && fit(&m_stopped, count * static_cast<int>(sizeof(qint32)), true, &remade)
              && fit(&m_topology, static_cast<int>(bytes(cloth.topology)), true, &remade)
              && fit(&m_terms, static_cast<int>(bytes(cloth.terms)), true, &remade)
              && fit(&m_contacts, (2 * count + 1) * static_cast<int>(sizeof(qint32)), true, &remade)
              && fit(&m_pushes, 0, true, &remade)
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
/// @brief Runs a step's sweeps from its start, and gives back where the vertices went, four numbers each, and which
/// the cloth nearby stopped. False if the card failed, and then it computes no more.
bool ClothCompute::sweep(const Step& step, const Start& start, QVector<float>* positions, QVector<qint32>* stopped)
{
    const int count = m_vertex_count;
    const int colours = static_cast<int>(m_colour_starts.size()) - 1;
    const int sweeps = static_cast<int>(start.weights.size());
    if (!m_ready || step.vertex_count != count || start.positions.size() != 4 * count
        || start.motion.size() != 8 * count)
    {
        return false;
    }

    // The Sweep blocks: one for each colour, then one for each sweep's acceleration, each in its own slot.
    const int slot_count = colours + sweeps;
    bool remade = false;
    const int push_size = 36 * step.self_contact_count * static_cast<int>(sizeof(float));
    if (!fit(&m_contacts, static_cast<int>(bytes(start.contacts)), true, &remade)
        || !fit(&m_pushes, push_size, true, &remade) || !fit(&m_sweeps, slot_count * m_slot, false, &remade)
        || (remade && !createBindings()))
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

    QRhiCommandBuffer* commands = nullptr;
    if (m_rhi->beginOffscreenFrame(&commands) != QRhi::FrameOpSuccess)
    {
        m_ready = false;
        return false;
    }
    const QByteArray none(count * static_cast<int>(sizeof(qint32)), 0);
    QRhiResourceUpdateBatch* upload = m_rhi->nextResourceUpdateBatch();
    upload->updateDynamicBuffer(m_step.get(), 0, sizeof(Step), &step);
    upload->updateDynamicBuffer(m_sweeps.get(), 0, static_cast<quint32>(blocks.size()), blocks.constData());
    upload->uploadStaticBuffer(m_positions.get(), 0, bytes(start.positions), start.positions.constData());
    upload->uploadStaticBuffer(m_snapshot.get(), 0, bytes(start.positions), start.positions.constData());
    upload->uploadStaticBuffer(m_motion.get(), 0, bytes(start.motion), start.motion.constData());
    upload->uploadStaticBuffer(m_stopped.get(), 0, static_cast<quint32>(none.size()), none.constData());
    if (!start.contacts.isEmpty())
    {
        upload->uploadStaticBuffer(m_contacts.get(), 0, bytes(start.contacts), start.contacts.constData());
    }

    commands->beginComputePass(upload);
    for (int sweep = 0; sweep < sweeps; ++sweep)
    {
        if (step.self_contact_count > 0)
        {
            commands->setComputePipeline(m_contact_pipeline.get());
            commands->setShaderResources(m_contact_bindings.get());
            commands->dispatch(groups(step.self_contact_count), 1, 1);
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
    QRhiResourceUpdateBatch* readback = m_rhi->nextResourceUpdateBatch();
    readback->readBackBuffer(m_positions.get(), 0, bytes(start.positions), &swept);
    readback->readBackBuffer(m_stopped.get(), 0, static_cast<quint32>(none.size()), &stopped_ones);
    commands->endComputePass(readback);
    if (m_rhi->endOffscreenFrame() != QRhi::FrameOpSuccess
        || swept.data.size() != static_cast<int>(bytes(start.positions)) || stopped_ones.data.size() != none.size())
    {
        m_ready = false;
        return false;
    }

    positions->resize(4 * count);
    std::memcpy(positions->data(), swept.data.constData(), static_cast<size_t>(swept.data.size()));
    stopped->resize(count);
    std::memcpy(stopped->data(), stopped_ones.data.constData(), static_cast<size_t>(stopped_ones.data.size()));
    return true;
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
    if (!m_sweep_bindings->create() || !m_accelerate_bindings->create() || !m_contact_bindings->create())
    {
        return false;
    }

    if (m_sweep_pipeline == nullptr)
    {
        const QShader sweep = loadShader(QStringLiteral(":/cloth/cloth_sweep.comp.qsb"));
        const QShader accelerate = loadShader(QStringLiteral(":/cloth/cloth_accelerate.comp.qsb"));
        const QShader contacts = loadShader(QStringLiteral(":/cloth/cloth_contacts.comp.qsb"));
        if (!sweep.isValid() || !accelerate.isValid() || !contacts.isValid())
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
        if (!m_sweep_pipeline->create() || !m_accelerate_pipeline->create() || !m_contact_pipeline->create())
        {
            m_sweep_pipeline.reset();
            m_accelerate_pipeline.reset();
            m_contact_pipeline.reset();
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
