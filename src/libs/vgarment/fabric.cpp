//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric.cpp
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

#include "fabric.h"

namespace
{
// The fabric patterns drape in unless they say otherwise.
const QString default_name = QStringLiteral("cottonShirting");

//---------------------------------------------------------------------------------------------------------------------
Fabric makeFabric(const QString& name, qreal weight, qreal warp, qreal weft, qreal bias, qreal bending)
{
    Fabric fabric;
    fabric.name = name;
    fabric.weight = weight;
    fabric.warp_stiffness = warp;
    fabric.weft_stiffness = weft;
    fabric.bias_stiffness = bias;
    fabric.bending = bending;
    return fabric;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief How hard it is to shear the fabric, turning its threads against each other, in N/m. Stretching on the bias
/// shears it: without crosswise contraction, 4 / bias = 1 / warp + 1 / weft + 1 / shear. Shearing is never taken to
/// be harder than stretching across the grain, which a bias too stiff for the warp and weft would ask for.
qreal Fabric::shearStiffness() const
{
    const qreal room = 4.0 / bias_stiffness - 1.0 / warp_stiffness - 1.0 / weft_stiffness;
    return room > 1.0 / weft_stiffness ? 1.0 / room : weft_stiffness;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fabrics the 3D View offers, the default first.
QVector<Fabric> Fabric::presets()
{
    return {makeFabric(default_name, 120.0, 2000.0, 1200.0, 150.0, 8.0),
            makeFabric(QStringLiteral("cottonJersey"), 170.0, 80.0, 40.0, 50.0, 3.0),
            makeFabric(QStringLiteral("denim"), 400.0, 4000.0, 2500.0, 300.0, 60.0),
            makeFabric(QStringLiteral("woolSuiting"), 260.0, 1500.0, 1100.0, 120.0, 15.0),
            makeFabric(QStringLiteral("chiffon"), 50.0, 600.0, 450.0, 40.0, 0.5)};
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The preset of that name, or the default fabric for a name it doesn't know.
Fabric Fabric::preset(const QString& name)
{
    const QVector<Fabric> fabrics = presets();
    for (const Fabric& fabric : fabrics)
    {
        if (fabric.name == name)
        {
            return fabric;
        }
    }
    return fabrics.first();
}

//---------------------------------------------------------------------------------------------------------------------
QString Fabric::defaultName()
{
    return default_name;
}
