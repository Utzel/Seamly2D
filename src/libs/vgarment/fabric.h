//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric.h
//  @author Julius
//  @date   6 Oct, 2026
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

#ifndef FABRIC_H
#define FABRIC_H

#include <QString>
#include <QVector>
#include <QtGlobal>

/// @brief A fabric as fabric testing describes it: how heavy it is, how much it resists being stretched along its
/// grain, across it and on the bias, and how stiffly it bends along its grain and across it; and, as CLO's shrinkage,
/// the share of its drafted size the cloth wants to be across the grain and along it: less than 1 shrinks it, as a rib
/// knit worn snug, more stretches it out.
///
/// Woven fabrics barely stretch along or across their grain but give on the bias, where the threads only have to
/// turn; knits give in every direction. The presets are typical values, in the ranges fabric tests such as KES-F
/// measure; their names are what patterns store, the 3D View shows them translated.
struct Fabric
{
    QString name;                    ///< what patterns store, as the presets are named
    qreal   weight = 120.0;          ///< in g per square metre
    qreal   warp_stiffness = 2000.0; ///< how hard it is to stretch along the grain, in N/m
    qreal   weft_stiffness = 1200.0; ///< across the grain, in N/m
    qreal   bias_stiffness = 150.0;  ///< at 45 degrees to the grain, in N/m
    qreal   bending_warp = 8.0;      ///< bending rigidity curving the grain, in micro newton metres
    qreal   bending_weft = 8.0;      ///< curving across the grain
    qreal   thickness = 0.3;         ///< how thick it is, in mm, as the 3D View draws it
    qreal   shrinkage_weft = 1.0;    ///< the share of its drafted size it wants to be across the grain
    qreal   shrinkage_warp = 1.0;    ///< along the grain

    qreal                  shearStiffness() const;

    static QVector<Fabric> presets();
    static Fabric          preset(const QString& name);
    static QString         defaultName();
};

#endif // FABRIC_H
