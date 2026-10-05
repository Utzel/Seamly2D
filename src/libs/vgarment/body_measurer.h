//---------------------------------------------------------------------------------------------------------------------
//  @file   body_measurer.h
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

#ifndef BODY_MEASURER_H
#define BODY_MEASURER_H

#include <QVector3D>
#include <QVector>
#include <QtGlobal>

class BodyModel;

/// @brief Measurements of a body in cm, taken like a tailor takes them.
struct BodyMeasurements
{
    qreal height = 0;   ///< floor to the top of the head
    qreal bust = 0;     ///< fullest girth of the chest
    qreal waist = 0;    ///< smallest girth between hips and chest
    qreal hip = 0;      ///< fullest girth of the hips and seat
    qreal neck = 0;     ///< smallest girth of the neck
};

/// @brief Measures bodies made by a BodyModel.
///
/// A girth is taken like a tape measure: the body is sliced horizontally, the slice outlines of the torso (or both
/// legs) are kept, and the length around their convex hull is measured, so the tape bridges hollows such as the one
/// between the breasts. Where each girth is searched for follows the body's joints.
class BodyMeasurer
{
public:
    explicit           BodyMeasurer(const BodyModel& model);

    BodyMeasurements   measure(const QVector<QVector3D>& positions) const;

    qreal              height(const QVector<QVector3D>& positions) const;
    qreal              bust(const QVector<QVector3D>& positions) const;
    qreal              waist(const QVector<QVector3D>& positions) const;
    qreal              hip(const QVector<QVector3D>& positions) const;
    qreal              neck(const QVector<QVector3D>& positions) const;

    static qreal       tapeGirth(const QVector<QVector3D>& positions, const QVector<quint32>& triangles, float level,
                                 float max_center_x, float max_extent_x = 0);

private:
    const BodyModel&   m_model;

    qreal              extremeGirth(const QVector<QVector3D>& positions, float from, float to, bool largest,
                                    float max_extent_x = 0) const;
    float              shoulderDistance(const QVector<QVector3D>& positions) const;
};

#endif // BODY_MEASURER_H
