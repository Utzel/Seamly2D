//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_limbline.h
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

#ifndef TST_LIMBLINE_H
#define TST_LIMBLINE_H

#include <QObject>

class TST_LimbLine : public QObject
{
    Q_OBJECT
public:
    explicit TST_LimbLine(QObject* parent = nullptr);

private slots:
    void straightLimbFollowsItsBone() const;
    void bendIsAnArc() const;
    void frontTurnsWithTheLine() const;
    void anglesGoRoundLikeTheBody() const;
    void heightFindsThePlaceAlong() const;
    void noJointsNoLine() const;

private:
    Q_DISABLE_COPY(TST_LimbLine)
};

#endif // TST_LIMBLINE_H
