//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_converter.cpp
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

#include "fabric_converter.h"

// Versions are major.minor.patch, as the other formats' are: the major part only for stable releases, the minor one
// for a big change or ten small ones, the patch one for a small change. FabricMinVer and FabricMaxVer in the header
// change with them.
const QString VFabricConverter::FabricMinVerStr = QStringLiteral("1.0.0");
const QString VFabricConverter::FabricMaxVerStr = QStringLiteral("1.0.0");
const QString VFabricConverter::CurrentSchema   = QStringLiteral("://schema/fabric/v1.0.0.xsd");

//---------------------------------------------------------------------------------------------------------------------
VFabricConverter::VFabricConverter(const QString& file_name)
    : VAbstractConverter(file_name)
{
    ValidateInputFile(CurrentSchema);
}

//---------------------------------------------------------------------------------------------------------------------
int VFabricConverter::minVer() const
{
    return FabricMinVer;
}

//---------------------------------------------------------------------------------------------------------------------
int VFabricConverter::maxVer() const
{
    return FabricMaxVer;
}

//---------------------------------------------------------------------------------------------------------------------
QString VFabricConverter::minVerStr() const
{
    return FabricMinVerStr;
}

//---------------------------------------------------------------------------------------------------------------------
QString VFabricConverter::maxVerStr() const
{
    return FabricMaxVerStr;
}

//---------------------------------------------------------------------------------------------------------------------
QString VFabricConverter::getSchema(int ver) const
{
    switch (ver)
    {
        case FabricMaxVer:
            return CurrentSchema;
        default:
            InvalidVersion(ver);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VFabricConverter::applyPatches()
{
    switch (m_ver)
    {
        case FabricMaxVer:
            break;
        default:
            InvalidVersion(m_ver);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VFabricConverter::downgradeToCurrentMaxVersion()
{
    setVersion(FabricMaxVerStr);
    Save();
}

//---------------------------------------------------------------------------------------------------------------------
bool VFabricConverter::isReadOnly() const
{
    return false;
}
