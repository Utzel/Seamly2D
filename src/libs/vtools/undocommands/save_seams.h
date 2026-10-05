//---------------------------------------------------------------------------------------------------------------------
//  @file   save_seams.h
//  @author Julius
//  @date   5 Oct, 2026
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

#ifndef SAVE_SEAMS_H
#define SAVE_SEAMS_H

#include <QString>
#include <QVector>

#include "vundocommand.h"

/// @brief Replaces the pattern's seams, for sewing pieces together, flipping a seam or taking it out.
class SaveSeams : public VUndoCommand
{
    Q_OBJECT
public:
                   SaveSeams(const QString& text, const QVector<VSeam>& old_seams, const QVector<VSeam>& new_seams,
                             VAbstractPattern* doc, QUndoCommand* parent = nullptr);
    virtual       ~SaveSeams() = default;

    virtual void   undo() override;
    virtual void   redo() override;
    virtual int    id() const override;

private:
    Q_DISABLE_COPY(SaveSeams)

    QVector<VSeam> m_old_seams;
    QVector<VSeam> m_new_seams;
};

#endif // SAVE_SEAMS_H
