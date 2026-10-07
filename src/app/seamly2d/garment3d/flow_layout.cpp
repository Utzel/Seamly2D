//---------------------------------------------------------------------------------------------------------------------
//  @file   flow_layout.cpp
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

#include "flow_layout.h"

#include <QWidget>

//---------------------------------------------------------------------------------------------------------------------
/// @brief Spacing is the gap between widgets side by side, in pixels; rows follow each other without one.
FlowLayout::FlowLayout(QWidget* parent, int spacing)
    : QLayout(parent)
    , m_items()
{
    setSpacing(spacing);
}

//---------------------------------------------------------------------------------------------------------------------
FlowLayout::~FlowLayout()
{
    while (!m_items.isEmpty())
    {
        delete m_items.takeLast();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
}

//---------------------------------------------------------------------------------------------------------------------
int FlowLayout::count() const
{
    return static_cast<int>(m_items.size());
}

//---------------------------------------------------------------------------------------------------------------------
QLayoutItem* FlowLayout::itemAt(int index) const
{
    return m_items.value(index, nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
QLayoutItem* FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief It takes the width it is given, but no more height than its rows need.
Qt::Orientations FlowLayout::expandingDirections() const
{
    return Qt::Orientations();
}

//---------------------------------------------------------------------------------------------------------------------
bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How tall its rows are at the given width, in pixels.
int FlowLayout::heightForWidth(int width) const
{
    return layOut(QRect(0, 0, width, 0), false);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief As wide as its widest widget, so each fits whole in a row of its own.
QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem* item : m_items)
    {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief All its widgets in one row.
QSize FlowLayout::sizeHint() const
{
    int width = 0;
    int height = 0;
    for (const QLayoutItem* item : m_items)
    {
        const QSize hint = item->sizeHint();
        width += hint.width() + (width > 0 ? spacing() : 0);
        height = qMax(height, hint.height());
    }
    const QMargins margins = contentsMargins();
    return QSize(width + margins.left() + margins.right(), height + margins.top() + margins.bottom());
}

//---------------------------------------------------------------------------------------------------------------------
void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    layOut(rect, true);
}

//---------------------------------------------------------------------------------------------------------------------
// Places the widgets in rows within the rectangle, if apply says so, and returns how tall the rows are. A widget
// starts a new row where it doesn't fit beside the ones before; one wider than the rectangle gets a row of its own.
// Widgets in a row are centred on it.
int FlowLayout::layOut(const QRect& rect, bool apply) const
{
    const QRect area = rect.marginsRemoved(contentsMargins());
    QVector<QLayoutItem*> row;
    int x = area.x();
    int y = area.y();
    int row_height = 0;

    auto place_row = [apply, &row, &y, &row_height]()
    {
        if (apply)
        {
            for (QLayoutItem* item : row)
            {
                const QSize hint = item->sizeHint();
                const QRect placed = item->geometry();
                item->setGeometry(QRect(QPoint(placed.x(), y + (row_height - hint.height()) / 2), hint));
            }
        }
        row.clear();
    };

    for (QLayoutItem* item : m_items)
    {
        if (item->isEmpty())
        {
            continue;
        }
        const QSize hint = item->sizeHint();
        if (!row.isEmpty() && x + hint.width() > area.right() + 1)
        {
            place_row();
            y += row_height;
            x = area.x();
            row_height = 0;
        }
        if (apply)
        {
            item->setGeometry(QRect(QPoint(x, y), hint));
        }
        row.append(item);
        x += hint.width() + spacing();
        row_height = qMax(row_height, hint.height());
    }
    place_row();

    const QMargins margins = contentsMargins();
    return y + row_height - rect.y() + margins.bottom();
}
