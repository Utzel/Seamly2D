//---------------------------------------------------------------------------------------------------------------------
//  @file   tool_strip.cpp
//  @author Julius
//  @date   11 Oct, 2026
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

#include "tool_strip.h"

#include "flow_layout.h"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QScreen>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <numeric>

namespace
{
// Gap between the groups side by side, and around them in the panel, in pixels.
const int group_spacing = 6;
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
ToolStrip::ToolStrip(QWidget* parent)
    : QWidget(parent)
    , m_groups()
    , m_ranks()
    , m_in_row()
    , m_row(new QHBoxLayout(this))
    , m_end_bar(new QToolBar(this))
    , m_more_action(nullptr)
    , m_panel(new QFrame(this, Qt::Popup))
    , m_panel_layout(nullptr)
    , m_stale(true)
    , m_arranging(false)
{
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(group_spacing);

    m_end_bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_more_action = m_end_bar->addAction(tr("More Tools"));
    m_more_action->setToolTip(tr("The tools there is no room for here; all of them show with the view maximized"));
    m_more_action->setVisible(false);
    connect(m_more_action, &QAction::triggered, this, &ToolStrip::showPanel);

    // The click outside that closes the panel doesn't go on to More, which would open it again.
    m_panel->setAttribute(Qt::WA_NoMouseReplay);
    m_panel->setFrameShape(QFrame::StyledPanel);
    m_panel_layout = new FlowLayout(m_panel, group_spacing);
    m_panel_layout->setContentsMargins(group_spacing, group_spacing, group_spacing, group_spacing);
    m_panel->installEventFilter(this);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds a group after those added before. Where the row is too narrow for all of them, those of a lower rank
/// stay in it before those of a higher one.
void ToolStrip::addGroup(QWidget* group, int rank)
{
    m_groups.append(group);
    m_ranks.append(rank);
    closeOnUse(group);
    m_stale = true;
    arrange();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds a tool at the end of the row, after More and the tools added there before, always in it.
void ToolStrip::addEndAction(QAction* action)
{
    m_end_bar->addAction(action);
    m_stale = true;
    arrange();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Opens the panel of the groups there is no room for in the row; it shows only while there are some.
QAction* ToolStrip::moreAction() const
{
    return m_more_action;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the group is in the row, rather than in the panel; the tools at the end always are.
bool ToolStrip::isInRow(QWidget* group) const
{
    return m_in_row.contains(group) || group == m_end_bar;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The panel More opens.
QWidget* ToolStrip::panel() const
{
    return m_panel;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief All the groups in the row.
QSize ToolStrip::sizeHint() const
{
    const QSize end = m_end_bar->sizeHint();
    int width = end.width() - (m_more_action->isVisible() ? moreWidth() : 0);
    int height = end.height();
    for (const QWidget* group : m_groups)
    {
        const QSize hint = group->sizeHint();
        width += hint.width() + group_spacing;
        height = qMax(height, hint.height());
    }
    return QSize(width, height);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Only More and the tools at the end in the row.
QSize ToolStrip::minimumSizeHint() const
{
    const QSize end = m_end_bar->sizeHint();
    return QSize(end.width() + (m_more_action->isVisible() ? 0 : moreWidth()), end.height());
}

//---------------------------------------------------------------------------------------------------------------------
// A group in the panel that grew or shrank may fit in the row now, or no longer.
bool ToolStrip::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_panel && event->type() == QEvent::LayoutRequest)
    {
        arrange();
    }
    return QWidget::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
// Likewise a group in the row.
bool ToolStrip::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest)
    {
        arrange();
    }
    return QWidget::event(event);
}

//---------------------------------------------------------------------------------------------------------------------
void ToolStrip::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    arrange();
}

//---------------------------------------------------------------------------------------------------------------------
// A tool used from the panel closes it, so the view is free for the work the tool starts: clicked, switched on or off
// as by the keyboard, or chosen from its menu.
void ToolStrip::closeOnUse(QWidget* group)
{
    QList<QToolBar*> bars = group->findChildren<QToolBar*>();
    if (QToolBar* bar = qobject_cast<QToolBar*>(group))
    {
        bars.append(bar);
    }
    for (QToolBar* bar : std::as_const(bars))
    {
        connect(bar, &QToolBar::actionTriggered, m_panel, &QWidget::hide);
    }
    const QList<QToolButton*> buttons = group->findChildren<QToolButton*>();
    for (QToolButton* button : buttons)
    {
        connect(button, &QToolButton::toggled, m_panel, &QWidget::hide);
    }
    const QList<QComboBox*> boxes = group->findChildren<QComboBox*>();
    for (QComboBox* box : boxes)
    {
        connect(box, QOverload<int>::of(&QComboBox::activated), m_panel, &QWidget::hide);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// How wide More is in the row, shown or not.
int ToolStrip::moreWidth() const
{
    const QWidget* more = m_end_bar->widgetForAction(m_more_action);
    return more != nullptr ? more->sizeHint().width() : 0;
}

//---------------------------------------------------------------------------------------------------------------------
// Keeps in the row the groups that matter most, each that still fits beside More and the tools at the end, and puts
// the others in the panel, both in their order.
void ToolStrip::arrange()
{
    if (m_arranging)
    {
        return;
    }

    const int end_width = m_end_bar->sizeHint().width() - (m_more_action->isVisible() ? moreWidth() : 0);
    int all_width = end_width;
    for (const QWidget* group : std::as_const(m_groups))
    {
        all_width += group->sizeHint().width() + group_spacing;
    }

    QVector<QWidget*> in_row;
    if (all_width <= width())
    {
        in_row = m_groups;
    }
    else
    {
        QVector<int> by_rank(m_groups.size());
        std::iota(by_rank.begin(), by_rank.end(), 0);
        std::stable_sort(by_rank.begin(), by_rank.end(), [this](int a, int b)
        {
            return m_ranks.at(a) < m_ranks.at(b);
        });
        int room = width() - end_width - moreWidth();
        QVector<bool> kept(m_groups.size(), false);
        for (const int index : std::as_const(by_rank))
        {
            const int needed = m_groups.at(index)->sizeHint().width() + group_spacing;
            if (needed <= room)
            {
                kept[index] = true;
                room -= needed;
            }
        }
        for (int i = 0; i < m_groups.size(); ++i)
        {
            if (kept.at(i))
            {
                in_row.append(m_groups.at(i));
            }
        }
    }
    if (in_row == m_in_row && !m_stale)
    {
        return;
    }

    m_arranging = true;
    m_in_row = in_row;
    m_stale = false;
    while (QLayoutItem* item = m_row->takeAt(0))
    {
        delete item;
    }
    while (QLayoutItem* item = m_panel_layout->takeAt(0))
    {
        delete item;
    }
    for (QWidget* group : std::as_const(m_groups))
    {
        if (in_row.contains(group))
        {
            m_row->addWidget(group);
        }
        else
        {
            m_panel_layout->addWidget(group);
        }
        group->show();
    }
    const bool more = in_row.size() < m_groups.size();
    m_more_action->setVisible(more);
    m_row->addStretch(1);
    m_row->addWidget(m_end_bar);
    if (!more)
    {
        m_panel->hide();
    }
    m_arranging = false;
}

//---------------------------------------------------------------------------------------------------------------------
// The panel opens below the row, as wide as it, with its groups in rows; above it where the screen ends.
void ToolStrip::showPanel()
{
    const int frame = m_panel->frameWidth();
    const int width = qMax(this->width(), m_panel_layout->minimumSize().width() + 2 * frame);
    const QSize size(width, m_panel_layout->heightForWidth(width - 2 * frame) + 2 * frame);
    QPoint at = mapToGlobal(QPoint(0, height()));
    if (const QScreen* screen = this->screen())
    {
        const QRect area = screen->availableGeometry();
        if (at.y() + size.height() > area.bottom() + 1)
        {
            at.setY(mapToGlobal(QPoint(0, 0)).y() - size.height());
        }
        at.setX(qBound(area.left(), at.x(), area.right() + 1 - size.width()));
    }
    m_panel->setGeometry(QRect(at, size));
    m_panel->show();
}
