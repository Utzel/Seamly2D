//---------------------------------------------------------------------------------------------------------------------
//  @file   body_data.h
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

#ifndef BODY_DATA_H
#define BODY_DATA_H

#include <QHash>
#include <QSharedPointer>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

/// @brief One shape target: offsets of some vertices from the base mesh, in cm.
struct BodyTarget
{
    QVector<quint32>   indices;
    QVector<QVector3D> offsets;
};

/// @brief The MakeHuman body the avatar is made from: base mesh, joints and shape targets.
///
/// The data is the CC0 MakeHuman base mesh and targets, converted by scripts/avatar/build_avatar_data.py into
/// share/avatar/body.dat. Positions are in cm with y up and the figure facing +z.
struct BodyData
{
    QVector<QVector3D>               base_positions;        ///< skin vertices first, then the joint helpers
    int                              skin_vertex_count = 0;
    QVector<quint32>                 triangles;             ///< skin only, counter-clockwise seen from outside
    QHash<QString, QVector<quint32>> joints;                ///< helper vertices around each joint, by joint name
    QHash<QString, BodyTarget>       targets;               ///< by name, e.g. "torso/measure-waist-circ-incr"

    static QSharedPointer<const BodyData> load(const QString& path);
    static QSharedPointer<const BodyData> standard();
};

#endif // BODY_DATA_H
