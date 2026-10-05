//---------------------------------------------------------------------------------------------------------------------
//  @file   body_fitter.h
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

#ifndef BODY_FITTER_H
#define BODY_FITTER_H

#include <QtGlobal>

#include "body_measurer.h"
#include "body_model.h"

/// @brief A fitted body and what it actually measures, which can differ from what was asked for when the
/// measurements are beyond what the body model can do.
struct BodyFit
{
    BodyShape        shape;
    BodyMeasurements measured;
};

/// @brief Finds the body shape that matches a set of measurements.
///
/// The uniform scale sets the height. The overall build (MakeHuman's weight) is chosen to match bust, waist and hip
/// together, then MakeHuman's measure targets correct each girth on its own. Gender and age are taken as given.
/// Measurements that are 0 are left free.
class BodyFitter
{
public:
    explicit           BodyFitter(const BodyModel& model);

    BodyFit            fit(const BodyMeasurements& wanted, qreal gender, qreal age) const;

private:
    const BodyModel&   m_model;
    BodyMeasurer       m_measurer;

    qreal              scaleFor(const BodyShape& shape, const BodyMeasurements& wanted) const;
};

#endif // BODY_FITTER_H
