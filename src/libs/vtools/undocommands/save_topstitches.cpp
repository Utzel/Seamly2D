//---------------------------------------------------------------------------------------------------------------------
//  @file   save_topstitches.cpp
//  @author Julius
//  @date   6 Oct, 2026
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

#include "save_topstitches.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveTopstitches::SaveTopstitches(const QString& text, const VTopstitches& old_topstitches,
                                 const VTopstitches& new_topstitches, VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_topstitches(old_topstitches)
    , m_new_topstitches(new_topstitches)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveTopstitches::undo()
{
    doc->setTopstitches(m_old_topstitches);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveTopstitches::redo()
{
    doc->setTopstitches(m_new_topstitches);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveTopstitches::id() const
{
    return static_cast<int>(UndoCommand::SaveTopstitches);
}
