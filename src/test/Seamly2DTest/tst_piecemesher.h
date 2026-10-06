//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_piecemesher.h
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

#ifndef TST_PIECEMESHER_H
#define TST_PIECEMESHER_H

#include <QObject>

class TST_PieceMesher : public QObject
{
    Q_OBJECT
public:
    explicit TST_PieceMesher(QObject* parent = nullptr);

private slots:
    void rectangleIsFilled() const;
    void concaveOutlineStaysInside() const;
    void cornersAreKept() const;
    void curveIsEvenlySampled() const;
    void windingDoesNotMatter() const;
    void edgeLengthSetsResolution() const;
    void outlineWithoutAreaGivesEmptyMesh() const;
    void nearlyStraightEdgesAreMeshed() const;
    void pieceIsMeshedInCentimetres() const;
    void pathPointsBecomeVertices() const;
    void meshStretchNamesVertices() const;

private:
    Q_DISABLE_COPY(TST_PieceMesher)
};

#endif // TST_PIECEMESHER_H
