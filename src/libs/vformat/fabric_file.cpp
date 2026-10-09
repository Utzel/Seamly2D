//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_file.cpp
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

#include "fabric_file.h"

#include "../ifc/xml/fabric_converter.h"

const QString VFabricFile::Extension       = QStringLiteral("sfab");
const QString VFabricFile::TagFabric       = QStringLiteral("fabric");
const QString VFabricFile::AttrName        = QStringLiteral("name");
const QString VFabricFile::AttrWeight      = QStringLiteral("weight");
const QString VFabricFile::AttrWarp        = QStringLiteral("warp");
const QString VFabricFile::AttrWeft        = QStringLiteral("weft");
const QString VFabricFile::AttrBias        = QStringLiteral("bias");
const QString VFabricFile::AttrBendingWarp = QStringLiteral("bendingWarp");
const QString VFabricFile::AttrBendingWeft = QStringLiteral("bendingWeft");
const QString VFabricFile::AttrThickness   = QStringLiteral("thickness");

//---------------------------------------------------------------------------------------------------------------------
VFabricFile::VFabricFile()
    : VDomDocument()
{}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Makes the file the given fabric's, in the current version of the format.
void VFabricFile::setFabric(const VCustomFabric& fabric)
{
    clear();
    QDomElement fabric_element = createElement(TagFabric);

    QDomElement version = createElement(TagVersion);
    version.appendChild(createTextNode(VFabricConverter::FabricMaxVerStr));
    fabric_element.appendChild(version);

    SetAttribute(fabric_element, AttrName, fabric.name);
    SetAttribute(fabric_element, AttrWeight, fabric.weight);
    SetAttribute(fabric_element, AttrWarp, fabric.warp);
    SetAttribute(fabric_element, AttrWeft, fabric.weft);
    SetAttribute(fabric_element, AttrBias, fabric.bias);
    SetAttribute(fabric_element, AttrBendingWarp, fabric.bending_warp);
    SetAttribute(fabric_element, AttrBendingWeft, fabric.bending_weft);
    SetAttribute(fabric_element, AttrThickness, fabric.thickness);

    appendChild(fabric_element);
    insertBefore(createProcessingInstruction(QStringLiteral("xml"),
                                             QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")), firstChild());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fabric the file holds.
VCustomFabric VFabricFile::fabric() const
{
    const QDomElement fabric_element = documentElement();
    VCustomFabric fabric;
    fabric.name = GetParametrEmptyString(fabric_element, AttrName);
    fabric.weight = GetParametrDouble(fabric_element, AttrWeight, QStringLiteral("0"));
    fabric.warp = GetParametrDouble(fabric_element, AttrWarp, QStringLiteral("0"));
    fabric.weft = GetParametrDouble(fabric_element, AttrWeft, QStringLiteral("0"));
    fabric.bias = GetParametrDouble(fabric_element, AttrBias, QStringLiteral("0"));
    fabric.bending_warp = GetParametrDouble(fabric_element, AttrBendingWarp, QStringLiteral("0"));
    fabric.bending_weft = GetParametrDouble(fabric_element, AttrBendingWeft, QStringLiteral("0"));
    fabric.thickness = GetParametrDouble(fabric_element, AttrThickness, QStringLiteral("0"));
    return fabric;
}
