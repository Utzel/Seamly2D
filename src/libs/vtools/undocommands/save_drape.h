//---------------------------------------------------------------------------------------------------------------------
//  @file   save_drape.h
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

#ifndef SAVE_DRAPE_H
#define SAVE_DRAPE_H

#include <QString>

#include "vundocommand.h"

/// @brief Replaces how the garment hangs, as when the 3D View puts it back as arranged, so that can be undone.
class SaveDrape : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveDrape(const QString& text, const VGarmentDrape& old_drape,
                                         const VGarmentDrape& new_drape, VAbstractPattern* doc,
                                         QUndoCommand* parent = nullptr);
    virtual                   ~SaveDrape() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveDrape)

    VGarmentDrape              m_old_drape;
    VGarmentDrape              m_new_drape;
};

#endif // SAVE_DRAPE_H
