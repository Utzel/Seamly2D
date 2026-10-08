//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_stretch.h
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

#ifndef SEAM_STRETCH_H
#define SEAM_STRETCH_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>

/// @brief A place where two sewn stretches meet, as the distance along each in cm.
struct SeamMatch
{
    qreal first = 0;
    qreal second = 0;
};

/// @brief A vertex sewn onto the other side of a seam, at a point between two of that side's vertices.
///
/// Both ends of the edge are the same vertex where the stitch meets a vertex.
struct Stitch
{
    quint32 vertex = 0;
    quint32 edge_start = 0;
    quint32 edge_end = 0;
    qreal   along = 0;  ///< where on the edge, from 0 at its start to 1 at its end
};

/// @brief A stretch of seam line that is sewn to another: its points in cm, in the direction it is sewn, and how far
/// along it its notches are. Taken from a mesh, it also knows which vertex each point is.
class SeamStretch
{
public:
                              SeamStretch() = default;
    explicit                  SeamStretch(const QVector<QPointF>& points,
                                          const QVector<qreal>& notches = QVector<qreal>(),
                                          const QVector<quint32>& vertices = QVector<quint32>());

    const QVector<QPointF>&   points() const;
    const QVector<qreal>&     notches() const;
    const QVector<quint32>&   vertices() const;
    const QVector<qreal>&     distances() const;

    bool                      isEmpty() const;
    qreal                     length() const;
    QPointF                   pointAt(qreal distance) const;
    SeamStretch               reversed() const;

    static SeamStretch        joined(const QVector<SeamStretch>& stretches);
    static QVector<SeamMatch> matches(const SeamStretch& first, const SeamStretch& second);
    static QVector<Stitch>    stitches(const SeamStretch& first, const SeamStretch& second);

private:
    QVector<QPointF>          m_points;
    QVector<qreal>            m_notches;
    QVector<quint32>          m_vertices;
    QVector<qreal>            m_distances;
};

#endif // SEAM_STRETCH_H
