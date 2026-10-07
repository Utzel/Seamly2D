//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_fit.h
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

#ifndef GARMENT_FIT_H
#define GARMENT_FIT_H

#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "body_collider.h"
#include "cloth_solver.h"
#include "garment_mesh.h"

/// @brief How a garment on the body fits it, vertex by vertex, besides how much its cloth is stretched
/// (GarmentMesh::strain()): how far the cloth stands off the body, its ease, and how hard it presses on the body.
///
/// The cloth rests on the body its thickness off the skin, so ease is measured from there: 0 where the cloth touches.
/// The pressure is what the drape simulation's body contact pushes back with, the cloth's thickness in, over the cloth
/// around each vertex and two rings of its neighbours, as a pressure sensor takes it over its pad, in kPa.
class GarmentFit
{
public:
    static qreal          farthestEase();

    static QVector<qreal> ease(const BodyCollider& body, const QVector<QVector3D>& positions,
                               const ClothSettings& settings = ClothSettings());
    static QVector<qreal> pressure(const GarmentMesh& mesh, const BodyCollider& body,
                                   const QVector<QVector3D>& positions,
                                   const ClothSettings& settings = ClothSettings());
};

#endif // GARMENT_FIT_H
