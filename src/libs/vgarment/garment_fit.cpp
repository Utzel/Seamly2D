//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_fit.cpp
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

#include "garment_fit.h"

#include <QPair>

#include <algorithm>

namespace
{
// Ease is measured up to this far, in cm; cloth further off the body has this much. The body is searched in steps
// that widen out to it, so the cloth close to the body, most of it, is quick to measure.
const qreal farthest_ease = 10.0;
const qreal ease_steps[] = {1.5, 4.0, farthest_ease};

// Pressure comes out in dyn per square cm, the solver's units; one is 0.1 Pa.
const qreal kilopascals_per_unit = 1.0e-4;

// Pressure is taken over the cloth around each vertex and this many rings of its neighbours, as a pressure sensor
// takes it over its pad: vertex by vertex, how a coarse mesh lies over the curved body shows as speckles.
const int pressure_rings = 2;

//---------------------------------------------------------------------------------------------------------------------
// Each vertex's value with its neighbours' added, along the mesh's edges.
QVector<qreal> withNeighbours(const QVector<qreal>& values, const QVector<QPair<int, int>>& edges)
{
    QVector<qreal> summed = values;
    for (const QPair<int, int>& edge : edges)
    {
        summed[edge.first] += values.at(edge.second);
        summed[edge.second] += values.at(edge.first);
    }
    return summed;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief The most ease measured, in cm; cloth standing further off the body has this much.
qreal GarmentFit::farthestEase()
{
    return farthest_ease;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How far each vertex at the given positions is from resting on the body, in cm: its distance from the skin
/// less the cloth's thickness, 0 where it touches or is inside, at most farthestEase(). Without a body, the most.
QVector<qreal> GarmentFit::ease(const BodyCollider& body, const QVector<QVector3D>& positions,
                                const ClothSettings& settings)
{
    QVector<qreal> ease(positions.size(), farthest_ease);
    if (body.isEmpty())
    {
        return ease;
    }

    for (int i = 0; i < positions.size(); ++i)
    {
        const QVector3D& position = positions.at(i);
        for (const qreal step : ease_steps)
        {
            const float reach = static_cast<float>(settings.thickness + step);
            BodyContact contact;
            if (body.closest(position, body.trianglesNear(position, reach), &contact) && contact.distance <= reach)
            {
                ease[i] = qBound(0.0, static_cast<qreal>(contact.distance) - settings.thickness, farthest_ease);
                break;
            }
        }
    }
    return ease;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How hard the cloth around each vertex of the mesh at the given positions presses on the body, in kPa: the
/// body's push back where the cloth comes closer than its thickness, as the drape simulation has it, over the draped
/// cloth around the vertex and its neighbours. 0 where none of it touches, and everywhere without a body or positions
/// for another mesh.
QVector<qreal> GarmentFit::pressure(const GarmentMesh& mesh, const BodyCollider& body,
                                    const QVector<QVector3D>& positions, const ClothSettings& settings)
{
    const int count = mesh.vertexCount();
    QVector<qreal> pressure(count, 0.0);
    if (body.isEmpty() || positions.size() != count)
    {
        return pressure;
    }

    // The draped cloth around each vertex, a third of each triangle it is a corner of, and the edges between vertices.
    QVector<qreal> area(count, 0.0);
    QVector<QPair<int, int>> edges;
    edges.reserve(mesh.indices.size());
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const int corners[3] = {static_cast<int>(mesh.indices.at(i)), static_cast<int>(mesh.indices.at(i + 1)),
                                static_cast<int>(mesh.indices.at(i + 2))};
        const qreal third = QVector3D::crossProduct(positions.at(corners[1]) - positions.at(corners[0]),
                                                    positions.at(corners[2]) - positions.at(corners[0])).length() / 6.0;
        for (int k = 0; k < 3; ++k)
        {
            area[corners[k]] += third;
            edges.append(qMakePair(qMin(corners[k], corners[(k + 1) % 3]), qMax(corners[k], corners[(k + 1) % 3])));
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    // The body's push back on each vertex, in dyn.
    QVector<qreal> push(count, 0.0);
    const float thickness = static_cast<float>(settings.thickness);
    for (int i = 0; i < count; ++i)
    {
        const QVector3D& position = positions.at(i);
        BodyContact contact;
        if (body.closest(position, body.trianglesNear(position, thickness), &contact) && contact.distance < thickness)
        {
            push[i] = settings.contact_stiffness * (settings.thickness - contact.distance);
        }
    }

    for (int ring = 0; ring < pressure_rings; ++ring)
    {
        push = withNeighbours(push, edges);
        area = withNeighbours(area, edges);
    }
    for (int i = 0; i < count; ++i)
    {
        pressure[i] = area.at(i) > 0 ? push.at(i) / area.at(i) * kilopascals_per_unit : 0.0;
    }
    return pressure;
}
