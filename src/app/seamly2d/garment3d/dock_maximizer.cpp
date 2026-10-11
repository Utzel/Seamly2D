//---------------------------------------------------------------------------------------------------------------------
//  @file   dock_maximizer.cpp
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

#include "dock_maximizer.h"

#include <QAction>
#include <QEvent>
#include <QMainWindow>
#include <QMargins>
#include <QMenu>
#include <QScreen>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// The actions with keys among these, and among those of their menus.
void actionsWithKeys(const QList<QAction*>& actions, QList<QAction*>& with_keys)
{
    for (QAction* action : actions)
    {
        if (!action->shortcuts().isEmpty() && !with_keys.contains(action))
        {
            with_keys.append(action);
        }
        if (QMenu* menu = action->menu())
        {
            actionsWithKeys(menu->actions(), with_keys);
        }
    }
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
DockMaximizer::DockMaximizer(QMainWindow* window, QDockWidget* dock)
    : QObject(window)
    , m_window(window)
    , m_dock(dock)
    , m_maximized(false)
    , m_full_screen(false)
    , m_size(Size::Normal)
    , m_floating(false)
    , m_layout()
    , m_features(dock->features())
    , m_geometry()
    , m_window_states(Qt::WindowNoState)
    , m_put_away()
    , m_lent()
    , m_changing(false)
{
    connect(m_dock, &QDockWidget::visibilityChanged, this, &DockMaximizer::dockShown);
    connect(m_dock, &QDockWidget::topLevelChanged, this, &DockMaximizer::dockFloated);
    m_window->installEventFilter(this);
}

//---------------------------------------------------------------------------------------------------------------------
bool DockMaximizer::isMaximized() const
{
    return m_maximized;
}

//---------------------------------------------------------------------------------------------------------------------
bool DockMaximizer::isFullScreen() const
{
    return m_full_screen;
}

//---------------------------------------------------------------------------------------------------------------------
// Taken out of full screen some other way, as the window manager can, the main window gets its menus and toolbars
// back.
bool DockMaximizer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_window && event->type() == QEvent::WindowStateChange && !m_changing
        && m_size == Size::FullScreen && !m_floating && !m_window->windowState().testFlag(Qt::WindowFullScreen))
    {
        m_window_states = m_window->windowState();
        change(m_maximized, false);
    }
    return QObject::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Maximized, the dock has all of the main window, or of the screen it floats on. Asked for in full screen, it
/// leaves full screen, maximized or not.
void DockMaximizer::setMaximized(bool maximized)
{
    change(maximized, false);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Full screen, the dock has the whole screen. Leaving it, the dock is maximized again if it was before, or
/// back in its place.
void DockMaximizer::setFullScreen(bool full_screen)
{
    change(m_maximized, full_screen);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Puts everything back as it was before the dock grew.
void DockMaximizer::restore()
{
    change(false, false);
}

//---------------------------------------------------------------------------------------------------------------------
// Goes from the size the dock is to the size asked for, a step at a time: full screen is maximized and more.
void DockMaximizer::change(bool maximized, bool full_screen)
{
    const bool maximized_changed = maximized != m_maximized;
    const bool full_screen_changed = full_screen != m_full_screen;
    m_maximized = maximized;
    m_full_screen = full_screen;

    const Size wanted = full_screen ? Size::FullScreen : maximized ? Size::Maximized : Size::Normal;
    m_changing = true;
    if (m_size == Size::FullScreen && wanted != Size::FullScreen)
    {
        leaveScreen();
        m_size = Size::Maximized;
    }
    if (m_size == Size::Maximized && wanted == Size::Normal)
    {
        unmaximize();
        m_size = Size::Normal;
    }
    if (m_size == Size::Normal && wanted != Size::Normal)
    {
        maximize();
        m_size = Size::Maximized;
    }
    if (m_size == Size::Maximized && wanted == Size::FullScreen)
    {
        fillScreen();
        m_size = Size::FullScreen;
    }
    m_changing = false;

    if (maximized_changed)
    {
        emit maximizedChanged(m_maximized);
    }
    if (full_screen_changed)
    {
        emit fullScreenChanged(m_full_screen);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Docked, the dock keeps its place, and can't be taken from it, while the other docks and the central widget are put
// away; floating, it fills the screen it is on, as far as the taskbar leaves room. Closed, it opens, to stay open.
void DockMaximizer::maximize()
{
    m_dock->show();
    m_floating = m_dock->isFloating();
    if (m_floating)
    {
        m_geometry = m_dock->geometry();
        const QRect frame = m_dock->frameGeometry();
        const QMargins margins(m_geometry.left() - frame.left(), m_geometry.top() - frame.top(),
                               frame.right() - m_geometry.right(), frame.bottom() - m_geometry.bottom());
        m_dock->setGeometry(m_dock->screen()->availableGeometry().marginsRemoved(margins));
        return;
    }

    m_layout = m_window->saveState();
    m_features = m_dock->features();
    m_dock->setFeatures(m_features & ~(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable));
    const QList<QDockWidget*> docks = m_window->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QDockWidget* dock : docks)
    {
        if (dock != m_dock)
        {
            dock->hide();
        }
    }
    if (QWidget* central = m_window->centralWidget())
    {
        central->hide();
    }
    m_dock->raise();
}

//---------------------------------------------------------------------------------------------------------------------
void DockMaximizer::unmaximize()
{
    if (m_floating)
    {
        m_dock->setGeometry(m_geometry);
        return;
    }

    if (QWidget* central = m_window->centralWidget())
    {
        central->show();
    }
    m_window->restoreState(m_layout);
    m_dock->setFeatures(m_features);
}

//---------------------------------------------------------------------------------------------------------------------
// The main window goes full screen with the dock alone in it, the dock's title bar, the menus, toolbars and status bar
// put away, their keys still working; a floating dock goes full screen on the screen it is on.
void DockMaximizer::fillScreen()
{
    if (m_floating)
    {
        m_dock->setWindowState(m_dock->windowState() | Qt::WindowFullScreen);
        return;
    }
    hideTitleBar();

    QList<QWidget*> bars;
    const QList<QToolBar*> tool_bars = m_window->findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
    for (QToolBar* tool_bar : tool_bars)
    {
        bars.append(tool_bar);
    }
    bars.append(m_window->menuWidget());
    bars.append(m_window->findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly));
    QList<QAction*> with_keys;
    m_put_away.clear();
    for (QWidget* bar : std::as_const(bars))
    {
        if (bar != nullptr && !bar->isHidden())
        {
            actionsWithKeys(bar->actions(), with_keys);
            m_put_away.append(bar);
            bar->hide();
        }
    }
    // Hidden, the bars' actions no longer take their keys; lent to the main window, they do.
    m_lent.clear();
    const QList<QAction*> own = m_window->actions();
    for (QAction* action : std::as_const(with_keys))
    {
        if (!own.contains(action))
        {
            m_window->addAction(action);
            m_lent.append(action);
        }
    }

    m_window_states = m_window->windowState();
    m_window->setWindowState((m_window_states & ~Qt::WindowMinimized) | Qt::WindowFullScreen);
}

//---------------------------------------------------------------------------------------------------------------------
void DockMaximizer::leaveScreen()
{
    if (m_floating)
    {
        m_dock->setWindowState(m_dock->windowState() & ~Qt::WindowFullScreen);
        return;
    }
    showTitleBar();

    for (const QPointer<QAction>& action : std::as_const(m_lent))
    {
        if (!action.isNull())
        {
            m_window->removeAction(action);
        }
    }
    m_lent.clear();
    for (const QPointer<QWidget>& bar : std::as_const(m_put_away))
    {
        if (!bar.isNull())
        {
            bar->show();
        }
    }
    m_put_away.clear();
    m_window->setWindowState(m_window_states);
}

//---------------------------------------------------------------------------------------------------------------------
// An empty title bar takes no room.
void DockMaximizer::hideTitleBar()
{
    m_dock->setTitleBarWidget(new QWidget(m_dock));
}

//---------------------------------------------------------------------------------------------------------------------
void DockMaximizer::showTitleBar()
{
    if (QWidget* empty = m_dock->titleBarWidget())
    {
        m_dock->setTitleBarWidget(nullptr);
        empty->deleteLater();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Closed while it fills the window or the screen, the dock puts everything back as it was, and stays closed.
void DockMaximizer::dockShown(bool visible)
{
    if (!visible && !m_changing && m_size != Size::Normal && m_dock->isHidden())
    {
        change(false, false);
        m_changing = true;
        m_dock->hide();
        m_changing = false;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Docked while it floats filling its screen, the dock stays where it was put, in the size the main window gives it.
void DockMaximizer::dockFloated(bool floating)
{
    if (!floating && !m_changing && m_size != Size::Normal && m_floating)
    {
        m_size = Size::Normal;
        m_dock->setWindowState(m_dock->windowState() & ~Qt::WindowFullScreen);
        change(false, false);
    }
}
