//---------------------------------------------------------------------------------------------------------------------
//  @file   save_topstitches.h
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

#ifndef SAVE_TOPSTITCHES_H
#define SAVE_TOPSTITCHES_H

#include <QString>

#include "vundocommand.h"

/// @brief Replaces the garment's topstitching.
class SaveTopstitches : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveTopstitches(const QString& text, const VTopstitches& old_topstitches,
                                               const VTopstitches& new_topstitches, VAbstractPattern* doc,
                                               QUndoCommand* parent = nullptr);
    virtual                   ~SaveTopstitches() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveTopstitches)

    VTopstitches               m_old_topstitches;
    VTopstitches               m_new_topstitches;
};

#endif // SAVE_TOPSTITCHES_H
