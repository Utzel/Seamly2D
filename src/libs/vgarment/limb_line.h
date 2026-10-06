//---------------------------------------------------------------------------------------------------------------------
//  @file   limb_line.h
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

#ifndef LIMB_LINE_H
#define LIMB_LINE_H

#include <QVector3D>
#include <QVector>
#include <QtGlobal>

/// @brief The middle line of a limb, from joint to joint, bent smoothly where the limb bends.
///
/// Places on the line are given by their distance along it from the first joint, in cm; the line goes on straight
/// beyond the first and the last joint. Where the limb bends, the line runs in an arc instead of a corner, so a tube
/// around it doesn't fold into itself on the inside of the bend.
///
/// Along the line runs a front: square to the line and turning with it as little as it can, starting out towards
/// +z, the way the body faces. Around the line, 0 degrees is the front and 90 degrees a quarter turn anticlockwise
/// looking along the line, so back along the line, the front and 90 degrees sit like y, z and x do.
class LimbLine
{
public:
                       LimbLine() = default;
                       LimbLine(const QVector<QVector3D>& joints, qreal bend, qreal before, qreal after);

    bool               isEmpty() const;
    qreal              length() const;

    QVector3D          pointAt(qreal along) const;
    QVector3D          directionAt(qreal along) const;
    QVector3D          frontAt(qreal along) const;
    QVector3D          aroundAt(qreal along, qreal angle) const;

    qreal              alongNearest(const QVector3D& point, qreal* distance = nullptr) const;
    qreal              alongAtHeight(qreal height) const;

private:
    QVector<QVector3D> m_points;      // every step cm along the line
    QVector<QVector3D> m_directions;
    QVector<QVector3D> m_fronts;
    qreal              m_start = 0;   // where the first point is, before the first joint
    qreal              m_length = 0;  // from the first joint to the last

    qreal              alongPoint(int index) const;
    void               sample(qreal along, int* index, float* t) const;
};

#endif // LIMB_LINE_H
