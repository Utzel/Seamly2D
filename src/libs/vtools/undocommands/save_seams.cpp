//---------------------------------------------------------------------------------------------------------------------
//  @file   save_seams.cpp
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

#include "save_seams.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveSeams::SaveSeams(const QString& text, const QVector<VSeam>& old_seams, const QVector<VSeam>& new_seams,
                     VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_seams(old_seams)
    , m_new_seams(new_seams)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveSeams::undo()
{
    doc->setSeams(m_old_seams);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveSeams::redo()
{
    doc->setSeams(m_new_seams);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveSeams::id() const
{
    return static_cast<int>(UndoCommand::SaveSeams);
}
