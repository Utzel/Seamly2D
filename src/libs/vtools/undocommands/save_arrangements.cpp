//---------------------------------------------------------------------------------------------------------------------
//  @file   save_arrangements.cpp
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

#include "save_arrangements.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveArrangements::SaveArrangements(const QString& text, const QVector<VPieceArrangement>& old_arrangements,
                                   const QVector<VPieceArrangement>& new_arrangements, VAbstractPattern* doc,
                                   QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_arrangements(old_arrangements)
    , m_new_arrangements(new_arrangements)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveArrangements::undo()
{
    doc->setArrangements(m_old_arrangements);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveArrangements::redo()
{
    doc->setArrangements(m_new_arrangements);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveArrangements::id() const
{
    return static_cast<int>(UndoCommand::SaveArrangements);
}
