//---------------------------------------------------------------------------------------------------------------------
//  @file   body_model.h
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

#ifndef BODY_MODEL_H
#define BODY_MODEL_H

#include <QMap>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <QtGlobal>

#include "body_data.h"

/// @brief What a body looks like, in MakeHuman's terms. Every value runs from 0 to 1 with 0.5 the average, except
/// scale and the measure adjustments.
struct BodyShape
{
    qreal                gender = 0.5;   ///< 0 female, 1 male
    qreal                age = 0.5;      ///< 0 is 1 year, 0.1875 is 11, 0.5 is 25, 1 is 90 years
    qreal                muscle = 0.5;
    qreal                weight = 0.5;
    qreal                cup_size = 0.5;
    qreal                firmness = 0.5;
    qreal                scale = 1.0;    ///< uniform scale, sets the height
    QMap<QString, qreal> measures;       ///< measure targets from -1 (smaller) to 1 (larger), by base name

    static qreal         ageFromYears(qreal years);
};

/// @brief Builds bodies from the MakeHuman data: the base mesh plus a weighted mix of shape targets.
///
/// Which targets are mixed, and how strongly, follows MakeHuman's macro modifiers as implemented in MPFB2
/// (https://github.com/makehumancommunity/mpfb2, targetservice.py, GPLv3, by the MakeHuman team). The height and
/// proportions targets are left out: the height comes from the uniform scale instead.
class BodyModel
{
public:
    explicit                 BodyModel(QSharedPointer<const BodyData> data = BodyData::standard());

    bool                     isValid() const;
    int                      skinVertexCount() const;
    const QVector<quint32>&  triangles() const;

    QVector<QVector3D>       evaluate(const BodyShape& shape) const;
    QVector3D                joint(const QVector<QVector3D>& positions, const QString& name) const;
    QVector3D                crotch(const QVector<QVector3D>& positions) const;

    static QVector<QPair<QString, qreal>> macroTargets(const BodyShape& shape);

private:
    QSharedPointer<const BodyData> m_data;
    QVector<int>             m_crotch_line;  ///< skin vertices on the middle line from the back between the legs
                                             ///< to the front
};

#endif // BODY_MODEL_H
