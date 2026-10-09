//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_converter.h
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

#ifndef FABRIC_CONVERTER_H
#define FABRIC_CONVERTER_H

#include "abstract_converter.h"

/// @brief Checks a fabric file, as the 3D View's fabric library keeps them, against its schema, and brings one of an
/// older version up to the current one.
class VFabricConverter : public VAbstractConverter
{
public:
    explicit                   VFabricConverter(const QString& file_name);
    virtual                   ~VFabricConverter() = default;

    static const QString       FabricMaxVerStr;
    static const QString       CurrentSchema;
    static constexpr const int FabricMinVer = CONVERTER_VERSION_CHECK(1, 0, 0);
    static constexpr const int FabricMaxVer = CONVERTER_VERSION_CHECK(1, 0, 0);

protected:
    virtual int                minVer() const override;
    virtual int                maxVer() const override;

    virtual QString            minVerStr() const override;
    virtual QString            maxVerStr() const override;

    virtual QString            getSchema(int ver) const override;
    virtual void               applyPatches() override;
    virtual void               downgradeToCurrentMaxVersion() override;

    virtual bool               isReadOnly() const override;

private:
    Q_DISABLE_COPY(VFabricConverter)
    static const QString       FabricMinVerStr;
};

#endif // FABRIC_CONVERTER_H
