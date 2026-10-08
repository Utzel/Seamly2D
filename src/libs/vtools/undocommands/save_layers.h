//---------------------------------------------------------------------------------------------------------------------
//  @file   save_layers.h
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

#ifndef SAVE_LAYERS_H
#define SAVE_LAYERS_H

#include <QString>

#include "vundocommand.h"

/// @brief Replaces the layers the pieces are worn in.
class SaveLayers : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveLayers(const QString& text, const QVector<VPieceLayer>& old_layers,
                                          const QVector<VPieceLayer>& new_layers, VAbstractPattern* doc,
                                          QUndoCommand* parent = nullptr);
    virtual                   ~SaveLayers() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveLayers)

    QVector<VPieceLayer>       m_old_layers;
    QVector<VPieceLayer>       m_new_layers;
};

#endif // SAVE_LAYERS_H
