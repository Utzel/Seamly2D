//---------------------------------------------------------------------------------------------------------------------
//  @file   dock_maximizer.h
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

#ifndef DOCK_MAXIMIZER_H
#define DOCK_MAXIMIZER_H

#include <QByteArray>
#include <QDockWidget>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRect>

class QAction;
class QEvent;
class QMainWindow;

/// @brief Gives a dock widget all of its main window, the other docks and the central widget put away, or the whole
/// screen, the menus, toolbars and the dock's title bar put away as well, and puts it all back as it was. Docked, the
/// dock keeps its place for when it is back; floating, it fills the screen it is on and leaves the main window be.
///
/// The main window's layout is saved as it was, never as it is while the dock fills it: restore() first.
class DockMaximizer : public QObject
{
    Q_OBJECT

public:
                                    DockMaximizer(QMainWindow* window, QDockWidget* dock);
    virtual                        ~DockMaximizer() = default;

    bool                            isMaximized() const;
    bool                            isFullScreen() const;

    virtual bool                    eventFilter(QObject* watched, QEvent* event) override;

public slots:
    void                            setMaximized(bool maximized);
    void                            setFullScreen(bool full_screen);
    void                            restore();

signals:
    void                            maximizedChanged(bool maximized);
    void                            fullScreenChanged(bool full_screen);

private:
    Q_DISABLE_COPY(DockMaximizer)

    enum class Size
    {
        Normal,
        Maximized,
        FullScreen
    };

    QMainWindow*                    m_window;
    QDockWidget*                    m_dock;
    bool                            m_maximized;         // asked for, full screen or not
    bool                            m_full_screen;       // asked for
    Size                            m_size;              // as it is
    bool                            m_floating;          // whether the dock was floating as it grew
    QByteArray                      m_layout;            // the main window's docks and toolbars before
    QDockWidget::DockWidgetFeatures m_features;          // the dock's before
    QRect                           m_geometry;          // where the floating dock was before
    Qt::WindowStates                m_window_states;     // the main window's before full screen
    QList<QPointer<QWidget>>        m_put_away;          // the menus and toolbars put away for full screen
    QList<QPointer<QAction>>        m_lent;              // their actions, the main window's for full screen
    bool                            m_changing;

    void                            change(bool maximized, bool full_screen);
    void                            maximize();
    void                            unmaximize();
    void                            fillScreen();
    void                            leaveScreen();
    void                            hideTitleBar();
    void                            showTitleBar();
    void                            dockShown(bool visible);
    void                            dockFloated(bool floating);
};

#endif // DOCK_MAXIMIZER_H
