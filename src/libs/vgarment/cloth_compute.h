//---------------------------------------------------------------------------------------------------------------------
//  @file   cloth_compute.h
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

#ifndef CLOTH_COMPUTE_H
#define CLOTH_COMPUTE_H

#include <QVector>
#include <QtGlobal>

#include <memory>

class QRhi;
class QRhiBuffer;
class QRhiComputePipeline;
class QRhiShaderResourceBindings;

/// @brief Runs the cloth solver's sweeps on the graphics card.
///
/// ClothSolver packs the cloth the way the compute shaders read it (shaders/cloth_sweep.comp says how) and hands over
/// each step's start; the sweeps then run on the card, each working out how the cloth pushes itself apart first, then
/// moving the colours in turn and carrying the vertices on by its Chebyshev acceleration, and the vertices come back
/// where the sweeps left them. Works on whatever device it is given, on the thread that opened it.
class ClothCompute
{
public:
    /// @brief What holds for a whole step, laid out as the shaders' Step block: the physics, then where the parts of
    /// the packed buffers start.
    struct Step
    {
        float  time_step = 0;
        float  damping = 0;
        float  stitch_stiffness = 0;
        float  contact_stiffness = 0;
        float  self_contact_stiffness = 0;
        float  friction = 0;
        float  thickness = 0;
        float  floor_height = 0;
        qint32 has_floor = 0;
        qint32 has_body = 0;
        float  contact_margin = 0;
        float  max_move = 0;
        float  friction_rest = 0;
        qint32 vertex_count = 0;
        qint32 vertex_terms = 0;
        qint32 membrane_corners = 0;
        qint32 hinge_corners = 0;
        qint32 stitch_corners = 0;
        qint32 hinges = 0;
        qint32 stitches = 0;
        qint32 body = 0;
        qint32 self_starts = 0;
        qint32 self_roles = 0;
        qint32 self_contacts = 0;
        qint32 self_contact_count = 0;
        qint32 padding[3] = {0, 0, 0};  // to whole vectors, as Direct3D has uniform blocks
    };

    /// @brief What stays the same from step to step: the topology, the colours' vertices first, the terms, four
    /// numbers to a vector, and where each colour starts in the topology, and where the last one ends.
    struct Cloth
    {
        qint32          vertex_count = 0;
        QVector<qint32> topology;
        QVector<float>  terms;
        QVector<qint32> colour_starts;
    };

    /// @brief Each step's start: four numbers for each vertex where it starts the sweeps, eight where it started the
    /// step and would go by itself, the contacts, and the Chebyshev weight of each sweep, 0 for none.
    struct Start
    {
        QVector<float>  positions;
        QVector<float>  motion;
        QVector<qint32> contacts;
        QVector<float>  weights;
    };

    explicit           ClothCompute(QRhi* rhi);
                       ~ClothCompute();

    bool               isReady() const;
    bool               setCloth(const Cloth& cloth);
    bool               sweep(const Step& step, const Start& start, QVector<float>* positions,
                             QVector<qint32>* stopped);

private:
    Q_DISABLE_COPY(ClothCompute)

    QRhi*                                       m_rhi;
    std::unique_ptr<QRhiBuffer>                 m_step;
    std::unique_ptr<QRhiBuffer>                 m_sweeps;
    std::unique_ptr<QRhiBuffer>                 m_positions;
    std::unique_ptr<QRhiBuffer>                 m_motion;
    std::unique_ptr<QRhiBuffer>                 m_snapshot;
    std::unique_ptr<QRhiBuffer>                 m_earlier;
    std::unique_ptr<QRhiBuffer>                 m_stopped;
    std::unique_ptr<QRhiBuffer>                 m_topology;
    std::unique_ptr<QRhiBuffer>                 m_terms;
    std::unique_ptr<QRhiBuffer>                 m_contacts;
    std::unique_ptr<QRhiBuffer>                 m_pushes;
    std::unique_ptr<QRhiShaderResourceBindings> m_sweep_bindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_accelerate_bindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_contact_bindings;
    std::unique_ptr<QRhiComputePipeline>        m_sweep_pipeline;
    std::unique_ptr<QRhiComputePipeline>        m_accelerate_pipeline;
    std::unique_ptr<QRhiComputePipeline>        m_contact_pipeline;
    QVector<qint32>                             m_colour_starts;
    qint32                                      m_vertex_count;
    int                                         m_slot;  // bytes from one Sweep block to the next
    bool                                        m_ready;

    bool               createBindings();
    bool               fit(std::unique_ptr<QRhiBuffer>* buffer, int size, bool storage, bool* remade);
};

#endif // CLOTH_COMPUTE_H
