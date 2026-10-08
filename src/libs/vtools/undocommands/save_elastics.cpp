//---------------------------------------------------------------------------------------------------------------------
//  @file   save_elastics.cpp
//  @author Julius
//  @date   8 Oct, 2026
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

#include "save_elastics.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveElastics::SaveElastics(const QString& text, const QVector<VElastic>& old_elastics,
                           const QVector<VElastic>& new_elastics, VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_elastics(old_elastics)
    , m_new_elastics(new_elastics)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveElastics::undo()
{
    doc->setElastics(m_old_elastics);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveElastics::redo()
{
    doc->setElastics(m_new_elastics);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveElastics::id() const
{
    return static_cast<int>(UndoCommand::SaveElastics);
}
