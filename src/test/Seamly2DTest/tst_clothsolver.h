//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_clothsolver.h
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

#ifndef TST_CLOTHSOLVER_H
#define TST_CLOTHSOLVER_H

#include <QObject>

class TST_ClothSolver : public QObject
{
    Q_OBJECT
public:
    explicit TST_ClothSolver(QObject* parent = nullptr);

private slots:
    void freeFallFollowsGravity() const;
    void fullAirDampingKeepsNoSpeed() const;
    void pinnedClothHangs() const;
    void stitchesCloseTheGap() const;
    void colliderMeasuresDistance() const;
    void clothRestsOnSphere() const;
    void clothLandsOnCloth() const;
    void foldedClothKeepsItsLayers() const;
    void seamsCloseDespiteSelfContact() const;
    void biasGivesMoreThanGrain() const;
    void stifferFabricBendsLess() const;
    void shearFollowsFromBias() const;

private:
    Q_DISABLE_COPY(TST_ClothSolver)
};

#endif // TST_CLOTHSOLVER_H
