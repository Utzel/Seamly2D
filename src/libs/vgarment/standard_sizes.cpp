//---------------------------------------------------------------------------------------------------------------------
//  @file   standard_sizes.cpp
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

#include "standard_sizes.h"

#include <cstdlib>

namespace
{
// Women's sizes are for a woman this tall, in cm.
const qreal women_height = 168.0;

// The sizes offered when a pattern hasn't chosen one: the middle of each range.
const int default_woman_size = 38;
const int default_man_size = 50;

//---------------------------------------------------------------------------------------------------------------------
StandardSize makeSize(int size, qreal height, qreal bust, qreal waist, qreal hip)
{
    StandardSize made;
    made.size = size;
    made.measurements.height = height;
    made.measurements.bust = bust;
    made.measurements.waist = waist;
    made.measurements.hip = hip;
    return made;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Women's sizes 34 to 48, from the smallest up.
QVector<StandardSize> StandardSizes::women()
{
    QVector<StandardSize> sizes;
    for (int size = 34; size <= 48; size += 2)
    {
        // Half the bust less 6 cm: 4 cm a size up to 44, 6 cm a size above.
        const qreal bust = 2.0 * size + 12.0 + qMax(0, size - 44);
        sizes.append(makeSize(size, women_height, bust, bust - 16.0, bust + 8.0));
    }
    return sizes;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Men's sizes 44 to 58, from the smallest up.
QVector<StandardSize> StandardSizes::men()
{
    const qreal heights[] = {168.0, 171.0, 174.0, 177.0, 180.0, 182.0, 184.0, 186.0};
    QVector<StandardSize> sizes;
    for (int size = 44, i = 0; size <= 58; size += 2, ++i)
    {
        const qreal chest = 2.0 * size;
        sizes.append(makeSize(size, heights[i], chest, chest - 12.0, chest + 4.0));
    }
    return sizes;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A woman's or a man's size; for a size not offered, the nearest one that is.
StandardSize StandardSizes::of(bool male, int size)
{
    const QVector<StandardSize> sizes = male ? men() : women();
    StandardSize nearest = sizes.first();
    for (const StandardSize& offered : sizes)
    {
        if (std::abs(offered.size - size) < std::abs(nearest.size - size))
        {
            nearest = offered;
        }
    }
    return nearest;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The size a woman's or a man's avatar has until another is chosen.
int StandardSizes::defaultSize(bool male)
{
    return male ? default_man_size : default_woman_size;
}
