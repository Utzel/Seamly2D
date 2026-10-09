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

/// @brief Runs the cloth solver's sweeps on the graphics card, and finds where the cloth may touch itself there.
///
/// ClothSolver packs the cloth the way the compute shaders read it (shaders/cloth_sweep.comp and
/// shaders/cloth_search.comp say how) and hands over each step's start. On the card, the cloth's parts that may touch
/// each other are found when the step asks for it, and kept there for the steps after; each step starts them, then
/// sweeps, each working out how the cloth pushes itself apart first, then moving the colours in turn and carrying the
/// vertices on by its Chebyshev acceleration, and the vertices come back where the sweeps left them. Works on whatever
/// device it is given, on the thread that opened it.
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
        qint32 self_capacity = 0;
        qint32 seam_folds = 1;
        qint32 self_count = 0;
        qint32 padding = 0;          // to whole vectors, as Direct3D has uniform blocks
    };

    /// @brief How the cloth's parts that may touch each other are found, laid out as cloth_search.comp's Search block:
    /// the clusters, where the search's lists start in the topology and the terms, then where its own parts of its
    /// buffers start, which it works out itself, whether pieces pass through each other or are worn in layers, which
    /// each step says, and what the solver's search goes by.
    struct Search
    {
        qint32 vertex_clusters = 0;
        qint32 face_clusters = 0;
        qint32 edge_clusters = 0;
        qint32 cluster_members = 0;
        qint32 faces = 0;
        qint32 edges = 0;
        qint32 vertex_pieces = 0;
        qint32 pieces = 0;
        qint32 stitched = 0;
        qint32 seam_neighbours = 0;
        qint32 rest = 0;
        qint32 boxes = 0;
        qint32 cluster_boxes = 0;
        qint32 pairs = 0;
        qint32 face_words = 0;
        qint32 edge_words = 0;
        qint32 found = 0;
        qint32 block_sums = 0;
        qint32 cursors = 0;
        qint32 bounds = 0;
        qint32 pass_through = 0;
        qint32 in_layers = 0;
        float  self_margin = 0;
        float  reach_limit = 0;
        float  rest_neighbours = 0;
        float  lying_on = 0;
        float  bound_share = 0;
        float  same_way = 0;
    };

    /// @brief What stays the same from step to step: the topology, the colours' vertices first, the terms, four
    /// numbers to a vector, where each colour starts in the topology, and where the last one ends, and how the search
    /// finds its way through them.
    struct Cloth
    {
        qint32          vertex_count = 0;
        QVector<qint32> topology;
        QVector<float>  terms;
        QVector<qint32> colour_starts;
        Search          search;
    };

    /// @brief Each step's start: four numbers for each vertex where it is first guessed to go, eight where it started
    /// the step and would go by itself, the body triangle each may touch, and the Chebyshev weight of each sweep, 0 for
    /// none. Whether the cloth keeps from passing through itself, whether the parts that may touch each other are to be
    /// found again, and whether pieces pass through each other; of pieces in layers, which way the cloth's outside
    /// faces around each vertex, four numbers each.
    struct Start
    {
        QVector<float>  positions;
        QVector<float>  motion;
        QVector<qint32> body;
        QVector<float>  weights;
        bool            self_contact = false;
        bool            search = false;
        bool            pass_through = false;
        QVector<float>  facing;
    };

    explicit           ClothCompute(QRhi* rhi);
                       ~ClothCompute();

    bool               isReady() const;
    bool               setCloth(const Cloth& cloth);
    bool               sweep(const Step& step, const Start& start, QVector<float>* positions,
                             QVector<qint32>* stopped, bool* searched);

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
    std::unique_ptr<QRhiBuffer>                 m_search_block;
    std::unique_ptr<QRhiBuffer>                 m_boxes;
    std::unique_ptr<QRhiBuffer>                 m_found;
    std::unique_ptr<QRhiShaderResourceBindings> m_sweep_bindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_accelerate_bindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_contact_bindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_search_bindings;
    std::unique_ptr<QRhiComputePipeline>        m_sweep_pipeline;
    std::unique_ptr<QRhiComputePipeline>        m_accelerate_pipeline;
    std::unique_ptr<QRhiComputePipeline>        m_contact_pipeline;
    std::unique_ptr<QRhiComputePipeline>        m_search_pipeline;
    QVector<qint32>                             m_colour_starts;
    Search                                      m_search;
    qint32                                      m_vertex_count;
    int                                         m_slot;        // bytes from one Sweep or Phase block to the next
    int                                         m_capacity;    // how many self contacts there is room for
    bool                                        m_found_kept;  // whether the self contacts found are on the card
    bool                                        m_ready;

    bool               createBindings();
    bool               fit(std::unique_ptr<QRhiBuffer>* buffer, int size, bool storage, bool* remade);
};

#endif // CLOTH_COMPUTE_H
