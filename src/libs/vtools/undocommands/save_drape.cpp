//---------------------------------------------------------------------------------------------------------------------
//  @file   save_drape.cpp
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

#include "save_drape.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveDrape::SaveDrape(const QString& text, const VGarmentDrape& old_drape, const VGarmentDrape& new_drape,
                     VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_drape(old_drape)
    , m_new_drape(new_drape)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveDrape::undo()
{
    doc->setDrape(m_old_drape);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveDrape::redo()
{
    doc->setDrape(m_new_drape);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveDrape::id() const
{
    return static_cast<int>(UndoCommand::SaveDrape);
}
