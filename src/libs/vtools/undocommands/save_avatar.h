//---------------------------------------------------------------------------------------------------------------------
//  @file   save_avatar.h
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

#ifndef SAVE_AVATAR_H
#define SAVE_AVATAR_H

#include <QString>

#include "vundocommand.h"

/// @brief Replaces the avatar the 3D View shows a pattern without measurements on.
class SaveAvatar : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveAvatar(const QString& text, const VGarmentAvatar& old_avatar,
                                          const VGarmentAvatar& new_avatar, VAbstractPattern* doc,
                                          QUndoCommand* parent = nullptr);
    virtual                   ~SaveAvatar() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveAvatar)

    VGarmentAvatar             m_old_avatar;
    VGarmentAvatar             m_new_avatar;
};

#endif // SAVE_AVATAR_H
