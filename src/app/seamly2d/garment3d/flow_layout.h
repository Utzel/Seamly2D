//---------------------------------------------------------------------------------------------------------------------
//  @file   flow_layout.h
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

#ifndef FLOW_LAYOUT_H
#define FLOW_LAYOUT_H

#include <QLayout>
#include <QRect>
#include <QSize>
#include <QVector>

/// @brief Lays its widgets out side by side at their preferred sizes, as many in a row as fit, and the rest in rows
/// below, as words flow into lines. It is as tall as its rows take at the width it gets, so a dock's toolbar of
/// several groups wraps onto a second row where the dock is narrow instead of hiding tools behind its overflow.
class FlowLayout : public QLayout
{
public:
    explicit                    FlowLayout(QWidget* parent = nullptr, int spacing = 0);
    virtual                    ~FlowLayout();

    virtual void                addItem(QLayoutItem* item) override;
    virtual int                 count() const override;
    virtual QLayoutItem*        itemAt(int index) const override;
    virtual QLayoutItem*        takeAt(int index) override;
    virtual Qt::Orientations    expandingDirections() const override;
    virtual bool                hasHeightForWidth() const override;
    virtual int                 heightForWidth(int width) const override;
    virtual QSize               minimumSize() const override;
    virtual QSize               sizeHint() const override;
    virtual void                setGeometry(const QRect& rect) override;

private:
    Q_DISABLE_COPY(FlowLayout)

    QVector<QLayoutItem*>       m_items;

    int                         layOut(const QRect& rect, bool apply) const;
};

#endif // FLOW_LAYOUT_H
