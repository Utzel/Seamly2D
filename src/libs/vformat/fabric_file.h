//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_file.h
//  @author Julius
//  @date   9 Oct, 2026
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

#ifndef FABRIC_FILE_H
#define FABRIC_FILE_H

#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/xml/vdomdocument.h"

/// @brief A fabric file, one fabric each, as the 3D View's fabric library keeps them and CLO keeps its fabric files:
/// the fabric's name, how heavy and how thick it is, how hard it is to stretch along its grain, across it and on the
/// bias, and how stiffly it bends along its grain and across it.
class VFabricFile : public VDomDocument
{
public:
                         VFabricFile();
    virtual             ~VFabricFile() = default;

    static const QString Extension;
    static const QString TagFabric;
    static const QString AttrName;
    static const QString AttrWeight;
    static const QString AttrWarp;
    static const QString AttrWeft;
    static const QString AttrBias;
    static const QString AttrBendingWarp;
    static const QString AttrBendingWeft;
    static const QString AttrThickness;

    void                 setFabric(const VCustomFabric& fabric);
    VCustomFabric        fabric() const;

private:
    Q_DISABLE_COPY(VFabricFile)
};

#endif // FABRIC_FILE_H
