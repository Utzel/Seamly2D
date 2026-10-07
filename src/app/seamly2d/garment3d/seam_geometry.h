//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_geometry.h
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

#ifndef SEAM_GEOMETRY_H
#define SEAM_GEOMETRY_H

#include <QColor>
#include <QPointF>
#include <QVector3D>
#include <QVector>
#include <QtQuick3D/QQuick3DGeometry>

/// @brief Colored bands along stretches of seam line, or colored lines between points, lying flat on the board of
/// pieces, in the board's coordinates: cm, y up. Or, for the pieces on the avatar, colored tubes along stretches of
/// seam line and lines between points in space, in scene coordinates.
///
/// Every vertex carries its color, so all seams can be drawn with one model; the material has to use vertex colors.
class SeamGeometry : public QQuick3DGeometry
{
    Q_OBJECT

public:
    struct Band
    {
        QVector<QPointF> points;
        QColor           color;
        qreal            width = 0;
    };

    struct Line
    {
        QPointF from;
        QPointF to;
        QColor  color;
    };

    struct Tube
    {
        QVector<QVector3D> points;
        QColor             color;
        qreal              radius = 0;
    };

    struct Segment
    {
        QVector3D from;
        QVector3D to;
        QColor    color;
    };

    explicit           SeamGeometry(QQuick3DObject* parent = nullptr);

    void               setBands(const QVector<Band>& bands);
    void               setLines(const QVector<Line>& lines);
    void               setTubes(const QVector<Tube>& tubes);
    void               setSegments(const QVector<Segment>& segments);

private:
    Q_DISABLE_COPY(SeamGeometry)

    void               setVertices(const QVector<float>& vertices, const QVector<quint32>& indices,
                                   PrimitiveType primitive_type);
};

#endif // SEAM_GEOMETRY_H
