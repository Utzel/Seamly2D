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

#include <QPointF>
#include <QSet>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "body_collider.h"
#include "fabric.h"
#include "garment_mesh.h"
#include "seam_stretch.h"

/// @brief How the cloth behaves, whatever its fabric. Units are cm, g and s.
struct ClothSettings
{
    qreal     stitch_stiffness = 5.0e5;        ///< of each stitch, in g/s²
    qreal     contact_stiffness = 1.0e6;       ///< of the body's surface, in g/s²
    qreal     self_contact_stiffness = 1.0e3;  ///< of cloth against cloth, in g/s²; layers caught between cloth and
                                               ///< body jitter if it is anywhere near as stiff as the body
    qreal     damping = 0.02;                  ///< of stretching and bending, in s
    qreal     air_damping = 3.0;               ///< share of the speed lost per second; cloth loses its swing to
                                               ///< the air quickly
    qreal     friction = 0.4;                  ///< against the body
    qreal     thickness = 0.3;                 ///< how far the cloth keeps from the body and from itself, in cm
    bool      self_contact = true;             ///< whether the cloth keeps from passing through itself
    bool      floor = true;                    ///< whether the cloth lands on a floor
    qreal     floor_height = 0.0;              ///< in cm
    QVector3D gravity = QVector3D(0.0f, -981.0f, 0.0f);
    int       iterations = 12;                 ///< sweeps over all vertices per step
    qreal     acceleration = 0.9;              ///< spectral radius the sweeps' Chebyshev acceleration is tuned for;
                                               ///< 0 for none
};

/// @brief Simulates sewn pieces of cloth draping over a body.
///
/// Each step is an implicit Euler step solved with Vertex Block Descent (Chen et al., SIGGRAPH 2024): the vertices
/// are coloured so no two neighbours share a colour, and each vertex in turn moves to where its own forces balance,
/// found with one Newton step on its 3 x 3 block. That stays stable with stiff cloth and large steps, which the
/// explicit methods don't.
///
/// Each piece is of a fabric, laid with its grain one way. Each triangle of it resists being stretched along the
/// grain, across it and sheared on the bias as its fabric does: an orthotropic Saint Venant-Kirchhoff membrane in the
/// fabric's warp and weft directions, as Chen et al. use for cloth, isotropic. Across each inner edge the cloth
/// resists bending with Bergou et al.'s quadratic bending (SCA 2006). Both are given in what fabric testing measures,
/// so the cloth behaves the same however finely it is meshed; only stretching stiffer than the sweeps of a step can
/// follow is taken to be as stiff as they can. Stitches pull the sides of seams together, the body pushes the cloth
/// out and holds it by friction. As in the paper, each vertex picks the body
/// triangle it may touch once per step and keeps it through the sweeps. Large colours are solved in parallel.
///
/// The cloth also keeps its thickness from itself: the parts of it that may touch, a vertex and a triangle or two
/// edges, are found again whenever the cloth has moved too far for the last ones to do, and each sweep pushes those
/// closer than the thickness apart, more softly than the body does. Parts next to each other in their flat piece or
/// across a seam don't push, or they would hold the seams open. Near other cloth, a vertex moves less than half the
/// way to it in a step, so nothing passes through (the conservative bound of Chen et al.'s Offset Geometric Contact,
/// SIGGRAPH 2025).
///
/// Stiff cloth needs more sweeps than a step can afford to settle, and without them it gives way slowly, as if it were
/// soft. Chebyshev acceleration (Wang, SIGGRAPH Asia 2015), as Chen et al. use it, carries each sweep on further
/// along the way the sweeps before went, which settles it in far fewer.
class ClothSolver
{
public:
    explicit           ClothSolver(const ClothSettings& settings = ClothSettings());

    const ClothSettings& settings() const;
    void               setGravity(const QVector3D& gravity);
    void               setFriction(qreal friction);
    void               setSelfContact(bool self_contact);

    quint32            addMesh(const GarmentMesh& mesh, const QVector<QVector3D>& positions,
                               const Fabric& fabric = Fabric(), qreal grain_angle = 90.0);
    void               addStitches(const QVector<Stitch>& stitches);
    void               setCollider(const BodyCollider& collider);
    void               setPinned(quint32 vertex, bool pinned);

    int                vertexCount() const;
    QVector<QVector3D> positions() const;
    QVector<QVector3D> velocities() const;
    qreal              widestStitch() const;

    void               step(qreal time_step);

private:
    // A triangle of cloth: its corners, its drafted area, how each corner's position makes up the deformation
    // gradient, in the fabric's weft and warp directions, and how hard it is to stretch across and along the grain
    // and to shear, in g/s².
    struct Membrane
    {
        int    vertices[3] = {0, 0, 0};
        double area = 0;
        double shape[3][2] = {{0, 0}, {0, 0}, {0, 0}};
        double weft = 0;
        double warp = 0;
        double shear = 0;
    };

    // Two triangles of cloth sharing an edge, which resist bending across it: the edge's ends, then the corners
    // opposite, how much each counts, and how stiffly, in g/s².
    struct Hinge
    {
        int    vertices[4] = {0, 0, 0, 0};
        double weights[4] = {0, 0, 0, 0};
        double stiffness = 0;
    };

    // A membrane or hinge a vertex takes part in, and as which of its corners.
    struct Role
    {
        int    term = 0;
        int    corner = 0;
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

    // Two parts of the cloth that may touch: a vertex and a triangle, the vertex first, or two edges. The side says on
    // which side of the triangle, or of the second edge, the first part was when the step started, 1 or -1; 0 if they
    // lay beside each other rather than one on the other. Parts too far apart to touch during the step aren't live.
    struct SelfContact
    {
        int    vertices[4] = {0, 0, 0, 0};
        bool   edges = false;
        double side = 0;
        bool   live = true;
    };

    // A self contact a vertex takes part in, and as which of its four vertices.
    struct SelfContactRole
    {
        int    contact = 0;
        int    role = 0;
    };

    ClothSettings      m_settings;
    QVector<double>    m_position;  // x, y and z of each vertex
    QVector<double>    m_velocity;
    QVector<double>    m_previous;
    QVector<double>    m_inertial;
    QVector<double>    m_mass;
    QVector<bool>      m_pinned;
    QVector<Membrane>  m_membranes;
    QVector<Hinge>     m_hinges;
    QVector<StitchTerm> m_stitches;
    QVector<int>       m_faces;        // three vertices per triangle of cloth
    QVector<int>       m_edges;        // two vertices per edge of cloth
    QVector<int>       m_pieces;       // which piece each vertex is of, in the order they were added
    QVector<QPointF>   m_rest;         // where each vertex is in its flat piece
    QVector<int>       m_piece_start;  // where each piece's vertices start, and where the last one's end
    BodyCollider       m_collider;

    bool               m_prepared;
    QVector<QVector<Role>>       m_vertex_membranes;
    QVector<QVector<Role>>       m_vertex_hinges;
    QVector<QVector<StitchRole>> m_vertex_stitches;
    QVector<QVector<int>>        m_colors;
    QVector<QVector<int>>        m_contacts;
    QVector<double>              m_contact_origin;
    QVector<int>                 m_contact_triangle;
    QVector<QVector<int>>        m_stitched;        // the vertices each vertex is stitched to, sorted
    QSet<quint64>                m_seam_neighbours; // vertices next to each other across a seam
    QVector<SelfContact>         m_self_contacts;
    QVector<int>                 m_self_role_start; // where each vertex's roles start in m_self_roles, and the end
    QVector<SelfContactRole>     m_self_roles;
    QVector<double>              m_self_bound;      // how far each vertex may move in the step, cloth being near
    QVector<char>                m_stopped;         // whether each vertex was stopped by the bound
    QVector<double>              m_self_found_at;   // where the vertices were when the self contacts were found
    QVector<double>              m_snapshot;        // where the vertices were when a colour started moving at once

    void               prepare();
    void               findContacts();
    void               findSelfContacts();
    void               findSelfContactsAround(const QVector<double>& heading);
    void               addSelfContact(const SelfContact& contact, double furthest);
    void               startSelfContact(SelfContact* contact, const QVector<double>& heading);
    void               solveVertex(int vertex, double time_step, const QVector<double>& others);
    double             bodyReach(int vertex) const;
    bool               sewnTogether(int a, int b) const;
    bool               closeAtRest(const int* first, int first_count, const int* second, int second_count) const;
};

#endif // CLOTH_SOLVER_H
