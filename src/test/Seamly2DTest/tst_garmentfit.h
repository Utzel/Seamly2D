//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_garmentfit.h
//  @author Julius
//  @date   7 Oct, 2026
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

#ifndef TST_GARMENTFIT_H
#define TST_GARMENTFIT_H

#include <QObject>

class TST_GarmentFit : public QObject
{
    Q_OBJECT
public:
    explicit TST_GarmentFit(QObject* parent = nullptr);

private slots:
    void easeIsTheGapOffTheBody() const;
    void pressureIsThePushBackOverTheCloth() const;
    void noBodyNoPressure() const;

private:
    Q_DISABLE_COPY(TST_GarmentFit)
};

#endif // TST_GARMENTFIT_H
