//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_patternseams.h
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

#ifndef TST_PATTERNSEAMS_H
#define TST_PATTERNSEAMS_H

#include <QObject>

class TST_PatternSeams : public QObject
{
    Q_OBJECT
public:
    explicit TST_PatternSeams(QObject* parent = nullptr);

private slots:
    void seamsAreReadBack() const;
    void seamsFollowTheSchema() const;
    void seamsOverSeveralStretchesAreReadBack() const;
    void noSeamsLeaveNoElement() const;
    void undoRestoresSeams() const;
    void foldsAreReadBack() const;
    void undoRestoresFolds() const;
    void elasticsAreReadBack() const;
    void undoRestoresElastics() const;
    void olderPatternsAreConverted() const;
    void arrangementsAreReadBack() const;
    void garmentDataKeepsSchemaOrder() const;
    void undoRestoresArrangements() const;
    void layersAreReadBack() const;
    void undoRestoresLayers() const;
    void fabricsAreReadBack() const;
    void fabricTexturesAreReadBack() const;
    void fabricShrinkageIsReadBack() const;
    void customFabricsAreReadBack() const;
    void undoRestoresFabrics() const;
    void topstitchesAreReadBack() const;
    void undoRestoresTopstitches() const;
    void avatarIsReadBack() const;
    void undoRestoresAvatar() const;
    void drapeIsReadBack() const;
    void drapeChangesThePatternWithoutUndo() const;

private:
    Q_DISABLE_COPY(TST_PatternSeams)
};

#endif // TST_PATTERNSEAMS_H
