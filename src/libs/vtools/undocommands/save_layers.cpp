//---------------------------------------------------------------------------------------------------------------------
//  @file   save_layers.cpp
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

#include "save_layers.h"

#include <QDomElement>

//---------------------------------------------------------------------------------------------------------------------
SaveLayers::SaveLayers(const QString& text, const QVector<VPieceLayer>& old_layers,
                       const QVector<VPieceLayer>& new_layers, VAbstractPattern* doc, QUndoCommand* parent)
    : VUndoCommand(QDomElement(), doc, parent)
    , m_old_layers(old_layers)
    , m_new_layers(new_layers)
{
    setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveLayers::undo()
{
    doc->setLayers(m_old_layers);
}

//---------------------------------------------------------------------------------------------------------------------
void SaveLayers::redo()
{
    doc->setLayers(m_new_layers);
}

//---------------------------------------------------------------------------------------------------------------------
int SaveLayers::id() const
{
    return static_cast<int>(UndoCommand::SaveLayers);
}
