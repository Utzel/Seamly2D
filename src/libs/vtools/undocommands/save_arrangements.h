//---------------------------------------------------------------------------------------------------------------------
//  @file   save_arrangements.h
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

#ifndef SAVE_ARRANGEMENTS_H
#define SAVE_ARRANGEMENTS_H

#include <QString>
#include <QVector>

#include "vundocommand.h"

/// @brief Replaces where the pieces start out on the avatar.
class SaveArrangements : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveArrangements(const QString& text, const QVector<VPieceArrangement>& old_arrangements,
                                                const QVector<VPieceArrangement>& new_arrangements,
                                                VAbstractPattern* doc, QUndoCommand* parent = nullptr);
    virtual                   ~SaveArrangements() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveArrangements)

    QVector<VPieceArrangement> m_old_arrangements;
    QVector<VPieceArrangement> m_new_arrangements;
};

#endif // SAVE_ARRANGEMENTS_H
