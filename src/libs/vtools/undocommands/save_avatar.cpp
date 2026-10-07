//---------------------------------------------------------------------------------------------------------------------
//  @file   save_avatar.cpp
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

#include "save_avatar.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveAvatar::SaveAvatar(const QString& text, const VGarmentAvatar& old_avatar, const VGarmentAvatar& new_avatar,
                       VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_avatar(old_avatar)
    , m_new_avatar(new_avatar)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveAvatar::undo()
{
    doc->setAvatar(m_old_avatar);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveAvatar::redo()
{
    doc->setAvatar(m_new_avatar);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveAvatar::id() const
{
    return static_cast<int>(UndoCommand::SaveAvatar);
}
