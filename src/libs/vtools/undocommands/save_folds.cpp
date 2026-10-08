//---------------------------------------------------------------------------------------------------------------------
//  @file   save_folds.cpp
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

#include "save_folds.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveFolds::SaveFolds(const QString& text, const QVector<VFold>& old_folds, const QVector<VFold>& new_folds,
                     VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_folds(old_folds)
    , m_new_folds(new_folds)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveFolds::undo()
{
    doc->setFolds(m_old_folds);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveFolds::redo()
{
    doc->setFolds(m_new_folds);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveFolds::id() const
{
    return static_cast<int>(UndoCommand::SaveFolds);
}
