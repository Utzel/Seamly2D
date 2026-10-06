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
// A measurement the fitting corrects: how to measure it, what it should be, and the measure target that changes it.
struct MeasureControl
{
    qreal   (BodyMeasurer::*measure)(const QVector<QVector3D>&) const;
    qreal   wanted;
    QString target;
    bool    sets_build;   ///< takes part in choosing the overall build
};

// Golden section steps for the build: the interval shrinks to 0.618^14, about 0.001.
const int build_search_steps = 14;

// Rounds of correcting each measurement with its measure target.
const int correction_rounds = 3;

// How far a measure target is moved to see how much its measurement changes.
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

    QVector<MeasureControl> controls;
    const MeasureControl girths[] = {
        {&BodyMeasurer::bust, wanted.bust, QStringLiteral("torso/measure-bust-circ"), true},
        {&BodyMeasurer::waist, wanted.waist, QStringLiteral("torso/measure-waist-circ"), true},
        {&BodyMeasurer::hip, wanted.hip, QStringLiteral("torso/measure-hips-circ"), true},
        {&BodyMeasurer::neck, wanted.neck, QStringLiteral("neck/measure-neck-circ"), false}};
    for (const MeasureControl& girth : girths)
    {
        if (girth.wanted > 0)
        {
            controls.append(girth);
        }
    }

    // Measurements grow with the scale, so a body is built at scale 1 and its measurements scaled to the wanted
    // height.
    auto scaled = [this, &wanted](const MeasureControl& control, const QVector<QVector3D>& positions)
    {
        const qreal scale = wanted.height > 0 ? wanted.height / m_measurer.height(positions) : 1.0;
        return (m_measurer.*control.measure)(positions) * scale;
    };

    // The overall build: the weight that best matches bust, waist and hip together.
    auto build_error = [this, &shape, &controls, &scaled](qreal weight)
    {
        BodyShape candidate = shape;
        candidate.weight = weight;
        const QVector<QVector3D> positions = m_model.evaluate(candidate);
        qreal error = 0;
        for (const MeasureControl& control : controls)
        {
            if (control.sets_build)
            {
                const qreal difference = scaled(control, positions) - control.wanted;
                error += difference * difference;
            }
        }
        return error;
    };

    bool any_build_girth = false;
    for (const MeasureControl& control : controls)
    {
        any_build_girth = any_build_girth || control.sets_build;
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

    // The upper arm and the forearm; an arm only known as a whole keeps the body's own proportions.
    qreal upper_arm = wanted.upper_arm;
    qreal lower_arm = wanted.lower_arm;
    if (wanted.arm > 0 && (upper_arm <= 0 || lower_arm <= 0))
    {
        if (upper_arm > 0)
        {
            lower_arm = wanted.arm - upper_arm;
        }
        else if (lower_arm > 0)
        {
            upper_arm = wanted.arm - lower_arm;
        }
        else
        {
            const QVector<QVector3D> positions = m_model.evaluate(shape);
            const qreal whole = m_measurer.arm(positions);
            upper_arm = whole > 0 ? wanted.arm * m_measurer.upperArm(positions) / whole : wanted.arm / 2.0;
            lower_arm = wanted.arm - upper_arm;
        }
    }
    const MeasureControl arm_lengths[] = {
        {&BodyMeasurer::upperArm, upper_arm, QStringLiteral("arms/measure-upperarm-length"), false},
        {&BodyMeasurer::lowerArm, lower_arm, QStringLiteral("arms/measure-lowerarm-length"), false}};
    for (const MeasureControl& length : arm_lengths)
    {
        if (length.wanted > 0)
        {
            controls.append(length);
        }
    }

    // Each measurement on its own: a secant step per round, with the slope taken once at the start.
    QVector<qreal> slopes(controls.size(), 0.0);
    for (int round = 0; round < correction_rounds; ++round)
    {
        for (int i = 0; i < controls.size(); ++i)
        {
            const MeasureControl& control = controls.at(i);
            const qreal value = shape.measures.value(control.target, 0.0);
            const qreal current = scaled(control, m_model.evaluate(shape));

            if (round == 0)
            {
                BodyShape probe = shape;
                probe.measures[control.target] = value + (value > 0 ? -slope_probe : slope_probe);
                const qreal probed = scaled(control, m_model.evaluate(probe));
                slopes[i] = (probed - current) / (probe.measures.value(control.target) - value);
            }

            if (slopes.at(i) > 0)
            {
                shape.measures[control.target] = qBound(-1.0, value + (control.wanted - current) / slopes.at(i),
                                                         1.0);
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
