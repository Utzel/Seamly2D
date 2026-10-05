//---------------------------------------------------------------------------------------------------------------------
//  @file   body_fitter.cpp
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

#include "body_fitter.h"

#include <QString>
#include <QVector>
#include <QtMath>

namespace
{
// A girth the fitting corrects: how to measure it, what it should be, and the measure target that changes it.
struct GirthControl
{
    qreal   (BodyMeasurer::*measure)(const QVector<QVector3D>&) const;
    qreal   wanted;
    QString target;
    bool    sets_build;   ///< takes part in choosing the overall build
};

// Golden section steps for the build: the interval shrinks to 0.618^14, about 0.001.
const int build_search_steps = 14;

// Rounds of correcting each girth with its measure target.
const int correction_rounds = 3;

// How far a measure target is moved to see how much its girth changes.
const qreal slope_probe = 0.25;
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
BodyFitter::BodyFitter(const BodyModel& model)
    : m_model(model)
    , m_measurer(model)
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Fits a body of the given gender (0 female, 1 male) and age (MakeHuman's value, see
/// BodyShape::ageFromYears()) to the wanted measurements in cm.
BodyFit BodyFitter::fit(const BodyMeasurements& wanted, qreal gender, qreal age) const
{
    BodyShape shape;
    shape.gender = gender;
    shape.age = age;

    QVector<GirthControl> girths;
    const GirthControl all_girths[] = {
        {&BodyMeasurer::bust, wanted.bust, QStringLiteral("torso/measure-bust-circ"), true},
        {&BodyMeasurer::waist, wanted.waist, QStringLiteral("torso/measure-waist-circ"), true},
        {&BodyMeasurer::hip, wanted.hip, QStringLiteral("torso/measure-hips-circ"), true},
        {&BodyMeasurer::neck, wanted.neck, QStringLiteral("neck/measure-neck-circ"), false}};
    for (const GirthControl& girth : all_girths)
    {
        if (girth.wanted > 0)
        {
            girths.append(girth);
        }
    }

    // Girths grow with the scale, so a body is built at scale 1 and its girths scaled to the wanted height.
    auto scaled_girth = [this, &wanted](const GirthControl& girth, const QVector<QVector3D>& positions)
    {
        const qreal scale = wanted.height > 0 ? wanted.height / m_measurer.height(positions) : 1.0;
        return (m_measurer.*girth.measure)(positions) * scale;
    };

    // The overall build: the weight that best matches bust, waist and hip together.
    auto build_error = [this, &shape, &girths, &scaled_girth](qreal weight)
    {
        BodyShape candidate = shape;
        candidate.weight = weight;
        const QVector<QVector3D> positions = m_model.evaluate(candidate);
        qreal error = 0;
        for (const GirthControl& girth : girths)
        {
            if (girth.sets_build)
            {
                const qreal difference = scaled_girth(girth, positions) - girth.wanted;
                error += difference * difference;
            }
        }
        return error;
    };

    bool any_build_girth = false;
    for (const GirthControl& girth : girths)
    {
        any_build_girth = any_build_girth || girth.sets_build;
    }
    if (any_build_girth)
    {
        const qreal golden = (qSqrt(5.0) - 1.0) / 2.0;
        qreal low = 0.0;
        qreal high = 1.0;
        qreal left = high - golden * (high - low);
        qreal right = low + golden * (high - low);
        qreal left_error = build_error(left);
        qreal right_error = build_error(right);
        for (int step = 0; step < build_search_steps; ++step)
        {
            if (left_error < right_error)
            {
                high = right;
                right = left;
                right_error = left_error;
                left = high - golden * (high - low);
                left_error = build_error(left);
            }
            else
            {
                low = left;
                left = right;
                left_error = right_error;
                right = low + golden * (high - low);
                right_error = build_error(right);
            }
        }
        shape.weight = (low + high) / 2.0;
    }

    // Each girth on its own: a secant step per round, with the slope taken once at the start.
    QVector<qreal> slopes(girths.size(), 0.0);
    for (int round = 0; round < correction_rounds; ++round)
    {
        for (int i = 0; i < girths.size(); ++i)
        {
            const GirthControl& girth = girths.at(i);
            const qreal value = shape.measures.value(girth.target, 0.0);
            const qreal current = scaled_girth(girth, m_model.evaluate(shape));

            if (round == 0)
            {
                BodyShape probe = shape;
                probe.measures[girth.target] = value + (value > 0 ? -slope_probe : slope_probe);
                const qreal probed = scaled_girth(girth, m_model.evaluate(probe));
                slopes[i] = (probed - current) / (probe.measures.value(girth.target) - value);
            }

            if (slopes.at(i) > 0)
            {
                shape.measures[girth.target] = qBound(-1.0, value + (girth.wanted - current) / slopes.at(i), 1.0);
            }
        }
    }

    shape.scale = scaleFor(shape, wanted);

    BodyFit fit;
    fit.shape = shape;
    fit.measured = m_measurer.measure(m_model.evaluate(shape));
    return fit;
}

//---------------------------------------------------------------------------------------------------------------------
// The uniform scale that gives the shape the wanted height, or 1 if no height is wanted.
qreal BodyFitter::scaleFor(const BodyShape& shape, const BodyMeasurements& wanted) const
{
    qreal scale = 1.0;
    if (wanted.height > 0)
    {
        BodyShape unscaled = shape;
        unscaled.scale = 1.0;
        const qreal height = m_measurer.height(m_model.evaluate(unscaled));
        if (height > 0)
        {
            scale = wanted.height / height;
        }
    }
    return scale;
}
