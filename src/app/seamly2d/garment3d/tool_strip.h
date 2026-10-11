//---------------------------------------------------------------------------------------------------------------------
//  @file   tool_strip.h
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

#ifndef TOOL_STRIP_H
#define TOOL_STRIP_H

#include <QVector>
#include <QWidget>

class FlowLayout;
class QAction;
class QEvent;
class QFrame;
class QHBoxLayout;
class QResizeEvent;
class QToolBar;

/// @brief A toolbar of groups of tools that keeps to one row. As far as it is wide, the groups that matter most stay
/// in the row, in their order, and the others wait behind More at its end, in a panel that opens below it with them in
/// rows; wide enough, all are in the row. The tools at its end beside More, as the one maximizing the view, are always
/// there.
class ToolStrip : public QWidget
{
    Q_OBJECT

public:
    explicit                ToolStrip(QWidget* parent = nullptr);
    virtual                ~ToolStrip() = default;

    void                    addGroup(QWidget* group, int rank);
    void                    addEndAction(QAction* action);
    QAction*                moreAction() const;
    bool                    isInRow(QWidget* group) const;
    QWidget*                panel() const;

    virtual QSize           sizeHint() const override;
    virtual QSize           minimumSizeHint() const override;
    virtual bool            eventFilter(QObject* watched, QEvent* event) override;

protected:
    virtual bool            event(QEvent* event) override;
    virtual void            resizeEvent(QResizeEvent* event) override;

private:
    Q_DISABLE_COPY(ToolStrip)

    QVector<QWidget*>       m_groups;       // in their order in the row
    QVector<int>            m_ranks;        // which stay in the row first: the lowest
    QVector<QWidget*>       m_in_row;
    QHBoxLayout*            m_row;
    QToolBar*               m_end_bar;      // More and the tools always there
    QAction*                m_more_action;
    QFrame*                 m_panel;
    FlowLayout*             m_panel_layout;
    bool                    m_stale;        // groups added since the row was last arranged
    bool                    m_arranging;

    void                    closeOnUse(QWidget* group);
    int                     moreWidth() const;
    void                    arrange();
    void                    showPanel();
};

#endif // TOOL_STRIP_H
