//---------------------------------------------------------------------------------------------------------------------
//  @file   save_elastics.h
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

#ifndef SAVE_ELASTICS_H
#define SAVE_ELASTICS_H

#include <QString>

#include "vundocommand.h"

/// @brief Replaces the elastics sewn into the pieces.
class SaveElastics : public VUndoCommand
{
    Q_OBJECT
public:
                               SaveElastics(const QString& text, const QVector<VElastic>& old_elastics,
                                            const QVector<VElastic>& new_elastics, VAbstractPattern* doc,
                                            QUndoCommand* parent = nullptr);
    virtual                   ~SaveElastics() = default;

    virtual void               undo() override;
    virtual void               redo() override;
    virtual int                id() const override;

private:
    Q_DISABLE_COPY(SaveElastics)

    QVector<VElastic>          m_old_elastics;
    QVector<VElastic>          m_new_elastics;
};

#endif // SAVE_ELASTICS_H
