//---------------------------------------------------------------------------------------------------------------------
//  @file   standard_sizes.h
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

#ifndef STANDARD_SIZES_H
#define STANDARD_SIZES_H

#include <QVector>
#include <QtGlobal>

#include "body_measurer.h"

/// @brief A body of a European clothing size: the size, and the height, bust or chest, waist and hip of a body that
/// wears it, in cm.
struct StandardSize
{
    int              size = 0;
    BodyMeasurements measurements;
};

/// @brief The European clothing sizes an avatar can be given when a pattern has no measurements.
///
/// They follow the usual grading of European size charts and are meant as a start, to be changed to the body wanted.
/// A woman's size is half her bust less 6 cm, for a woman 168 cm tall, sizes 34 to 48 graded 4 cm a size in bust,
/// waist and hip, and 6 cm from 44 up. A man's size is half his chest, sizes 44 to 58, his waist 12 cm less than his
/// chest and his hip 4 cm more, and he is taller the larger his size.
class StandardSizes
{
public:
    static QVector<StandardSize> women();
    static QVector<StandardSize> men();
    static StandardSize          of(bool male, int size);
    static int                   defaultSize(bool male);
};

#endif // STANDARD_SIZES_H
