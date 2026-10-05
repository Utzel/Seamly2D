//---------------------------------------------------------------------------------------------------------------------
//  @file   cloth_solver.h
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

#ifndef CLOTH_SOLVER_H
#define CLOTH_SOLVER_H

#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "body_collider.h"
#include "garment_mesh.h"
#include "seam_stretch.h"

/// @brief How the cloth behaves. Units are cm, g and s.
struct ClothSettings
{
    qreal     density = 0.015;            ///< fabric weight in g per square cm; a light cotton
    qreal     stretch_stiffness = 2.0e4;  ///< of each mesh edge, in g/s²
    qreal     bend_stiffness = 30.0;      ///< across each mesh edge, in g/s²
    qreal     stitch_stiffness = 5.0e4;   ///< of each stitch, in g/s²; seams hold like the cloth
    qreal     contact_stiffness = 1.0e5;  ///< of the body's surface, in g/s²
    qreal     damping = 0.02;             ///< of stretching and bending, in s
    qreal     air_damping = 1.0;          ///< share of the speed lost per second
    qreal     friction = 0.4;             ///< against the body
    qreal     thickness = 0.3;            ///< how far the cloth keeps from the body, in cm
    bool      floor = true;               ///< whether the cloth lands on a floor
    qreal     floor_height = 0.0;         ///< in cm
    QVector3D gravity = QVector3D(0.0f, -981.0f, 0.0f);
    int       iterations = 12;            ///< sweeps over all vertices per step
};

/// @brief Simulates sewn pieces of cloth draping over a body.
///
/// Each step is an implicit Euler step solved with Vertex Block Descent (Chen et al., SIGGRAPH 2024): the vertices
/// are coloured so no two neighbours share a colour, and each vertex in turn moves to where its own forces balance,
/// found with one Newton step on its 3 x 3 block. That stays stable with stiff cloth and large steps, which the
/// explicit methods don't.
///
/// The cloth resists stretching along its mesh edges and bending across them. Stitches pull the sides of seams
/// together, the body pushes the cloth out and holds it by friction. As in the paper, each vertex picks the body
/// triangle it may touch once per step and keeps it through the sweeps. Large colours are solved in parallel.
class ClothSolver
{
public:
    explicit           ClothSolver(const ClothSettings& settings = ClothSettings());

    const ClothSettings& settings() const;
    void               setGravity(const QVector3D& gravity);
    void               setFriction(qreal friction);

    quint32            addMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions);
    void               addStitches(const QVector<Stitch>& stitches);
    void               setCollider(const BodyCollider& collider);
    void               setPinned(quint32 vertex, bool pinned);

    int                vertexCount() const;
    QVector<QVector3D> positions() const;
    QVector<QVector3D> velocities() const;
    qreal              widestStitch() const;

    void               step(qreal time_step);

private:
    struct Spring
    {
        int    a = 0;
        int    b = 0;
        double rest_length = 0;
        double stiffness = 0;
    };

    struct StitchTerm
    {
        int    vertex = 0;
        int    edge_start = 0;
        int    edge_end = 0;
        double along = 0;
    };

    // A stitch a vertex takes part in, and as which of its three vertices.
    struct StitchRole
    {
        int    stitch = 0;
        double weight = 0;
    };

    ClothSettings      m_settings;
    QVector<double>    m_position;  // x, y and z of each vertex
    QVector<double>    m_velocity;
    QVector<double>    m_last_velocity;
    QVector<double>    m_previous;
    QVector<double>    m_inertial;
    QVector<double>    m_mass;
    QVector<bool>      m_pinned;
    QVector<Spring>    m_springs;
    QVector<StitchTerm> m_stitches;
    BodyCollider       m_collider;

    bool               m_prepared;
    bool               m_stepped;
    QVector<QVector<int>>        m_vertex_springs;
    QVector<QVector<StitchRole>> m_vertex_stitches;
    QVector<QVector<int>>        m_colors;
    QVector<QVector<int>>        m_contacts;
    QVector<double>              m_contact_origin;
    QVector<int>                 m_contact_triangle;

    void               prepare();
    void               findContacts();
    void               solveVertex(int vertex, double time_step);
};

#endif // CLOTH_SOLVER_H
