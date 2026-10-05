//---------------------------------------------------------------------------------------------------------------------
//  @file   body_model.cpp
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

#include "body_model.h"

#include <QStringList>

#include <limits>

namespace
{
// One stretch of a macro value between two targets, as in MPFB2's macro.json.
struct MacroPart
{
    qreal   lowest;
    qreal   highest;
    QString low;
    QString high;
};

typedef QVector<QPair<QString, qreal>> Components;

// Targets weaker than this are left out. MPFB2 uses 0.01; a smaller cutoff keeps the body continuous in its
// parameters, which the fitting needs.
const qreal weight_cutoff = 0.0001;

// MakeHuman's default mix of its three ethnic base shapes.
const qreal race_weight = 1.0 / 3.0;

//---------------------------------------------------------------------------------------------------------------------
// Splits a macro value into the weights of the targets on either side of it.
Components interpolate(qreal value, const QVector<MacroPart>& parts)
{
    Components components;
    for (const MacroPart& part : parts)
    {
        if (value > part.lowest && value < part.highest)
        {
            const qreal position = (value - part.lowest) / (part.highest - part.lowest);
            if (!part.low.isEmpty())
            {
                components.append(qMakePair(part.low, 1.0 - position));
            }
            if (!part.high.isEmpty())
            {
                components.append(qMakePair(part.high, position));
            }
        }
    }
    return components;
}

//---------------------------------------------------------------------------------------------------------------------
// The usual three-way split around an average, with MPFB2's boundaries.
QVector<MacroPart> averageParts(const QString& name, qreal low_end = 0.49998, qreal high_start = 0.49999)
{
    return {{-0.01, low_end, QStringLiteral("min") + name, QStringLiteral("average") + name},
            {high_start, 1.01, QStringLiteral("average") + name, QStringLiteral("max") + name}};
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Converts an age in years to MakeHuman's age value.
qreal BodyShape::ageFromYears(qreal years)
{
    qreal value = 1.0;
    if (years <= 1.0)
    {
        value = 0.0;
    }
    else if (years < 11.0)
    {
        value = (years - 1.0) / 10.0 * 0.1875;
    }
    else if (years < 25.0)
    {
        value = 0.1875 + (years - 11.0) / 14.0 * 0.3125;
    }
    else if (years < 90.0)
    {
        value = 0.5 + (years - 25.0) / 65.0 * 0.5;
    }
    return value;
}

//---------------------------------------------------------------------------------------------------------------------
BodyModel::BodyModel(QSharedPointer<const BodyData> data)
    : m_data(data)
{}

//---------------------------------------------------------------------------------------------------------------------
bool BodyModel::isValid() const
{
    return !m_data.isNull() && m_data->skin_vertex_count > 0;
}

//---------------------------------------------------------------------------------------------------------------------
int BodyModel::skinVertexCount() const
{
    return isValid() ? m_data->skin_vertex_count : 0;
}

//---------------------------------------------------------------------------------------------------------------------
const QVector<quint32>& BodyModel::triangles() const
{
    static const QVector<quint32> no_triangles;
    return isValid() ? m_data->triangles : no_triangles;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Vertex positions of the body in cm: standing on y = 0, centred on the pelvis, facing +z. The skin
/// vertices come first (see skinVertexCount()), the joint helpers after them.
QVector<QVector3D> BodyModel::evaluate(const BodyShape& shape) const
{
    QVector<QVector3D> positions;
    if (isValid())
    {
        positions = m_data->base_positions;

        auto apply = [this, &positions](const QString& name, qreal weight)
        {
            const auto target = m_data->targets.constFind(name);
            if (target != m_data->targets.constEnd() && weight > 0)
            {
                const float factor = static_cast<float>(weight);
                for (int i = 0; i < target->indices.size(); ++i)
                {
                    positions[static_cast<int>(target->indices.at(i))] += target->offsets.at(i) * factor;
                }
            }
        };

        for (const QPair<QString, qreal>& target : macroTargets(shape))
        {
            apply(target.first, target.second);
        }
        for (auto measure = shape.measures.constBegin(); measure != shape.measures.constEnd(); ++measure)
        {
            const qreal value = qBound(-1.0, measure.value(), 1.0);
            apply(measure.key() + (value > 0 ? QStringLiteral("-incr") : QStringLiteral("-decr")), qAbs(value));
        }

        float floor = std::numeric_limits<float>::max();
        for (int i = 0; i < m_data->skin_vertex_count; ++i)
        {
            floor = qMin(floor, positions.at(i).y());
        }
        const QVector3D pelvis = joint(positions, QStringLiteral("pelvis"));
        const QVector3D offset(-pelvis.x(), -floor, -pelvis.z());
        const float scale = static_cast<float>(shape.scale);
        for (QVector3D& position : positions)
        {
            position = (position + offset) * scale;
        }
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Centre of a joint's helper vertices, e.g. "pelvis", "neck", "l-shoulder". Null for unknown joints.
QVector3D BodyModel::joint(const QVector<QVector3D>& positions, const QString& name) const
{
    QVector3D center;
    if (isValid())
    {
        const QVector<quint32> indices = m_data->joints.value(name);
        for (const quint32 index : indices)
        {
            center += positions.at(static_cast<int>(index));
        }
        if (!indices.isEmpty())
        {
            center /= static_cast<float>(indices.size());
        }
    }
    return center;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The macro targets and their weights for a shape, following MPFB2's
/// TargetService.calculate_target_stack_from_macro_info_dict().
///
/// One deliberate difference: the breast targets are weighted by how female the body is. MPFB2 applies them at
/// full strength as soon as the body is at all female.
QVector<QPair<QString, qreal>> BodyModel::macroTargets(const BodyShape& shape)
{
    const Components gender = interpolate(shape.gender, {{-0.01, 1.01, QStringLiteral("female"),
                                                          QStringLiteral("male")}});
    const Components age = interpolate(shape.age,
                                       {{-0.01, 0.1874998, QStringLiteral("baby"), QStringLiteral("child")},
                                        {0.1874999, 0.49998, QStringLiteral("child"), QStringLiteral("young")},
                                        {0.49999, 1.01, QStringLiteral("young"), QStringLiteral("old")}});
    const Components muscle = interpolate(shape.muscle, averageParts(QStringLiteral("muscle")));
    const Components weight = interpolate(shape.weight, averageParts(QStringLiteral("weight")));
    const Components cup_size = interpolate(shape.cup_size, averageParts(QStringLiteral("cup")));
    const Components firmness = interpolate(shape.firmness, averageParts(QStringLiteral("firmness"), 0.4998, 0.4999));

    QVector<QPair<QString, qreal>> targets;
    auto add = [&targets](const QString& name, qreal target_weight)
    {
        if (target_weight > weight_cutoff)
        {
            targets.append(qMakePair(name, target_weight));
        }
    };

    for (const QString& race : {QStringLiteral("african"), QStringLiteral("asian"), QStringLiteral("caucasian")})
    {
        for (const auto& a : age)
        {
            for (const auto& g : gender)
            {
                add(QStringLiteral("macrodetails/%1-%2-%3").arg(race, g.first, a.first),
                    race_weight * g.second * a.second);
            }
        }
    }

    for (const auto& g : gender)
    {
        for (const auto& a : age)
        {
            for (const auto& m : muscle)
            {
                for (const auto& w : weight)
                {
                    add(QStringLiteral("macrodetails/universal-%1-%2-%3-%4").arg(g.first, a.first, m.first, w.first),
                        g.second * a.second * m.second * w.second);
                }
            }
        }
    }

    for (const auto& g : gender)
    {
        for (const auto& a : age)
        {
            for (const auto& m : muscle)
            {
                for (const auto& w : weight)
                {
                    for (const auto& c : cup_size)
                    {
                        for (const auto& f : firmness)
                        {
                            const QString name = QStringLiteral("breast/%1-%2-%3-%4-%5-%6")
                                                     .arg(g.first, a.first, m.first, w.first, c.first, f.first);
                            // There are no male breast targets, no baby ones, and the average one is the base.
                            if (g.first == QLatin1String("female") && a.first != QLatin1String("baby")
                                && !name.endsWith(QLatin1String("averagecup-averagefirmness")))
                            {
                                add(name, g.second * a.second * m.second * w.second * c.second * f.second);
                            }
                        }
                    }
                }
            }
        }
    }
    return targets;
}
