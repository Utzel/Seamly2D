//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_bodymodel.h
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

#ifndef TST_BODYMODEL_H
#define TST_BODYMODEL_H

#include <QObject>

class TST_BodyModel : public QObject
{
    Q_OBJECT
public:
    explicit TST_BodyModel(QObject* parent = nullptr);

private slots:
    void bodyDataLoads() const;
    void ageFromYears() const;
    void macroTargetsFollowMpfb() const;
    void defaultBodiesHaveTheirHeights() const;
    void bodyStandsOnTheFloor() const;
    void scaleSetsHeight() const;
    void measureTargetChangesGirth() const;
    void tapeGirthOfCylinder() const;
    void tapeLeavesOutArms() const;
    void armLengthsRunFromTheShoulderTip() const;
    void legsAreMeasuredFromTheFloor() const;
    void fitMatchesMeasurements() const;
    void fitReachesTypicalBodies() const;
    void fitKeepsTheArmsProportions() const;
    void fitKeepsTheLegsProportions() const;
    void fitStaysInRange() const;
    void standardSizesFollowTheGrading() const;
    void standardSizesAreReached() const;
    void wrapFindsBodyParts() const;
    void wrapPlacesOnAGivenPart() const;
    void wrappedPiecesStartOutsideTheBody() const;
    void sleevesStartAroundTheArm() const;
    void collarsStartAroundTheNeck() const;
    void arrangementPointsSitOnTheBody() const;
    void arrangementPointsFollowTheAvatar() const;
    void piecesTurnAndTurnOver() const;
    void piecesMoveAndTilt() const;
    void superimposedPiecesLieOnTheirPartner() const;

private:
    Q_DISABLE_COPY(TST_BodyModel)
};

#endif // TST_BODYMODEL_H
