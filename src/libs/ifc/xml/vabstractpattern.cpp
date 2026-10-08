//---------------------------------------------------------------------------------------------------------------------
/// @file   vabstractpattern.cpp
/// @author Douglas S Caskey
/// @date   17 Sep, 2023
///
/// @copyright
/// Copyright (C) 2017 - 2023 Seamly, LLC
/// https://github.com/fashionfreedom/seamly2d
///
/// @brief
/// Seamly2D is free software: you can redistribute it and/or modify
/// it under the terms of the GNU General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// Seamly2D is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
///
/// @file   vabstractpattern.cpp
/// @author Roman Telezhynskyi <dismine(at)gmail.com>
/// @date   15 6, 2015
///
/// @author Douglas S Caskey
/// @date   7.31.2022
///
/// @brief
/// @copyright
/// This source code is part of the Seamly2D project, a pattern making
/// program, whose allow create and modeling patterns of clothing.
/// Copyright (C) 2013-2022 Seamly2D project
/// <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
/// This source code is part of the Valentina project, a pattern making
/// program, whose allow create and modeling patterns of clothing.
/// Copyright (C) 2015 Valentina project
/// <https://bitbucket.org/dismine/valentina> All Rights Reserved.
///
/// Valentina is free software: you can redistribute it and/or modify
/// it under the terms of the GNU General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// Valentina is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with Valentina.  If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------------------------------------------------

#include "vabstractpattern.h"

#include "vdomdocument.h"
#include "vpatternconverter.h"
#include "vtoolrecord.h"
#include "../exception/vexceptionemptyparameter.h"
#include "../exception/vexceptionobjecterror.h"
#include "../exception/vexceptionconversionerror.h"
#include "../ifc/ifcdef.h"
#include "../ifc/exception/vexceptionbadid.h"
#include "../qmuparser/qmutokenparser.h"
#include "../vmisc/vabstractapplication.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vpiecenode.h"
#include "../vtools/tools/vabstracttool.h"
#include "../vtools/tools/vdatatool.h"

#include <QDataStream>
#include <QDomNode>
#include <QDomNodeList>
#include <QLatin1String>
#include <QList>
#include <QMessageBox>
#include <QMessageLogger>
#include <QSet>
#include <QString>
#include <QtDebug>

class QDomElement;

const QString VAbstractPattern::TagPattern              = QStringLiteral("pattern");
const QString VAbstractPattern::TagCalculation          = QStringLiteral("calculation");
const QString VAbstractPattern::TagModeling             = QStringLiteral("modeling");
const QString VAbstractPattern::TagPieces               = QStringLiteral("pieces");
const QString VAbstractPattern::TagPiece                = QStringLiteral("piece");
const QString VAbstractPattern::TagDescription          = QStringLiteral("description");
const QString VAbstractPattern::TagNotes                = QStringLiteral("notes");
const QString VAbstractPattern::TagImage                = QStringLiteral("image");
const QString VAbstractPattern::TagMeasurements         = QStringLiteral("measurements");
const QString VAbstractPattern::TagVariables            = QStringLiteral("variables");
const QString VAbstractPattern::TagVariable             = QStringLiteral("variable");
const QString VAbstractPattern::TagFinalMeasurements    = QStringLiteral("finalMeasurements");
const QString VAbstractPattern::TagFinalMeasurement     = QStringLiteral("finalMeasurement");
const QString VAbstractPattern::TagSeams                = QStringLiteral("seams");
const QString VAbstractPattern::TagSeam                 = QStringLiteral("seam");
const QString VAbstractPattern::TagFirstStretch         = QStringLiteral("first");
const QString VAbstractPattern::TagSecondStretch        = QStringLiteral("second");
const QString VAbstractPattern::TagFolds                = QStringLiteral("folds");
const QString VAbstractPattern::TagFold                 = QStringLiteral("fold");
const QString VAbstractPattern::TagElastics             = QStringLiteral("elastics");
const QString VAbstractPattern::TagElastic              = QStringLiteral("elastic");
const QString VAbstractPattern::TagArrangements         = QStringLiteral("arrangements");
const QString VAbstractPattern::TagArrangement          = QStringLiteral("arrangement");
const QString VAbstractPattern::TagLayers               = QStringLiteral("layers");
const QString VAbstractPattern::TagLayer                = QStringLiteral("layer");
const QString VAbstractPattern::TagFabrics              = QStringLiteral("fabrics");
const QString VAbstractPattern::TagFabric               = QStringLiteral("fabric");
const QString VAbstractPattern::TagCustomFabric         = QStringLiteral("customFabric");
const QString VAbstractPattern::TagTexture              = QStringLiteral("texture");
const QString VAbstractPattern::TagTopstitches          = QStringLiteral("topstitches");
const QString VAbstractPattern::TagTopstitch            = QStringLiteral("topstitch");
const QString VAbstractPattern::TagAvatar               = QStringLiteral("avatar");
const QString VAbstractPattern::TagDrape                = QStringLiteral("drape");
const QString VAbstractPattern::TagCloth                = QStringLiteral("cloth");
const QString VAbstractPattern::TagClothPin             = QStringLiteral("clothPin");
const QString VAbstractPattern::TagDraftBlock           = QStringLiteral("draftBlock");
const QString VAbstractPattern::TagGroups               = QStringLiteral("groups");
const QString VAbstractPattern::TagGroup                = QStringLiteral("group");
const QString VAbstractPattern::TagGroupItem            = QStringLiteral("item");
const QString VAbstractPattern::TagPoint                = QStringLiteral("point");
const QString VAbstractPattern::TagSpline               = QStringLiteral("spline");
const QString VAbstractPattern::TagArc                  = QStringLiteral("arc");
const QString VAbstractPattern::TagElArc                = QStringLiteral("elArc");
const QString VAbstractPattern::TagTools                = QStringLiteral("tools");
const QString VAbstractPattern::TagOperation            = QStringLiteral("operation");
const QString VAbstractPattern::TagGradation            = QStringLiteral("gradation");
const QString VAbstractPattern::TagHeights              = QStringLiteral("heights");
const QString VAbstractPattern::TagSizes                = QStringLiteral("sizes");
const QString VAbstractPattern::TagData                 = QStringLiteral("data");
const QString VAbstractPattern::TagPatternInfo          = QStringLiteral("patternInfo");
const QString VAbstractPattern::TagPatternName          = QStringLiteral("patternName");
const QString VAbstractPattern::TagPatternNum           = QStringLiteral("patternNumber");
const QString VAbstractPattern::TagCustomerName         = QStringLiteral("customer");
const QString VAbstractPattern::TagCompanyName          = QStringLiteral("company");
const QString VAbstractPattern::TagPatternLabel         = QStringLiteral("patternLabel");
const QString VAbstractPattern::TagGrainline            = QStringLiteral("grainline");
const QString VAbstractPattern::TagPath                 = QStringLiteral("path");
const QString VAbstractPattern::TagNodes                = QStringLiteral("nodes");
const QString VAbstractPattern::TagNode                 = QStringLiteral("node");
const QString VAbstractPattern::TagLine                 = QStringLiteral("line");

const QString VAbstractPattern::TagDraftImages          = QStringLiteral("images");
const QString VAbstractPattern::TagDraftImage           = QStringLiteral("image");
const QString VAbstractPattern::AttrId                  = QStringLiteral("id");
const QString VAbstractPattern::AttrFilename            = QStringLiteral("filename");
const QString VAbstractPattern::AttrLocked              = QStringLiteral("locked");
const QString VAbstractPattern::AttrAnchor              = QStringLiteral("anchor");
const QString VAbstractPattern::AttrXPos                = QStringLiteral("xPos");
const QString VAbstractPattern::AttrYPos                = QStringLiteral("yPos");
const QString VAbstractPattern::AttrHeight              = QStringLiteral("height");
const QString VAbstractPattern::AttrXScale              = QStringLiteral("xScale");
const QString VAbstractPattern::AttrYScale              = QStringLiteral("yScale");
const QString VAbstractPattern::AttrAspectRatio         = QStringLiteral("aspectRatio");
const QString VAbstractPattern::AttrUnits               = QStringLiteral("units");
const QString VAbstractPattern::AttrOpacity             = QStringLiteral("opacity");
const QString VAbstractPattern::AttrOrder               = QStringLiteral("order");
const QString VAbstractPattern::AttrSource              = QStringLiteral("src");
const QString VAbstractPattern::AttrXOffset             = QStringLiteral("xOffset");
const QString VAbstractPattern::AttrYOffset             = QStringLiteral("yOffset");
const QString VAbstractPattern::AttrBasepoint           = QStringLiteral("basepoint");


const QString VAbstractPattern::AttrName                = QStringLiteral("name");
const QString VAbstractPattern::AttrVisible             = QStringLiteral("visible");
const QString VAbstractPattern::AttrGroupLocked         = QStringLiteral("locked");
const QString VAbstractPattern::AttrGroupColor          = QStringLiteral("groupColor");
const QString VAbstractPattern::AttrObject              = QStringLiteral("object");
const QString VAbstractPattern::AttrTool                = QStringLiteral("tool");
const QString VAbstractPattern::AttrType                = QStringLiteral("type");
const QString VAbstractPattern::AttrLetter              = QStringLiteral("letter");
const QString VAbstractPattern::AttrAnnotation          = QStringLiteral("annotation");
const QString VAbstractPattern::AttrOrientation         = QStringLiteral("orientation");
const QString VAbstractPattern::AttrRotationWay         = QStringLiteral("rotationWay");
const QString VAbstractPattern::AttrTilt                = QStringLiteral("tilt");
const QString VAbstractPattern::AttrFoldPosition        = QStringLiteral("foldPosition");
const QString VAbstractPattern::AttrQuantity            = QStringLiteral("quantity");
const QString VAbstractPattern::AttrOnFold              = QStringLiteral("onFold");
const QString VAbstractPattern::AttrDateFormat          = QStringLiteral("dateFormat");
const QString VAbstractPattern::AttrTimeFormat          = QStringLiteral("timeFormat");
const QString VAbstractPattern::AttrArrows              = QStringLiteral("arrows");
const QString VAbstractPattern::AttrArrowLength         = QStringLiteral("arrowLength");
const QString VAbstractPattern::AttrNodeReverse         = QStringLiteral("reverse");
const QString VAbstractPattern::AttrNodeExcluded        = QStringLiteral("excluded");
const QString VAbstractPattern::AttrNodeIsNotch         = QStringLiteral("notch");
const QString VAbstractPattern::AttrNodeNotchType       = QStringLiteral("notchType");
const QString VAbstractPattern::AttrNodeNotchSubType    = QStringLiteral("notchSubtype");
const QString VAbstractPattern::AttrNodeShowNotch       = QStringLiteral("showNotch");
const QString VAbstractPattern::AttrNodeShowSecondNotch = QStringLiteral("showSecondNotch");
const QString VAbstractPattern::AttrNodeNotchLength     = QStringLiteral("notchLength");
const QString VAbstractPattern::AttrNodeNotchWidth      = QStringLiteral("notchWidth");
const QString VAbstractPattern::AttrNodeNotchAngle      = QStringLiteral("notchAngle");
const QString VAbstractPattern::AttrNodeNotchCount      = QStringLiteral("notchCount");
const QString VAbstractPattern::AttrSABefore            = QStringLiteral("before");
const QString VAbstractPattern::AttrSAAfter             = QStringLiteral("after");
const QString VAbstractPattern::AttrStart               = QStringLiteral("start");
const QString VAbstractPattern::AttrPath                = QStringLiteral("path");
const QString VAbstractPattern::AttrEnd                 = QStringLiteral("end");
const QString VAbstractPattern::AttrIncludeAs           = QStringLiteral("includeAs");
const QString VAbstractPattern::AttrWidth               = QStringLiteral("width");
const QString VAbstractPattern::AttrRotation            = QStringLiteral("rotation");
const QString VAbstractPattern::AttrFirstPiece          = QStringLiteral("firstPiece");
const QString VAbstractPattern::AttrFirstStart          = QStringLiteral("firstStart");
const QString VAbstractPattern::AttrFirstEnd            = QStringLiteral("firstEnd");
const QString VAbstractPattern::AttrSecondPiece         = QStringLiteral("secondPiece");
const QString VAbstractPattern::AttrSecondStart         = QStringLiteral("secondStart");
const QString VAbstractPattern::AttrSecondEnd           = QStringLiteral("secondEnd");
const QString VAbstractPattern::AttrFirstBackward       = QStringLiteral("firstBackward");
const QString VAbstractPattern::AttrSecondBackward      = QStringLiteral("secondBackward");
const QString VAbstractPattern::AttrBackward            = QStringLiteral("backward");
const QString VAbstractPattern::AttrPiece               = QStringLiteral("piece");
const QString VAbstractPattern::AttrPart                = QStringLiteral("part");
const QString VAbstractPattern::AttrDefault             = QStringLiteral("default");
const QString VAbstractPattern::AttrStitched            = QStringLiteral("stitched");
const QString VAbstractPattern::AttrStyle               = QStringLiteral("style");
const QString VAbstractPattern::AttrGender              = QStringLiteral("gender");
const QString VAbstractPattern::AttrSize                = QStringLiteral("size");
const QString VAbstractPattern::AttrBust                = QStringLiteral("bust");
const QString VAbstractPattern::AttrWaist               = QStringLiteral("waist");
const QString VAbstractPattern::AttrHip                 = QStringLiteral("hip");
const QString VAbstractPattern::AttrTurnedOver          = QStringLiteral("turnedOver");
const QString VAbstractPattern::AttrArrangementPoint    = QStringLiteral("point");
const QString VAbstractPattern::AttrDistance            = QStringLiteral("distance");
const QString VAbstractPattern::AttrLean                = QStringLiteral("lean");
const QString VAbstractPattern::AttrSwing               = QStringLiteral("swing");
const QString VAbstractPattern::AttrNumber              = QStringLiteral("number");
const QString VAbstractPattern::AttrRatio               = QStringLiteral("ratio");
const QString VAbstractPattern::AttrShrinkageWeft       = QStringLiteral("shrinkageWeft");
const QString VAbstractPattern::AttrShrinkageWarp       = QStringLiteral("shrinkageWarp");
const QString VAbstractPattern::AttrWeight              = QStringLiteral("weight");
const QString VAbstractPattern::AttrWarp                = QStringLiteral("warp");
const QString VAbstractPattern::AttrWeft                = QStringLiteral("weft");
const QString VAbstractPattern::AttrBias                = QStringLiteral("bias");
const QString VAbstractPattern::AttrBendingWarp         = QStringLiteral("bendingWarp");
const QString VAbstractPattern::AttrBendingWeft         = QStringLiteral("bendingWeft");
const QString VAbstractPattern::AttrThickness           = QStringLiteral("thickness");
const QString VAbstractPattern::AttrAvatar              = QStringLiteral("avatar");
const QString VAbstractPattern::AttrEdgeLength          = QStringLiteral("edgeLength");
const QString VAbstractPattern::AttrCopy                = QStringLiteral("copy");
const QString VAbstractPattern::AttrAtX                 = QStringLiteral("atX");
const QString VAbstractPattern::AttrAtY                 = QStringLiteral("atY");
const QString VAbstractPattern::AttrAtZ                 = QStringLiteral("atZ");

const QString VAbstractPattern::AttrAll                 = QStringLiteral("all");

const QString VAbstractPattern::AttrH50                 = QStringLiteral("h50");
const QString VAbstractPattern::AttrH56                 = QStringLiteral("h56");
const QString VAbstractPattern::AttrH62                 = QStringLiteral("h62");
const QString VAbstractPattern::AttrH68                 = QStringLiteral("h68");
const QString VAbstractPattern::AttrH74                 = QStringLiteral("h74");
const QString VAbstractPattern::AttrH80                 = QStringLiteral("h80");
const QString VAbstractPattern::AttrH86                 = QStringLiteral("h86");
const QString VAbstractPattern::AttrH92                 = QStringLiteral("h92");
const QString VAbstractPattern::AttrH98                 = QStringLiteral("h98");
const QString VAbstractPattern::AttrH104                = QStringLiteral("h104");
const QString VAbstractPattern::AttrH110                = QStringLiteral("h110");
const QString VAbstractPattern::AttrH116                = QStringLiteral("h116");
const QString VAbstractPattern::AttrH122                = QStringLiteral("h122");
const QString VAbstractPattern::AttrH128                = QStringLiteral("h128");
const QString VAbstractPattern::AttrH134                = QStringLiteral("h134");
const QString VAbstractPattern::AttrH140                = QStringLiteral("h140");
const QString VAbstractPattern::AttrH146                = QStringLiteral("h146");
const QString VAbstractPattern::AttrH152                = QStringLiteral("h152");
const QString VAbstractPattern::AttrH158                = QStringLiteral("h158");
const QString VAbstractPattern::AttrH164                = QStringLiteral("h164");
const QString VAbstractPattern::AttrH170                = QStringLiteral("h170");
const QString VAbstractPattern::AttrH176                = QStringLiteral("h176");
const QString VAbstractPattern::AttrH182                = QStringLiteral("h182");
const QString VAbstractPattern::AttrH188                = QStringLiteral("h188");
const QString VAbstractPattern::AttrH194                = QStringLiteral("h194");
const QString VAbstractPattern::AttrH200                = QStringLiteral("h200");

const QString VAbstractPattern::AttrS22                 = QStringLiteral("s22");
const QString VAbstractPattern::AttrS24                 = QStringLiteral("s24");
const QString VAbstractPattern::AttrS26                 = QStringLiteral("s26");
const QString VAbstractPattern::AttrS28                 = QStringLiteral("s28");
const QString VAbstractPattern::AttrS30                 = QStringLiteral("s30");
const QString VAbstractPattern::AttrS32                 = QStringLiteral("s32");
const QString VAbstractPattern::AttrS34                 = QStringLiteral("s34");
const QString VAbstractPattern::AttrS36                 = QStringLiteral("s36");
const QString VAbstractPattern::AttrS38                 = QStringLiteral("s38");
const QString VAbstractPattern::AttrS40                 = QStringLiteral("s40");
const QString VAbstractPattern::AttrS42                 = QStringLiteral("s42");
const QString VAbstractPattern::AttrS44                 = QStringLiteral("s44");
const QString VAbstractPattern::AttrS46                 = QStringLiteral("s46");
const QString VAbstractPattern::AttrS48                 = QStringLiteral("s48");
const QString VAbstractPattern::AttrS50                 = QStringLiteral("s50");
const QString VAbstractPattern::AttrS52                 = QStringLiteral("s52");
const QString VAbstractPattern::AttrS54                 = QStringLiteral("s54");
const QString VAbstractPattern::AttrS56                 = QStringLiteral("s56");
const QString VAbstractPattern::AttrS58                 = QStringLiteral("s58");
const QString VAbstractPattern::AttrS60                 = QStringLiteral("s60");
const QString VAbstractPattern::AttrS62                 = QStringLiteral("s62");
const QString VAbstractPattern::AttrS64                 = QStringLiteral("s64");
const QString VAbstractPattern::AttrS66                 = QStringLiteral("s66");
const QString VAbstractPattern::AttrS68                 = QStringLiteral("s68");
const QString VAbstractPattern::AttrS70                 = QStringLiteral("s70");
const QString VAbstractPattern::AttrS72                 = QStringLiteral("s72");

const QString VAbstractPattern::AttrCustom              = QStringLiteral("custom");
const QString VAbstractPattern::AttrDefHeight           = QStringLiteral("defHeight");
const QString VAbstractPattern::AttrDefSize             = QStringLiteral("defSize");
const QString VAbstractPattern::AttrExtension           = QStringLiteral("extension");

const QString VAbstractPattern::VariableName           = QStringLiteral("name");
const QString VAbstractPattern::VariableFormula        = QStringLiteral("formula");
const QString VAbstractPattern::VariableDescription    = QStringLiteral("description");

const QString VAbstractPattern::NodeArc                 = QStringLiteral("NodeArc");
const QString VAbstractPattern::NodeElArc               = QStringLiteral("NodeElArc");
const QString VAbstractPattern::NodePoint               = QStringLiteral("NodePoint");
const QString VAbstractPattern::NodeSpline              = QStringLiteral("NodeSpline");
const QString VAbstractPattern::NodeSplinePath          = QStringLiteral("NodeSplinePath");

QHash<quint32, VDataTool*> VAbstractPattern::tools = QHash<quint32, VDataTool*>();
QVector<VLabelTemplateLine> VAbstractPattern::patternLabelLines = QVector<VLabelTemplateLine>();
bool VAbstractPattern::patternLabelWasChanged = false;

namespace
{
// How the avatar's gender is stored.
const QString female_gender = QStringLiteral("female");
const QString male_gender = QStringLiteral("male");

//---------------------------------------------------------------------------------------------------------------------
void ReadExpressionAttribute(QVector<VFormulaField> &expressions, const QDomElement &element, const QString &attribute)
{
    VFormulaField formula;
    try
    {
        formula.expression = VDomDocument::GetParametrString(element, attribute);
    }
    catch (VExceptionEmptyParameter &error)
    {
        Q_UNUSED(error)
        return;
    }

    formula.element = element;
    formula.attribute = attribute;

    expressions.append(formula);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The image of a fabric an element of the fabrics holds, if any.
VFabricTexture readFabricTexture(const QDomElement& parent)
{
    VFabricTexture texture;
    const QDomElement element = parent.firstChildElement(VAbstractPattern::TagTexture);
    if (!element.isNull())
    {
        texture.image = QByteArray::fromBase64(element.text().toLatin1());
        texture.extension = VDomDocument::GetParametrString(element, VAbstractPattern::AttrExtension,
                                                            QStringLiteral("PNG"));
        texture.width = VDomDocument::GetParametrDouble(element, VAbstractPattern::AttrWidth, QStringLiteral("10"));
    }
    return texture;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief A draped cloth's vertices as the pattern keeps them: for each, five little-endian 32-bit floats, x and y in
/// the flat piece, then x, y and z as draped.
QByteArray clothBytes(const VDrapedCloth& cloth)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    const int count = qMin(cloth.rest.size() / 2, cloth.positions.size() / 3);
    for (int i = 0; i < count; ++i)
    {
        stream << cloth.rest.at(2 * i) << cloth.rest.at(2 * i + 1) << cloth.positions.at(3 * i)
               << cloth.positions.at(3 * i + 1) << cloth.positions.at(3 * i + 2);
    }
    return bytes;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Reads a draped cloth's vertices as clothBytes() keeps them; a vertex cut short is left out.
void readClothBytes(const QByteArray& bytes, VDrapedCloth& cloth)
{
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    float values[5];
    while (!stream.atEnd())
    {
        for (float& value : values)
        {
            stream >> value;
        }
        if (stream.status() != QDataStream::Ok)
        {
            break;
        }
        cloth.rest << values[0] << values[1];
        cloth.positions << values[2] << values[3] << values[4];
    }
}
}

//---------------------------------------------------------------------------------------------------------------------
VAbstractPattern::VAbstractPattern(QObject *parent)
    : QObject(parent)
    , VDomDocument()
    , m_activeDraftBlock(QString())
    , m_DefaultLineColor(qApp->Settings()->getDefaultLineColor())
    , m_DefaultLineWeight(qApp->Settings()->getDefaultLineWeight())
    , m_DefaultLineType(qApp->Settings()->getDefaultLineType())
    , defaultBasePoint(QString())
    , lastSavedExportFormat(QString())
    , m_cursorId(0)
    , toolsOnRemove(QVector<VDataTool*>())
    , m_history(QVector<VToolRecord>())
    , patternPieces(QStringList())
    , modified(false)
{}

//---------------------------------------------------------------------------------------------------------------------
QStringList VAbstractPattern::ListMeasurements() const
{
    QSet<QString> measurements;
    QSet<QString> others;

    const QStringList variables = listVariables();
    for (int i=0; i < variables.size(); ++i)
    {
        others.insert(variables.at(i));
    }

    const QVector<VFormulaField> expressions = ListExpressions();
    for (int i=0; i < expressions.size(); ++i)
    {
        // Eval formula
        QScopedPointer<qmu::QmuTokenParser> cal(new qmu::QmuTokenParser(expressions.at(i).expression, false, false));
        const QMap<int, QString> tokens = cal->GetTokens();// Tokens (variables, measurements)
        delete cal.take();

        const QList<QString> tValues = tokens.values();
        for (int j = 0; j < tValues.size(); ++j)
        {
            if (tValues.at(j) == QChar('-'))
            {
                continue;
            }

            if (measurements.contains(tValues.at(j)))
            {
                continue;
            }

            if (others.contains(tValues.at(j)))
            {
                continue;
            }

            if (IsVariable(tValues.at(j)) || IsPostfixOperator(tValues.at(j)) || IsFunction(tValues.at(j)))
            {
                others.insert(tValues.at(j));
            }
            else
            {
                measurements.insert(tValues.at(j));
            }
        }
    }

	return QStringList(measurements.values());
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief changeActiveDraftBlock set new active draft block name.
/// @param name new name.
/// @param parse parser file mode.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::changeActiveDraftBlock(const QString &name, const Document &parse)
{
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "name draft block is empty");
    if (draftBlockNameExists(name) && m_activeDraftBlock != name)
    {
        m_activeDraftBlock = name;
        if (parse == Document::FullParse)
        {
            emit activeDraftBlockChanged(name);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getActiveDraftBlockName return current draft block name.
/// @return draft block name.
//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getActiveDraftBlockName() const
{
    return m_activeDraftBlock;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getActiveDraftElement return draftBlock element for current draft block.
/// @param element draftBlock element.
/// @return true if found.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::getActiveDraftElement(QDomElement &element) const
{
    if (m_activeDraftBlock.isEmpty() == false)
    {
        const QDomNodeList elements = this->documentElement().elementsByTagName(TagDraftBlock);
        if (elements.size() == 0)
        {
            return false;
        }
        for ( qint32 i = 0; i < elements.count(); i++ )
        {
            element = elements.at( i ).toElement();
            if (element.isNull() == false)
            {
                const QString fieldName = element.attribute( AttrName );
                if ( fieldName == m_activeDraftBlock )
                {
                    return true;
                }
            }
        }
        element = QDomElement();
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief draftBlockNameExists check if draft block with this name exists.
/// @param name draft block name.
/// @return true if exist.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::draftBlockNameExists(const QString &name) const
{
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "draft block name is empty");
    const QDomNodeList elements = this->documentElement().elementsByTagName(TagDraftBlock);
    if (elements.size() == 0)
    {
        return false;
    }
    for ( qint32 i = 0; i < elements.count(); i++ )
    {
        const QDomElement elem = elements.at( i ).toElement();
        if (elem.isNull() == false)
        {
            if ( GetParametrString(elem, AttrName) == name )
            {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getActiveNodeElement find element in current draft block by name.
/// @param name name tag.
/// @param element element.
/// @return true if found.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::getActiveNodeElement(const QString &name, QDomElement &element) const
{
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "draft block name is empty");
    QDomElement draftBlockElement;
    if (getActiveDraftElement(draftBlockElement))
    {
        const QDomNodeList listElement = draftBlockElement.elementsByTagName(name);
        if (listElement.size() != 1)
        {
            return false;
        }
        element = listElement.at( 0 ).toElement();
        if (element.isNull() == false)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::parseGroups(const QDomElement &domElement)
{
    Q_ASSERT_X(!domElement.isNull(), Q_FUNC_INFO, "domElement is null");

    QMap<quint32, quint32> itemTool;
    QMap<quint32, bool> itemVisibility;

    QDomNode domNode = domElement.firstChild();
    while (domNode.isNull() == false)
    {
        if (domNode.isElement())
        {
            const QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                if (domElement.tagName() == TagGroup)
                {
                    VContainer::UpdateId(GetParametrUInt(domElement, AttrId, NULL_ID_STR));

                    const QPair<bool, QMap<quint32, quint32> > groupData = parseItemElement(domElement);
                    const QMap<quint32, quint32> group = groupData.second;
                    auto i = group.constBegin();
                    while (i != group.constEnd())
                    {
                        if (!itemTool.contains(i.key()))
                        {
                            itemTool.insert(i.key(), i.value());
                        }

                        const bool previous = itemVisibility.value(i.key(), false);
                        itemVisibility.insert(i.key(), previous || groupData.first);
                        ++i;
                    }
                }
            }
        }
        domNode = domNode.nextSibling();
    }

    auto i = itemTool.constBegin();
    while (i != itemTool.constEnd())
    {
        if (tools.contains(i.value()))
        {
            VDataTool* tool = tools.value(i.value());
            tool->GroupVisibility(i.key(), itemVisibility.value(i.key(), true));
        }
        ++i;
    }
}

//---------------------------------------------------------------------------------------------------------------------
int VAbstractPattern::draftBlockCount() const
{
    const QDomElement rootElement = this->documentElement();
    if (rootElement.isNull())
    {
        return 0;
    }

    return rootElement.elementsByTagName(TagDraftBlock).count();
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::getDraftBlockElement(const QString &name)
{
    if (name.isEmpty() == false)
    {
        const QDomNodeList elements = this->documentElement().elementsByTagName(TagDraftBlock);
        if (elements.size() == 0)
        {
            return QDomElement();
        }
        for ( qint32 i = 0; i < elements.count(); i++ )
        {
            QDomElement element = elements.at( i ).toElement();
            if (element.isNull() == false)
            {
                if ( element.attribute( AttrName ) == name )
                {
                    return element;
                }
            }
        }
    }
    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief renameDraftBlock change draft block name.
/// @param oldName old draft block name.
/// @param newName new draft block name.
/// @return true if success.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::renameDraftBlock(const QString &oldName, const QString &newName)
{
    Q_ASSERT_X(!newName.isEmpty(), Q_FUNC_INFO, "new name draft block is empty");
    Q_ASSERT_X(!oldName.isEmpty(), Q_FUNC_INFO, "old name draft block is empty");

    if (draftBlockNameExists(oldName) == false)
    {
        qDebug() << "Draft block does not exist with name" << oldName;
        return false;
    }

    if (draftBlockNameExists(newName))
    {
        qDebug() << "Draft block already exists with name" << newName;
        return false;
    }

    QDomElement ppElement = getDraftBlockElement(oldName);
    if (ppElement.isElement())
    {
        if (m_activeDraftBlock == oldName)
        {
            m_activeDraftBlock = newName;
        }
        ppElement.setAttribute(AttrName, newName);
        emit patternChanged(false);//For situation when we change name directly, without undocommands.
        emit draftBlockNameChanged(oldName, newName);
        return true;
    }
    else
    {
        qDebug() << "Can't find draft block node with name" << oldName << Q_FUNC_INFO;
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief appendDraftBlock add new draft block.
///
/// Method check if not exist draft block with the same name and change name active draft block name, send signal
/// about change draft block. Doen't add draft block to file structure. This task make SPoint tool.
/// @param name draft block name.
/// @return true if success.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::appendDraftBlock(const QString &name)
{
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "name draft block is empty");
    if (name.isEmpty())
    {
        return false;
    }
    if (draftBlockNameExists(name) == false)
    {
        setActiveDraftBlock(name);
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VAbstractPattern::getCursorId() const
{
    return m_cursorId;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setCursorId(const quint32 &toolId)
{
    if (m_cursorId != toolId)
    {
        m_cursorId = toolId;
        emit ChangedCursor(m_cursorId);
    }
}


//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setDefaultPen(Pen pen)
{
  m_DefaultLineColor  = pen.color;
  m_DefaultLineWeight = pen.lineWeight;
  m_DefaultLineType   = pen.lineType;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setDefaultBasePoint(QString basePoint)
{
    defaultBasePoint = basePoint;
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getDefaultLineColor() const
{
  return m_DefaultLineColor;
}

//---------------------------------------------------------------------------------------------------------------------
qreal VAbstractPattern::getDefaultLineWeight() const
{
  return m_DefaultLineWeight;
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getDefaultLineType() const
{
  return m_DefaultLineType;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getTool return tool from tool list.
/// @param id tool id.
/// @return tool.
//---------------------------------------------------------------------------------------------------------------------
VDataTool *VAbstractPattern::getTool(quint32 id)
{
    ToolExists(id);
    return tools.value(id);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief AddTool add tool to list tools.
/// @param id tool id.
/// @param tool tool.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::AddTool(quint32 id, VDataTool *tool)
{
    Q_ASSERT_X(id != 0, Q_FUNC_INFO, "id == 0");
    SCASSERT(tool != nullptr)
            tools.insert(id, tool);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::RemoveTool(quint32 id)
{
    tools.remove(id);
}

//---------------------------------------------------------------------------------------------------------------------
VPiecePath VAbstractPattern::ParsePieceNodes(const QDomElement &domElement)
{
    VPiecePath path;
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull())
        {
            path.Append(ParseSANode(element));
        }
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<CustomSARecord> VAbstractPattern::ParsePieceCSARecords(const QDomElement &domElement)
{
    QVector<CustomSARecord> records;
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull())
        {
            CustomSARecord record;
            record.startPoint = GetParametrUInt(element, VAbstractPattern::AttrStart, NULL_ID_STR);
            record.path = GetParametrUInt(element, VAbstractPattern::AttrPath, NULL_ID_STR);
            record.endPoint = GetParametrUInt(element, VAbstractPattern::AttrEnd, NULL_ID_STR);
            record.reverse = getParameterBool(element, VAbstractPattern::AttrNodeReverse, falseStr);
            record.includeType = static_cast<PiecePathIncludeType>(GetParametrUInt(element,
                                                                                   VAbstractPattern::AttrIncludeAs,
                                                                                   "1"));
            records.append(record);
        }
    }
    return records;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> VAbstractPattern::ParsePieceInternalPaths(const QDomElement &domElement)
{
    QVector<quint32> records;
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull())
        {
            const quint32 path = GetParametrUInt(element, VAbstractPattern::AttrPath, NULL_ID_STR);
            if (path > NULL_ID)
            {
                records.append(path);
            }
        }
    }
    return records;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> VAbstractPattern::ParsePieceAnchors(const QDomElement &domElement)
{
    QVector<quint32> records;
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull())
        {
            const quint32 path = element.text().toUInt();
            if (path > NULL_ID)
            {
                records.append(path);
            }
        }
    }
    return records;
}

//---------------------------------------------------------------------------------------------------------------------
VPieceNode VAbstractPattern::ParseSANode(const QDomElement &domElement)
{
    const quint32 id = VDomDocument::GetParametrUInt(domElement, AttrIdObject, NULL_ID_STR);
    const bool reverse = VDomDocument::GetParametrUInt(domElement, VAbstractPattern::AttrNodeReverse, "0");
    const bool excluded = VDomDocument::getParameterBool(domElement, VAbstractPattern::AttrNodeExcluded, falseStr);
    const QString saBefore = VDomDocument::GetParametrString(domElement, VAbstractPattern::AttrSABefore,
                                                             currentSeamAllowance);
    const QString saAfter = VDomDocument::GetParametrString(domElement, VAbstractPattern::AttrSAAfter,
                                                            currentSeamAllowance);
    const PieceNodeAngle angle = static_cast<PieceNodeAngle>(VDomDocument::GetParametrUInt(domElement, AttrAngle, "0"));

    const bool notch = VDomDocument::getParameterBool(domElement, VAbstractPattern::AttrNodeIsNotch, falseStr);
    const NotchType notchType = stringToNotchType(VDomDocument::GetParametrString(domElement,
                                                          VAbstractPattern::AttrNodeNotchType,strSlit));
    const NotchSubType notchSubType = stringToNotchSubType(VDomDocument::GetParametrString(domElement,
                                                          VAbstractPattern::AttrNodeNotchSubType, strStraightforward));

    const bool showNotch = VDomDocument::getParameterBool(domElement, VAbstractPattern::AttrNodeShowNotch,
                                                          trueStr);
    const bool showSecond = VDomDocument::getParameterBool(domElement, VAbstractPattern::AttrNodeShowSecondNotch,
                                                          trueStr);
    const qreal  notchLength = VDomDocument::GetParametrDouble(domElement, VAbstractPattern::AttrNodeNotchLength,
                                                               QString::number(qApp->Settings()->getDefaultNotchLength()));
    const qreal   notchWidth = VDomDocument::GetParametrDouble(domElement, VAbstractPattern::AttrNodeNotchWidth,
                                                               QString::number(qApp->Settings()->getDefaultNotchWidth()));
    const qreal   notchAngle = VDomDocument::GetParametrDouble(domElement, VAbstractPattern::AttrNodeNotchAngle, ".00");
    const quint32 notchCount = VDomDocument::GetParametrUInt(domElement,   VAbstractPattern::AttrNodeNotchCount, "1");


    const QString t = VDomDocument::GetParametrString(domElement, AttrType, VAbstractPattern::NodePoint);
    Tool tool;

    const QStringList types = QStringList() << VAbstractPattern::NodePoint
                                            << VAbstractPattern::NodeArc
                                            << VAbstractPattern::NodeSpline
                                            << VAbstractPattern::NodeSplinePath
                                            << VAbstractPattern::NodeElArc;

    switch (types.indexOf(t))
    {
        case 0: // VAbstractPattern::NodePoint
            tool = Tool::NodePoint;
            break;
        case 1: // VAbstractPattern::NodeArc
            tool = Tool::NodeArc;
            break;
        case 2: // VAbstractPattern::NodeSpline
            tool = Tool::NodeSpline;
            break;
        case 3: // VAbstractPattern::NodeSplinePath
            tool = Tool::NodeSplinePath;
            break;
        case 4: // NodeElArc
            tool = Tool::NodeElArc;
            break;
        default:
            VException e(QObject::tr("Wrong tag name '%1'.").arg(t));
            throw e;
    }
    VPieceNode node(id, tool, reverse);
    node.setBeforeSAFormula(saBefore);
    node.setAfterSAFormula(saAfter);
    node.SetAngleType(angle);
    node.SetExcluded(excluded);
    node.setNotch(notch);
    node.setNotchType(notchType);
    node.setNotchSubType(notchSubType);
    node.setShowNotch(showNotch);
    node.setShowSeamlineNotch(showSecond);
    node.setNotchLength(notchLength);
    node.setNotchWidth(notchWidth);
    node.setNotchAngle(notchAngle);
    node.setNotchCount(notchCount);
    return node;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::AddToolOnRemove(VDataTool *tool)
{
    SCASSERT(tool != nullptr)
    toolsOnRemove.append(tool);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getHistory return list with list of history records.
/// @return list of history records.
//---------------------------------------------------------------------------------------------------------------------
QVector<VToolRecord> *VAbstractPattern::getHistory()
{
    return &m_history;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VToolRecord> VAbstractPattern::getBlockHistory() const
{
    QVector<VToolRecord> draftBlockHistory;
    for (qint32 i = 0; i< m_history.size(); ++i)
    {
        const VToolRecord tool = m_history.at(i);
        if (tool.getDraftBlockName() != getActiveDraftBlockName())
        {
            continue;
        }
        draftBlockHistory.append(tool);
    }
    return draftBlockHistory;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief getToolDraftBlockName return the draft block a tool was created in, by its history id.
 * @param id tool id, e.g. VGObject::getIdTool() for one of its created objects.
 * @return draft block name, or an empty string if no history entry has this id.
 */
QString VAbstractPattern::getToolDraftBlockName(quint32 id) const
{
    for (qint32 i = 0; i < m_history.size(); ++i)
    {
        if (m_history.at(i).getId() == id)
        {
            return m_history.at(i).getDraftBlockName();
        }
    }
    return QString();
}

//---------------------------------------------------------------------------------------------------------------------
QMap<quint32, Tool> VAbstractPattern::getGroupObjHistory() const
{
    QMap<quint32, Tool> draftBlockHistory;
    for (qint32 i = 0; i< m_history.size(); ++i)
    {
        const VToolRecord tool = m_history.at(i);
        if (tool.getDraftBlockName() != getActiveDraftBlockName())
        {
            continue;
        }
        draftBlockHistory.insert(tool.getId(), tool.getTypeTool());
    }
    return draftBlockHistory;
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::MPath() const
{
    return UniqueTagText(TagMeasurements);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetMPath(const QString &path)
{
    if (setTagText(TagMeasurements, path))
    {
        emit patternChanged(false);
        patternLabelWasChanged = true;
    }
    else
    {
        qWarning() << "Can't save path to measurements" << Q_FUNC_INFO;
    }
}

//---------------------------------------------------------------------------------------------------------------------
quint32 VAbstractPattern::SiblingNodeId(const quint32 &nodeId) const
{
    quint32 siblingId = NULL_ID;

    const QVector<VToolRecord> history = getBlockHistory();
    for (qint32 i = 0; i < history.size(); ++i)
    {
        const VToolRecord tool = history.at(i);
        if (nodeId == tool.getId())
        {
            if (i == 0)
            {
                siblingId = NULL_ID;
            }
            else
            {
                for (qint32 j = i; j > 0; --j)
                {
                    const VToolRecord tool = history.at(j-1);
                    switch ( tool.getTypeTool() )
                    {
                        case Tool::Piece:
                        case Tool::Union:
                        case Tool::NodeArc:
                        case Tool::NodeElArc:
                        case Tool::NodePoint:
                        case Tool::NodeSpline:
                        case Tool::NodeSplinePath:
                            continue;
                        default:
                            siblingId = tool.getId();
                            j = 0;// break loop
                            break;
                    }
                }
            }
        }
    }
    return siblingId;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList VAbstractPattern::getPatternPieces() const
{
    return patternPieces;
}

//---------------------------------------------------------------------------------------------------------------------
QMap<GHeights, bool> VAbstractPattern::GetGradationHeights() const
{
    QMap<GHeights, bool> map;

    map.insert(GHeights::ALL, true);
    map.insert(GHeights::H50, true);
    map.insert(GHeights::H56, true);
    map.insert(GHeights::H62, true);
    map.insert(GHeights::H68, true);
    map.insert(GHeights::H74, true);
    map.insert(GHeights::H80, true);
    map.insert(GHeights::H86, true);
    map.insert(GHeights::H92, true);
    map.insert(GHeights::H98, true);
    map.insert(GHeights::H104, true);
    map.insert(GHeights::H110, true);
    map.insert(GHeights::H116, true);
    map.insert(GHeights::H122, true);
    map.insert(GHeights::H128, true);
    map.insert(GHeights::H134, true);
    map.insert(GHeights::H140, true);
    map.insert(GHeights::H146, true);
    map.insert(GHeights::H152, true);
    map.insert(GHeights::H158, true);
    map.insert(GHeights::H164, true);
    map.insert(GHeights::H170, true);
    map.insert(GHeights::H176, true);
    map.insert(GHeights::H182, true);
    map.insert(GHeights::H188, true);
    map.insert(GHeights::H194, true);
    map.insert(GHeights::H200, true);

    QDomNodeList tags = elementsByTagName(TagGradation);
    if (tags.size() == 0)
    {
        return map;
    }

    QStringList gTags = QStringList() << TagHeights << TagSizes;
    QDomNode domNode = tags.at(0).firstChild();
    while (domNode.isNull() == false)
    {
        if (domNode.isElement())
        {
            const QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                const QString defValue = trueStr;
                switch (gTags.indexOf(domElement.tagName()))
                {
                    case 0: // TagHeights
                        if (getParameterBool(domElement, AttrAll, defValue))
                        {
                            return map;
                        }
                        else
                        {
                            map.insert(GHeights::ALL, false);
                        }

                        map.insert(GHeights::H50, getParameterBool(domElement, AttrH50, defValue));
                        map.insert(GHeights::H56, getParameterBool(domElement, AttrH56, defValue));
                        map.insert(GHeights::H62, getParameterBool(domElement, AttrH62, defValue));
                        map.insert(GHeights::H68, getParameterBool(domElement, AttrH68, defValue));
                        map.insert(GHeights::H74, getParameterBool(domElement, AttrH74, defValue));
                        map.insert(GHeights::H80, getParameterBool(domElement, AttrH80, defValue));
                        map.insert(GHeights::H86, getParameterBool(domElement, AttrH86, defValue));
                        map.insert(GHeights::H92, getParameterBool(domElement, AttrH92, defValue));
                        map.insert(GHeights::H98, getParameterBool(domElement, AttrH98, defValue));
                        map.insert(GHeights::H104, getParameterBool(domElement, AttrH104, defValue));
                        map.insert(GHeights::H110, getParameterBool(domElement, AttrH110, defValue));
                        map.insert(GHeights::H116, getParameterBool(domElement, AttrH116, defValue));
                        map.insert(GHeights::H122, getParameterBool(domElement, AttrH122, defValue));
                        map.insert(GHeights::H128, getParameterBool(domElement, AttrH128, defValue));
                        map.insert(GHeights::H134, getParameterBool(domElement, AttrH134, defValue));
                        map.insert(GHeights::H140, getParameterBool(domElement, AttrH140, defValue));
                        map.insert(GHeights::H146, getParameterBool(domElement, AttrH146, defValue));
                        map.insert(GHeights::H152, getParameterBool(domElement, AttrH152, defValue));
                        map.insert(GHeights::H158, getParameterBool(domElement, AttrH158, defValue));
                        map.insert(GHeights::H164, getParameterBool(domElement, AttrH164, defValue));
                        map.insert(GHeights::H170, getParameterBool(domElement, AttrH170, defValue));
                        map.insert(GHeights::H176, getParameterBool(domElement, AttrH176, defValue));
                        map.insert(GHeights::H182, getParameterBool(domElement, AttrH182, defValue));
                        map.insert(GHeights::H188, getParameterBool(domElement, AttrH188, defValue));
                        map.insert(GHeights::H194, getParameterBool(domElement, AttrH194, defValue));
                        map.insert(GHeights::H200, getParameterBool(domElement, AttrH200, defValue));
                        return map;
                    case 1: // TagSizes
                    default:
                        break;
                }
            }
        }
        domNode = domNode.nextSibling();
    }
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetGradationHeights(const QMap<GHeights, bool> &options)
{
    CheckTagExists(TagGradation);
    QDomNodeList tags = elementsByTagName(TagGradation);
    if (tags.isEmpty())
    {
        qWarning() << "Can't save tag " << TagGradation << Q_FUNC_INFO;
        return;
    }

    QStringList gTags = QStringList() << TagHeights << TagSizes;
    QDomNode domNode = tags.at(0).firstChild();
    while (domNode.isNull() == false)
    {
        if (domNode.isElement())
        {
            QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                switch (gTags.indexOf(domElement.tagName()))
                {
                    case 0: // TagHeights
                        SetAttribute(domElement, AttrAll, options.value(GHeights::ALL));
                        if (options.value(GHeights::ALL))
                        {
                            domElement.removeAttribute(AttrH50);
                            domElement.removeAttribute(AttrH56);
                            domElement.removeAttribute(AttrH62);
                            domElement.removeAttribute(AttrH68);
                            domElement.removeAttribute(AttrH74);
                            domElement.removeAttribute(AttrH80);
                            domElement.removeAttribute(AttrH86);
                            domElement.removeAttribute(AttrH92);
                            domElement.removeAttribute(AttrH98);
                            domElement.removeAttribute(AttrH104);
                            domElement.removeAttribute(AttrH110);
                            domElement.removeAttribute(AttrH116);
                            domElement.removeAttribute(AttrH122);
                            domElement.removeAttribute(AttrH128);
                            domElement.removeAttribute(AttrH134);
                            domElement.removeAttribute(AttrH140);
                            domElement.removeAttribute(AttrH146);
                            domElement.removeAttribute(AttrH152);
                            domElement.removeAttribute(AttrH158);
                            domElement.removeAttribute(AttrH164);
                            domElement.removeAttribute(AttrH170);
                            domElement.removeAttribute(AttrH176);
                            domElement.removeAttribute(AttrH182);
                            domElement.removeAttribute(AttrH188);
                            domElement.removeAttribute(AttrH194);
                            domElement.removeAttribute(AttrH200);
                        }
                        else
                        {
                            SetAttribute(domElement, AttrH50, options.value(GHeights::H50));
                            SetAttribute(domElement, AttrH56, options.value(GHeights::H56));
                            SetAttribute(domElement, AttrH62, options.value(GHeights::H62));
                            SetAttribute(domElement, AttrH68, options.value(GHeights::H68));
                            SetAttribute(domElement, AttrH74, options.value(GHeights::H74));
                            SetAttribute(domElement, AttrH80, options.value(GHeights::H80));
                            SetAttribute(domElement, AttrH86, options.value(GHeights::H86));
                            SetAttribute(domElement, AttrH92, options.value(GHeights::H92));
                            SetAttribute(domElement, AttrH98, options.value(GHeights::H98));
                            SetAttribute(domElement, AttrH104, options.value(GHeights::H104));
                            SetAttribute(domElement, AttrH110, options.value(GHeights::H110));
                            SetAttribute(domElement, AttrH116, options.value(GHeights::H116));
                            SetAttribute(domElement, AttrH122, options.value(GHeights::H122));
                            SetAttribute(domElement, AttrH128, options.value(GHeights::H128));
                            SetAttribute(domElement, AttrH134, options.value(GHeights::H134));
                            SetAttribute(domElement, AttrH140, options.value(GHeights::H140));
                            SetAttribute(domElement, AttrH146, options.value(GHeights::H146));
                            SetAttribute(domElement, AttrH152, options.value(GHeights::H152));
                            SetAttribute(domElement, AttrH158, options.value(GHeights::H158));
                            SetAttribute(domElement, AttrH164, options.value(GHeights::H164));
                            SetAttribute(domElement, AttrH170, options.value(GHeights::H170));
                            SetAttribute(domElement, AttrH176, options.value(GHeights::H176));
                            SetAttribute(domElement, AttrH182, options.value(GHeights::H182));
                            SetAttribute(domElement, AttrH188, options.value(GHeights::H188));
                            SetAttribute(domElement, AttrH194, options.value(GHeights::H194));
                            SetAttribute(domElement, AttrH200, options.value(GHeights::H200));
                        }

                        modified = true;
                        emit patternChanged(false);
                        return;
                    case 1: // TagSizes
                    default:
                        break;
                }
            }
        }
        domNode = domNode.nextSibling();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QMap<GSizes, bool> VAbstractPattern::GetGradationSizes() const
{
    QMap<GSizes, bool> map;
    map.insert(GSizes::ALL, true);
    map.insert(GSizes::S22, true);
    map.insert(GSizes::S24, true);
    map.insert(GSizes::S26, true);
    map.insert(GSizes::S28, true);
    map.insert(GSizes::S30, true);
    map.insert(GSizes::S32, true);
    map.insert(GSizes::S34, true);
    map.insert(GSizes::S36, true);
    map.insert(GSizes::S38, true);
    map.insert(GSizes::S40, true);
    map.insert(GSizes::S42, true);
    map.insert(GSizes::S44, true);
    map.insert(GSizes::S46, true);
    map.insert(GSizes::S48, true);
    map.insert(GSizes::S50, true);
    map.insert(GSizes::S52, true);
    map.insert(GSizes::S54, true);
    map.insert(GSizes::S56, true);
    map.insert(GSizes::S58, true);
    map.insert(GSizes::S60, true);
    map.insert(GSizes::S62, true);
    map.insert(GSizes::S64, true);
    map.insert(GSizes::S66, true);
    map.insert(GSizes::S68, true);
    map.insert(GSizes::S70, true);
    map.insert(GSizes::S72, true);

    QDomNodeList tags = elementsByTagName(TagGradation);
    if (tags.size() == 0)
    {
        return map;
    }

    QStringList gTags = QStringList() << TagHeights << TagSizes;
    QDomNode domNode = tags.at(0).firstChild();
    while (domNode.isNull() == false)
    {
        if (domNode.isElement())
        {
            const QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                const QString defValue = trueStr;
                switch (gTags.indexOf(domElement.tagName()))
                {
                    case 1: // TagSizes
                        if (getParameterBool(domElement, AttrAll, defValue))
                        {
                            return map;
                        }
                        else
                        {
                            map.insert(GSizes::ALL, false);
                        }

                        map.insert(GSizes::S22, getParameterBool(domElement, AttrS22, defValue));
                        map.insert(GSizes::S24, getParameterBool(domElement, AttrS24, defValue));
                        map.insert(GSizes::S26, getParameterBool(domElement, AttrS26, defValue));
                        map.insert(GSizes::S28, getParameterBool(domElement, AttrS28, defValue));
                        map.insert(GSizes::S30, getParameterBool(domElement, AttrS30, defValue));
                        map.insert(GSizes::S32, getParameterBool(domElement, AttrS32, defValue));
                        map.insert(GSizes::S34, getParameterBool(domElement, AttrS34, defValue));
                        map.insert(GSizes::S36, getParameterBool(domElement, AttrS36, defValue));
                        map.insert(GSizes::S38, getParameterBool(domElement, AttrS38, defValue));
                        map.insert(GSizes::S40, getParameterBool(domElement, AttrS40, defValue));
                        map.insert(GSizes::S42, getParameterBool(domElement, AttrS42, defValue));
                        map.insert(GSizes::S44, getParameterBool(domElement, AttrS44, defValue));
                        map.insert(GSizes::S46, getParameterBool(domElement, AttrS46, defValue));
                        map.insert(GSizes::S48, getParameterBool(domElement, AttrS48, defValue));
                        map.insert(GSizes::S50, getParameterBool(domElement, AttrS50, defValue));
                        map.insert(GSizes::S52, getParameterBool(domElement, AttrS52, defValue));
                        map.insert(GSizes::S54, getParameterBool(domElement, AttrS54, defValue));
                        map.insert(GSizes::S56, getParameterBool(domElement, AttrS56, defValue));
                        map.insert(GSizes::S58, getParameterBool(domElement, AttrS58, defValue));
                        map.insert(GSizes::S60, getParameterBool(domElement, AttrS60, defValue));
                        map.insert(GSizes::S62, getParameterBool(domElement, AttrS62, defValue));
                        map.insert(GSizes::S64, getParameterBool(domElement, AttrS64, defValue));
                        map.insert(GSizes::S66, getParameterBool(domElement, AttrS66, defValue));
                        map.insert(GSizes::S68, getParameterBool(domElement, AttrS68, defValue));
                        map.insert(GSizes::S70, getParameterBool(domElement, AttrS70, defValue));
                        map.insert(GSizes::S72, getParameterBool(domElement, AttrS72, defValue));
                        return map;
                    case 0: // TagHeights
                    default:
                        break;
                }
            }
        }
        domNode = domNode.nextSibling();
    }
    return map;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetGradationSizes(const QMap<GSizes, bool> &options)
{
    CheckTagExists(TagGradation);
    QDomNodeList tags = elementsByTagName(TagGradation);
    if (tags.isEmpty())
    {
        qWarning() << "Can't save tag " << TagGradation << Q_FUNC_INFO;
        return;
    }

    QStringList gTags = QStringList() << TagHeights << TagSizes;
    QDomNode domNode = tags.at(0).firstChild();
    while (domNode.isNull() == false)
    {
        if (domNode.isElement())
        {
            QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                switch (gTags.indexOf(domElement.tagName()))
                {
                    case 1: // TagSizes
                        SetAttribute(domElement, AttrAll, options.value(GSizes::ALL));
                        if (options.value(GSizes::ALL))
                        {
                            domElement.removeAttribute(AttrS22);
                            domElement.removeAttribute(AttrS24);
                            domElement.removeAttribute(AttrS26);
                            domElement.removeAttribute(AttrS28);
                            domElement.removeAttribute(AttrS30);
                            domElement.removeAttribute(AttrS32);
                            domElement.removeAttribute(AttrS34);
                            domElement.removeAttribute(AttrS36);
                            domElement.removeAttribute(AttrS38);
                            domElement.removeAttribute(AttrS40);
                            domElement.removeAttribute(AttrS42);
                            domElement.removeAttribute(AttrS44);
                            domElement.removeAttribute(AttrS46);
                            domElement.removeAttribute(AttrS48);
                            domElement.removeAttribute(AttrS50);
                            domElement.removeAttribute(AttrS52);
                            domElement.removeAttribute(AttrS54);
                            domElement.removeAttribute(AttrS56);
                            domElement.removeAttribute(AttrS58);
                            domElement.removeAttribute(AttrS60);
                            domElement.removeAttribute(AttrS62);
                            domElement.removeAttribute(AttrS64);
                            domElement.removeAttribute(AttrS66);
                            domElement.removeAttribute(AttrS68);
                            domElement.removeAttribute(AttrS70);
                            domElement.removeAttribute(AttrS72);
                        }
                        else
                        {
                            SetAttribute(domElement, AttrS22, options.value(GSizes::S22));
                            SetAttribute(domElement, AttrS24, options.value(GSizes::S24));
                            SetAttribute(domElement, AttrS26, options.value(GSizes::S26));
                            SetAttribute(domElement, AttrS28, options.value(GSizes::S28));
                            SetAttribute(domElement, AttrS30, options.value(GSizes::S30));
                            SetAttribute(domElement, AttrS32, options.value(GSizes::S32));
                            SetAttribute(domElement, AttrS34, options.value(GSizes::S34));
                            SetAttribute(domElement, AttrS36, options.value(GSizes::S36));
                            SetAttribute(domElement, AttrS38, options.value(GSizes::S38));
                            SetAttribute(domElement, AttrS40, options.value(GSizes::S40));
                            SetAttribute(domElement, AttrS42, options.value(GSizes::S42));
                            SetAttribute(domElement, AttrS44, options.value(GSizes::S44));
                            SetAttribute(domElement, AttrS46, options.value(GSizes::S46));
                            SetAttribute(domElement, AttrS48, options.value(GSizes::S48));
                            SetAttribute(domElement, AttrS50, options.value(GSizes::S50));
                            SetAttribute(domElement, AttrS52, options.value(GSizes::S52));
                            SetAttribute(domElement, AttrS54, options.value(GSizes::S54));
                            SetAttribute(domElement, AttrS56, options.value(GSizes::S56));
                            SetAttribute(domElement, AttrS58, options.value(GSizes::S58));
                            SetAttribute(domElement, AttrS60, options.value(GSizes::S60));
                            SetAttribute(domElement, AttrS62, options.value(GSizes::S62));
                            SetAttribute(domElement, AttrS64, options.value(GSizes::S64));
                            SetAttribute(domElement, AttrS66, options.value(GSizes::S66));
                            SetAttribute(domElement, AttrS68, options.value(GSizes::S68));
                            SetAttribute(domElement, AttrS70, options.value(GSizes::S70));
                            SetAttribute(domElement, AttrS72, options.value(GSizes::S72));
                        }

                        modified = true;
                        emit patternChanged(false);
                        return;
                    case 0: // TagHeights
                    default:
                        break;
                }
            }
        }
        domNode = domNode.nextSibling();
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetDescription() const
{
    return UniqueTagText(TagDescription);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetDescription(const QString &text)
{
    CheckTagExists(TagDescription);
    setTagText(TagDescription, text);
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetNotes() const
{
    return UniqueTagText(TagNotes);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetNotes(const QString &text)
{
    CheckTagExists(TagNotes);
    setTagText(TagNotes, text);
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetPatternName() const
{
    return UniqueTagText(TagPatternName);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetPatternName(const QString &qsName)
{
    CheckTagExists(TagPatternName);
    setTagText(TagPatternName, qsName);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetCompanyName() const
{
    return UniqueTagText(TagCompanyName);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetCompanyName(const QString &qsName)
{
    CheckTagExists(TagCompanyName);
    setTagText(TagCompanyName, qsName);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetPatternNumber() const
{
    return UniqueTagText(TagPatternNum);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetPatternNumber(const QString &qsNum)
{
    CheckTagExists(TagPatternNum);
    setTagText(TagPatternNum, qsNum);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetCustomerName() const
{
    return UniqueTagText(TagCustomerName);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetCustomerName(const QString &qsName)
{
    CheckTagExists(TagCustomerName);
    setTagText(TagCustomerName, qsName);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetLabelDateFormat() const
{
    QString globalLabelDateFormat = qApp->Settings()->GetLabelDateFormat();

    const QDomNodeList list = elementsByTagName(TagPatternLabel);
    if (list.isEmpty())
    {
        return globalLabelDateFormat;
    }

    QDomElement tag = list.at(0).toElement();
    return GetParametrString(tag, AttrDateFormat, globalLabelDateFormat);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetLabelDateFormat(const QString &format)
{
    QDomElement tag = CheckTagExists(TagPatternLabel);
    SetAttribute(tag, AttrDateFormat, format);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetLabelTimeFormat() const
{
    QString globalLabelTimeFormat = qApp->Settings()->GetLabelTimeFormat();

    const QDomNodeList list = elementsByTagName(TagPatternLabel);
    if (list.isEmpty())
    {
        return globalLabelTimeFormat;
    }

    QDomElement tag = list.at(0).toElement();
    return GetParametrString(tag, AttrTimeFormat, globalLabelTimeFormat);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetLabelTimeFormat(const QString &format)
{
    QDomElement tag = CheckTagExists(TagPatternLabel);
    SetAttribute(tag, AttrTimeFormat, format);
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setPatternLabelTemplate(const QVector<VLabelTemplateLine> &lines)
{
    QDomElement tag = CheckTagExists(TagPatternLabel);
    RemoveAllChildren(tag);
    SetLabelTemplate(tag, lines);
    patternLabelLines = lines;
    patternLabelWasChanged = true;
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VLabelTemplateLine> VAbstractPattern::getPatternLabelTemplate() const
{
    if (patternLabelLines.isEmpty())
    {
        const QDomNodeList list = elementsByTagName(TagPatternLabel);
        if (list.isEmpty())
        {
            return QVector<VLabelTemplateLine>();
        }

        patternLabelLines = GetLabelTemplate(list.at(0).toElement());
    }

    return patternLabelLines;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetPatternWasChanged(bool changed)
{
    patternLabelWasChanged = changed;
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::GetPatternWasChanged() const
{
    return patternLabelWasChanged;
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetImage() const
{
    return UniqueTagText(TagImage);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetImageExtension() const
{
    const QString defExt =  QStringLiteral("PNG");
    const QDomNodeList nodeList = this->elementsByTagName(TagImage);
    if (nodeList.isEmpty())
    {
        return defExt;
    }
    else
    {
        const QDomNode domNode = nodeList.at(0);
        if (domNode.isNull() == false && domNode.isElement())
        {
            const QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                const QString ext = domElement.attribute(AttrExtension, defExt);
                return ext;
            }
        }
    }
    return defExt;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetImage(const QString &text, const QString &extension)
{
    QDomElement imageElement = CheckTagExists(TagImage);
    setTagText(imageElement, text);
    CheckTagExists(TagImage).setAttribute(AttrExtension, extension);
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::DeleteImage()
{
    QDomElement pattern = documentElement();
    pattern.removeChild(CheckTagExists(TagImage));
    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::GetVersion() const
{
    return UniqueTagText(TagVersion, VPatternConverter::PatternMaxVerStr);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setVersion()
{
    setTagText(TagVersion, VPatternConverter::PatternMaxVerStr);
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getOpItems get vector of operation tool obects.
/// @param toolId operation tool id.
/// @param itemType  type of item - either source or destination.
/// @return vector of item element object ids.
//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> VAbstractPattern::getOpItems(const quint32 &toolId, const QString &itemType)
{
    QVector<quint32> items;
    quint32 objId;
    const QDomElement domElement = elementById(toolId);
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement dataElement = nodeList.at(i).toElement();
        if (!dataElement.isNull() && dataElement.tagName() == itemType)
        {
            const QDomNodeList srcList = dataElement.childNodes();
            for (qint32 j = 0; j < srcList.size(); ++j)
            {
                const QDomElement element = srcList.at(j).toElement();
                if (!element.isNull())
                {
                    objId = VDomDocument::GetParametrUInt(element, AttrIdObject, NULL_ID_STR);
                    items.append(objId);
                }
            }
        }
    }

    return items;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> VAbstractPattern::getDartItems(const quint32 &toolId)
{
    QVector<quint32> items;
    quint32 objId;
    const QDomElement domElement = elementById(toolId);

    objId = VDomDocument::GetParametrUInt(domElement, AttrPoint1, NULL_ID_STR);
    items.append(objId);

    objId = VDomDocument::GetParametrUInt(domElement, AttrPoint2, NULL_ID_STR);
    items.append(objId);

    return items;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief haveLiteChange we have unsaved change.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::haveLiteChange()
{
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::NeedFullParsing()
{
    emit UndoCommand();
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::ClearScene()
{
    emit ClearMainWindow();
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::updatePieceList(quint32 id)
{
    emit UpdateInLayoutList(id);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::selectedPiece(quint32 id)
{
    emit showPiece(id);
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::ToolExists(const quint32 &id)
{
    if (tools.contains(id) == false)
    {
        throw VExceptionBadId(tr("Can't find tool in table."), id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
VPiecePath VAbstractPattern::ParsePathNodes(const QDomElement &domElement)
{
    VPiecePath path;
    const QDomNodeList nodeList = domElement.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull() && element.tagName() == VAbstractPattern::TagNode)
        {
            path.Append(ParseSANode(element));
        }
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief setActiveDraftBlock set current draft block.
/// @param name draft block name.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setActiveDraftBlock(const QString &name)
{
    Q_ASSERT_X(!name.isEmpty(), Q_FUNC_INFO, "name draft block is empty");
    m_activeDraftBlock = name;
    emit activeDraftBlockChanged(name);
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::CheckTagExists(const QString &tag)
{
    const QDomNodeList list = elementsByTagName(tag);
    QDomElement element;
    if (list.isEmpty())
    {
        const QStringList tags = QStringList() << TagUnit << TagImage << TagDescription << TagNotes
                                         << TagGradation << TagPatternName << TagPatternNum << TagCompanyName
                                         << TagCustomerName << TagPatternLabel << TagDraftImages;
        switch (tags.indexOf(tag))
        {
            case 1: //TagImage
                element = createElement(TagImage);
                break;
            case 2: //TagDescription
                element = createElement(TagDescription);
                break;
            case 3: //TagNotes
                element = createElement(TagNotes);
                break;
            case 4: //TagGradation
            {
                element = createElement(TagGradation);

                QDomElement heights = createElement(TagHeights);
                heights.setAttribute(AttrAll, QLatin1String("true"));
                element.appendChild(heights);

                QDomElement sizes = createElement(TagSizes);
                sizes.setAttribute(AttrAll, QLatin1String("true"));
                element.appendChild(sizes);
                break;
            }
            case 5: // TagPatternName
                element = createElement(TagPatternName);
                break;
            case 6: // TagPatternNum
                element = createElement(TagPatternNum);
                break;
            case 7: // TagCompanyName
                element = createElement(TagCompanyName);
                break;
            case 8: // TagCustomerName
                element = createElement(TagCustomerName);
                break;
            case 9: // TagPatternLabel
                element = createElement(TagPatternLabel);
                break;
            case 10: // TagDraftImages
                element = createElement(TagDraftImages);
                break;
            case 0: //TagUnit (Mandatory tag)
            default:
                return QDomElement();
        }
        InsertTag(tags, element);
        return element;
    }
    return list.at(0).toElement();
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::InsertTag(const QStringList &tags, const QDomElement &element)
{
    QDomElement pattern = documentElement();
    for (int i = tags.indexOf(element.tagName())-1; i >= 0; --i)
    {
        const QDomNodeList list = elementsByTagName(tags.at(i));
        if (!list.isEmpty())
        {
            pattern.insertAfter(element, list.at(0));
            break;
        }
    }
    setVersion();
}

//---------------------------------------------------------------------------------------------------------------------
int VAbstractPattern::getActiveDraftBlockIndex() const
{
    const QDomNodeList blockList = elementsByTagName(TagDraftBlock);

    int index = 0;
    if (!blockList.isEmpty())
    {
        for (int i = 0; i < blockList.size(); ++i)
        {
            QDomElement node = blockList.at(i).toElement();
            if (node.attribute(AttrName) == m_activeDraftBlock)
            {
                index = i;
                break;
            }
        }
    }

    return index;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList VAbstractPattern::listVariables() const
{
    QStringList variables;
    const QDomNodeList list = elementsByTagName(TagVariable);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        try
        {
            variables.append(GetParametrString(dom, VariableName));
        }
        catch (VExceptionEmptyParameter &error)
        {
            Q_UNUSED(error)
        }
    }

    return variables;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListExpressions() const
{
    QVector<VFormulaField> list;

    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    // Note. Tool Union Details also contains formulas, but we don't use them for union and keep only to simplifying
    // working with nodes. Same code for saving reading.
    list << ListPointExpressions();
    list << ListArcExpressions();
    list << ListElArcExpressions();
    list << ListSplineExpressions();
    list << listVariableExpressions();
    list << listFinalMeasurementExpressions();
    list << ListOperationExpressions();
    list << ListPathExpressions();
    list << ListPieceExpressions();

    return list;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief isVariableUsed check if any of the variables is referenced by a formula in the pattern.
/// @param variable_names names of the variables to look for.
/// @return true if at least one formula uses one of the variables.
//---------------------------------------------------------------------------------------------------------------------

bool VAbstractPattern::isVariableUsed(const QStringList &variable_names) const
{
    QStringList names = variable_names;
    names.removeAll(QString());

    if (names.isEmpty())
    {
        return false;
    }

    const QVector<VFormulaField> expressions = ListExpressions();
    for (int i = 0; i < expressions.size(); ++i)
    {
        // Cheap pre-check. Parsing every formula in the pattern is expensive.
        bool found = false;
        for (int j = 0; j < names.size(); ++j)
        {
            if (expressions.at(i).expression.indexOf(names.at(j)) != -1)
            {
                found = true;
                break;
            }
        }

        if (not found)
        {
            continue;
        }

        try
        {
            QScopedPointer<qmu::QmuTokenParser> cal(new qmu::QmuTokenParser(expressions.at(i).expression, false,
                                                                            false));

            // Tokens (variables, measurements)
            const QList<QString> tokens = cal->GetTokens().values();
            for (int j = 0; j < names.size(); ++j)
            {
                if (tokens.contains(names.at(j)))
                {
                    return true;
                }
            }
        }
        catch (const qmu::QmuParserError &)
        {
            // Do nothing. Because we not sure if used. A formula is broken.
        }
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListPointExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment a number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagPoint);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrLength);
        ReadExpressionAttribute(expressions, dom, AttrAngle);
        ReadExpressionAttribute(expressions, dom, AttrC1Radius);
        ReadExpressionAttribute(expressions, dom, AttrC2Radius);
        ReadExpressionAttribute(expressions, dom, AttrCRadius);
        ReadExpressionAttribute(expressions, dom, AttrRadius);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListArcExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagArc);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrAngle1);
        ReadExpressionAttribute(expressions, dom, AttrAngle2);
        ReadExpressionAttribute(expressions, dom, AttrRadius);
        ReadExpressionAttribute(expressions, dom, AttrLength);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListElArcExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagElArc);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrRadius1);
        ReadExpressionAttribute(expressions, dom, AttrRadius2);
        ReadExpressionAttribute(expressions, dom, AttrAngle1);
        ReadExpressionAttribute(expressions, dom, AttrAngle2);
        ReadExpressionAttribute(expressions, dom, AttrRotationAngle);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListSplineExpressions() const
{
    QVector<VFormulaField> expressions;
    expressions << ListPathPointExpressions();
    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListPathPointExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(AttrPathPoint);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrKAsm1);
        ReadExpressionAttribute(expressions, dom, AttrKAsm2);
        ReadExpressionAttribute(expressions, dom, AttrAngle);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::listVariableExpressions() const
{
    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagVariable);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        ReadExpressionAttribute(expressions, dom, VariableFormula);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::listFinalMeasurementExpressions() const
{
    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagFinalMeasurement);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        ReadExpressionAttribute(expressions, dom, VariableFormula);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFinalMeasurement> VAbstractPattern::getFinalMeasurements() const
{
    QVector<VFinalMeasurement> measurements;
    const QDomNodeList list = elementsByTagName(TagFinalMeasurement);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();
        if (dom.isNull())
        {
            continue;
        }

        VFinalMeasurement measurement;
        measurement.name = dom.attribute(VariableName).simplified();
        measurement.formula = dom.attribute(VariableFormula, QStringLiteral("0"));
        measurement.description = dom.attribute(VariableDescription);
        measurements.append(measurement);
    }

    return measurements;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setFinalMeasurements(const QVector<VFinalMeasurement> &measurements)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagFinalMeasurements);

    if (measurements.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createElement(TagFinalMeasurements);

            QDomElement sibling = pattern.firstChildElement(TagVariables);
            if (sibling.isNull())
            {
                sibling = pattern.firstChildElement(TagMeasurements);
            }

            if (sibling.isNull())
            {
                pattern.insertBefore(element, pattern.firstChildElement(TagDraftBlock));
            }
            else
            {
                pattern.insertAfter(element, sibling);
            }
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VFinalMeasurement &measurement : measurements)
        {
            QDomElement tag = createElement(TagFinalMeasurement);
            SetAttribute(tag, VariableName, measurement.name);
            SetAttribute(tag, VariableFormula, measurement.formula);
            if (!measurement.description.isEmpty())
            {
                SetAttribute(tag, VariableDescription, measurement.description);
            }
            element.appendChild(tag);
        }
    }

    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
bool VSeamSide::operator==(const VSeamSide& other) const
{
    return piece_id == other.piece_id && start_node == other.start_node && end_node == other.end_node
           && backward == other.backward;
}

//---------------------------------------------------------------------------------------------------------------------
bool VSeam::operator==(const VSeam& other) const
{
    return first == other.first && second == other.second && reverse == other.reverse
           && qFuzzyCompare(1.0 + angle, 1.0 + other.angle) && first_more == other.first_more
           && second_more == other.second_more;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stretches of the first side, in the order it goes on over them.
QVector<VSeamSide> VSeam::firstSide() const
{
    return QVector<VSeamSide>{first} + first_more;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The stretches of the second side, in the order it goes on over them.
QVector<VSeamSide> VSeam::secondSide() const
{
    return QVector<VSeamSide>{second} + second_more;
}

//---------------------------------------------------------------------------------------------------------------------
bool VFold::operator==(const VFold& other) const
{
    return piece_id == other.piece_id && path_id == other.path_id && qFuzzyCompare(1.0 + angle, 1.0 + other.angle);
}

//---------------------------------------------------------------------------------------------------------------------
bool VElastic::operator==(const VElastic& other) const
{
    return piece_id == other.piece_id && start_node == other.start_node && end_node == other.end_node
           && path_id == other.path_id && qFuzzyCompare(1.0 + ratio, 1.0 + other.ratio);
}

//---------------------------------------------------------------------------------------------------------------------
bool VPieceArrangement::operator==(const VPieceArrangement& other) const
{
    return piece_id == other.piece_id && part == other.part && qFuzzyCompare(1.0 + angle, 1.0 + other.angle)
           && qFuzzyCompare(1.0 + height, 1.0 + other.height) && qFuzzyCompare(1.0 + rotation, 1.0 + other.rotation)
           && turned_over == other.turned_over && point == other.point
           && qFuzzyCompare(1.0 + distance, 1.0 + other.distance) && qFuzzyCompare(1.0 + lean, 1.0 + other.lean)
           && qFuzzyCompare(1.0 + swing, 1.0 + other.swing);
}

//---------------------------------------------------------------------------------------------------------------------
bool VPieceLayer::operator==(const VPieceLayer& other) const
{
    return piece_id == other.piece_id && layer == other.layer;
}

//---------------------------------------------------------------------------------------------------------------------
bool VFabricTexture::isNull() const
{
    return image.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
bool VFabricTexture::operator==(const VFabricTexture& other) const
{
    return image == other.image && extension == other.extension && qFuzzyCompare(1.0 + width, 1.0 + other.width);
}

//---------------------------------------------------------------------------------------------------------------------
bool VFabricShrinkage::isNull() const
{
    return weft <= 0 || warp <= 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool VFabricShrinkage::operator==(const VFabricShrinkage& other) const
{
    return (isNull() && other.isNull())
           || (qFuzzyCompare(1.0 + weft, 1.0 + other.weft) && qFuzzyCompare(1.0 + warp, 1.0 + other.warp));
}

//---------------------------------------------------------------------------------------------------------------------
bool VCustomFabric::operator==(const VCustomFabric& other) const
{
    auto same = [](qreal value, qreal other_value)
    {
        return qFuzzyCompare(1.0 + value, 1.0 + other_value);
    };
    return name == other.name && same(weight, other.weight) && same(warp, other.warp) && same(weft, other.weft)
           && same(bias, other.bias) && same(bending_warp, other.bending_warp)
           && same(bending_weft, other.bending_weft) && same(thickness, other.thickness);
}

//---------------------------------------------------------------------------------------------------------------------
bool VPieceFabric::operator==(const VPieceFabric& other) const
{
    return piece_id == other.piece_id && fabric == other.fabric && texture == other.texture
           && shrinkage == other.shrinkage;
}

//---------------------------------------------------------------------------------------------------------------------
bool VGarmentFabrics::operator==(const VGarmentFabrics& other) const
{
    return garment == other.garment && texture == other.texture && pieces == other.pieces
           && shrinkage == other.shrinkage && custom == other.custom;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fabric the piece is cut from: its own, or the garment's; empty for the 3D View's default.
QString VGarmentFabrics::of(quint32 piece_id) const
{
    for (const VPieceFabric& fabric : pieces)
    {
        if (fabric.piece_id == piece_id && !fabric.fabric.isEmpty())
        {
            return fabric.fabric;
        }
    }
    return garment;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The image of the fabric the piece is cut from: its own, or, cut from the garment's fabric, the garment's;
/// null for none.
VFabricTexture VGarmentFabrics::textureOf(quint32 piece_id) const
{
    for (const VPieceFabric& fabric : pieces)
    {
        if (fabric.piece_id == piece_id)
        {
            if (!fabric.texture.isNull() || !fabric.fabric.isEmpty())
            {
                return fabric.texture;
            }
            break;
        }
    }
    return texture;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief How much the fabric the piece is cut from shrinks: its own, or, cut from the garment's fabric, the
/// garment's; null for none.
VFabricShrinkage VGarmentFabrics::shrinkageOf(quint32 piece_id) const
{
    for (const VPieceFabric& fabric : pieces)
    {
        if (fabric.piece_id == piece_id)
        {
            if (!fabric.shrinkage.isNull() || !fabric.fabric.isEmpty())
            {
                return fabric.shrinkage;
            }
            break;
        }
    }
    return shrinkage;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The pattern's own fabric of that name; one with no name for none.
VCustomFabric VGarmentFabrics::customFabric(const QString& name) const
{
    for (const VCustomFabric& fabric : custom)
    {
        if (!name.isEmpty() && fabric.name == name)
        {
            return fabric;
        }
    }
    return VCustomFabric();
}

//---------------------------------------------------------------------------------------------------------------------
bool VTopstitch::operator==(const VTopstitch& other) const
{
    return piece_id == other.piece_id && start_node == other.start_node && end_node == other.end_node
           && stitched == other.stitched && style == other.style;
}

//---------------------------------------------------------------------------------------------------------------------
bool VTopstitches::operator==(const VTopstitches& other) const
{
    return all == other.all && style == other.style && color == other.color && segments == other.segments;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the segment of the piece from one path point to the next is topstitched: as it says if it has an
/// entry of its own, else as the whole garment is.
bool VTopstitches::isStitched(quint32 piece_id, quint32 start_node, quint32 end_node) const
{
    bool stitched = all;
    for (const VTopstitch& segment : segments)
    {
        if (segment.piece_id == piece_id && segment.start_node == start_node && segment.end_node == end_node)
        {
            stitched = segment.stitched;
        }
    }
    return stitched;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The style the segment is topstitched in: its own, if its entry has one, else the garment's; empty for the
/// 3D View's default.
QString VTopstitches::styleOf(quint32 piece_id, quint32 start_node, quint32 end_node) const
{
    QString found = style;
    for (const VTopstitch& segment : segments)
    {
        if (segment.piece_id == piece_id && segment.start_node == start_node && segment.end_node == end_node
            && !segment.style.isEmpty())
        {
            found = segment.style;
        }
    }
    return found;
}

//---------------------------------------------------------------------------------------------------------------------
bool VGarmentAvatar::isNull() const
{
    return size == 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool VGarmentAvatar::operator==(const VGarmentAvatar& other) const
{
    return male == other.male && size == other.size && qFuzzyCompare(1.0 + height, 1.0 + other.height)
           && qFuzzyCompare(1.0 + bust, 1.0 + other.bust) && qFuzzyCompare(1.0 + waist, 1.0 + other.waist)
           && qFuzzyCompare(1.0 + hip, 1.0 + other.hip);
}

//---------------------------------------------------------------------------------------------------------------------
bool VDrapedCloth::operator==(const VDrapedCloth& other) const
{
    return piece_id == other.piece_id && copy == other.copy && rest == other.rest && positions == other.positions;
}

//---------------------------------------------------------------------------------------------------------------------
bool VClothPin::operator==(const VClothPin& other) const
{
    return piece_id == other.piece_id && copy == other.copy && qFuzzyCompare(1.0 + x, 1.0 + other.x)
           && qFuzzyCompare(1.0 + y, 1.0 + other.y) && qFuzzyCompare(1.0 + at_x, 1.0 + other.at_x)
           && qFuzzyCompare(1.0 + at_y, 1.0 + other.at_y) && qFuzzyCompare(1.0 + at_z, 1.0 + other.at_z);
}

//---------------------------------------------------------------------------------------------------------------------
bool VGarmentDrape::isNull() const
{
    return cloths.isEmpty() && pins.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
bool VGarmentDrape::operator==(const VGarmentDrape& other) const
{
    return avatar == other.avatar && qFuzzyCompare(1.0 + edge_length, 1.0 + other.edge_length)
           && cloths == other.cloths && pins == other.pins;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The seams sewing the pieces together, in the order they were made.
///
/// A seam can name a piece or point that is gone, after the piece was deleted or its path edited. Such seams are
/// kept, so undoing the deletion brings them back, and whoever uses them has to skip them.
QVector<VSeam> VAbstractPattern::getSeams() const
{
    QVector<VSeam> seams;
    QDomElement element = documentElement().firstChildElement(TagSeams).firstChildElement(TagSeam);
    while (!element.isNull())
    {
        VSeam seam;
        seam.first.piece_id = GetParametrUInt(element, AttrFirstPiece, NULL_ID_STR);
        seam.first.start_node = GetParametrUInt(element, AttrFirstStart, NULL_ID_STR);
        seam.first.end_node = GetParametrUInt(element, AttrFirstEnd, NULL_ID_STR);
        seam.second.piece_id = GetParametrUInt(element, AttrSecondPiece, NULL_ID_STR);
        seam.second.start_node = GetParametrUInt(element, AttrSecondStart, NULL_ID_STR);
        seam.second.end_node = GetParametrUInt(element, AttrSecondEnd, NULL_ID_STR);
        seam.first.backward = getParameterBool(element, AttrFirstBackward, falseStr);
        seam.second.backward = getParameterBool(element, AttrSecondBackward, falseStr);
        seam.reverse = getParameterBool(element, AttrNodeReverse, falseStr);
        seam.angle = GetParametrDouble(element, AttrAngle, QStringLiteral("-1"));
        for (QDomElement more = element.firstChildElement(); !more.isNull(); more = more.nextSiblingElement())
        {
            VSeamSide stretch;
            stretch.piece_id = GetParametrUInt(more, AttrPiece, NULL_ID_STR);
            stretch.start_node = GetParametrUInt(more, AttrStart, NULL_ID_STR);
            stretch.end_node = GetParametrUInt(more, AttrEnd, NULL_ID_STR);
            stretch.backward = getParameterBool(more, AttrBackward, falseStr);
            if (more.tagName() == TagFirstStretch)
            {
                seam.first_more.append(stretch);
            }
            else if (more.tagName() == TagSecondStretch)
            {
                seam.second_more.append(stretch);
            }
        }
        seams.append(seam);

        element = element.nextSiblingElement(TagSeam);
    }
    return seams;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces all seams. Meant to be called by the SaveSeams undo command, which also keeps track of whether the
/// pattern was changed.
void VAbstractPattern::setSeams(const QVector<VSeam>& seams)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagSeams);

    if (seams.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagSeams);
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VSeam& seam : seams)
        {
            QDomElement tag = createElement(TagSeam);
            SetAttribute(tag, AttrFirstPiece, seam.first.piece_id);
            SetAttribute(tag, AttrFirstStart, seam.first.start_node);
            SetAttribute(tag, AttrFirstEnd, seam.first.end_node);
            SetAttribute(tag, AttrSecondPiece, seam.second.piece_id);
            SetAttribute(tag, AttrSecondStart, seam.second.start_node);
            SetAttribute(tag, AttrSecondEnd, seam.second.end_node);
            if (seam.reverse)
            {
                SetAttribute(tag, AttrNodeReverse, seam.reverse);
            }
            if (seam.angle >= 0)
            {
                SetAttribute(tag, AttrAngle, seam.angle);
            }
            if (seam.first.backward)
            {
                SetAttribute(tag, AttrFirstBackward, seam.first.backward);
            }
            if (seam.second.backward)
            {
                SetAttribute(tag, AttrSecondBackward, seam.second.backward);
            }
            for (const auto& more : {std::make_pair(TagFirstStretch, &seam.first_more),
                                     std::make_pair(TagSecondStretch, &seam.second_more)})
            {
                for (const VSeamSide& stretch : *more.second)
                {
                    QDomElement stretch_tag = createElement(more.first);
                    SetAttribute(stretch_tag, AttrPiece, stretch.piece_id);
                    SetAttribute(stretch_tag, AttrStart, stretch.start_node);
                    SetAttribute(stretch_tag, AttrEnd, stretch.end_node);
                    if (stretch.backward)
                    {
                        SetAttribute(stretch_tag, AttrBackward, stretch.backward);
                    }
                    tag.appendChild(stretch_tag);
                }
            }
            element.appendChild(tag);
        }
    }

    emit seamsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The internal paths the pieces are folded along. A fold can name a piece or path that is gone; it is kept,
/// so undoing the deletion brings it back, and whoever uses it has to skip it.
QVector<VFold> VAbstractPattern::getFolds() const
{
    QVector<VFold> folds;
    QDomElement element = documentElement().firstChildElement(TagFolds).firstChildElement(TagFold);
    while (!element.isNull())
    {
        VFold fold;
        fold.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        fold.path_id = GetParametrUInt(element, AttrPath, NULL_ID_STR);
        fold.angle = GetParametrDouble(element, AttrAngle, QStringLiteral("180"));
        folds.append(fold);

        element = element.nextSiblingElement(TagFold);
    }
    return folds;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the folds, one per internal path at most. Meant to be called by the SaveFolds undo command.
void VAbstractPattern::setFolds(const QVector<VFold>& folds)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagFolds);

    if (folds.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagFolds);
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VFold& fold : folds)
        {
            QDomElement tag = createElement(TagFold);
            SetAttribute(tag, AttrPiece, fold.piece_id);
            SetAttribute(tag, AttrPath, fold.path_id);
            SetAttribute(tag, AttrAngle, fold.angle);
            element.appendChild(tag);
        }
    }

    emit foldsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The elastics sewn into the pieces. An elastic can name a piece, a point or a path that is gone; it is kept, so
/// undoing the deletion brings it back, and whoever uses it has to skip it.
QVector<VElastic> VAbstractPattern::getElastics() const
{
    QVector<VElastic> elastics;
    QDomElement element = documentElement().firstChildElement(TagElastics).firstChildElement(TagElastic);
    while (!element.isNull())
    {
        VElastic elastic;
        elastic.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        elastic.start_node = GetParametrUInt(element, AttrStart, NULL_ID_STR);
        elastic.end_node = GetParametrUInt(element, AttrEnd, NULL_ID_STR);
        elastic.path_id = GetParametrUInt(element, AttrPath, NULL_ID_STR);
        elastic.ratio = GetParametrDouble(element, AttrRatio, QStringLiteral("0.8"));
        elastics.append(elastic);

        element = element.nextSiblingElement(TagElastic);
    }
    return elastics;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the elastics, one per segment or internal path at most. Meant to be called by the SaveElastics undo
/// command.
void VAbstractPattern::setElastics(const QVector<VElastic>& elastics)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagElastics);

    if (elastics.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagElastics);
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VElastic& elastic : elastics)
        {
            QDomElement tag = createElement(TagElastic);
            SetAttribute(tag, AttrPiece, elastic.piece_id);
            if (elastic.path_id != NULL_ID)
            {
                SetAttribute(tag, AttrPath, elastic.path_id);
            }
            else
            {
                SetAttribute(tag, AttrStart, elastic.start_node);
                SetAttribute(tag, AttrEnd, elastic.end_node);
            }
            SetAttribute(tag, AttrRatio, elastic.ratio);
            element.appendChild(tag);
        }
    }

    emit elasticsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Where the pieces start out on the avatar; pieces without one stay on the board.
QVector<VPieceArrangement> VAbstractPattern::getArrangements() const
{
    QVector<VPieceArrangement> arrangements;
    QDomElement element = documentElement().firstChildElement(TagArrangements).firstChildElement(TagArrangement);
    while (!element.isNull())
    {
        VPieceArrangement arrangement;
        arrangement.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        arrangement.part = GetParametrString(element, AttrPart);
        arrangement.angle = GetParametrDouble(element, AttrAngle, QStringLiteral("0"));
        arrangement.height = GetParametrDouble(element, AttrHeight, QStringLiteral("0"));
        arrangement.rotation = GetParametrDouble(element, AttrRotation, QStringLiteral("0"));
        arrangement.turned_over = getParameterBool(element, AttrTurnedOver, falseStr);
        arrangement.point = GetParametrEmptyString(element, AttrArrangementPoint);
        arrangement.distance = GetParametrDouble(element, AttrDistance, QStringLiteral("0"));
        arrangement.lean = GetParametrDouble(element, AttrLean, QStringLiteral("0"));
        arrangement.swing = GetParametrDouble(element, AttrSwing, QStringLiteral("0"));
        arrangements.append(arrangement);

        element = element.nextSiblingElement(TagArrangement);
    }
    return arrangements;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces all arrangements, one per piece at most. Meant to be called by the SaveArrangements undo command.
void VAbstractPattern::setArrangements(const QVector<VPieceArrangement>& arrangements)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagArrangements);

    if (arrangements.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagArrangements);
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VPieceArrangement& arrangement : arrangements)
        {
            QDomElement tag = createElement(TagArrangement);
            SetAttribute(tag, AttrPiece, arrangement.piece_id);
            SetAttribute(tag, AttrPart, arrangement.part);
            SetAttribute(tag, AttrAngle, arrangement.angle);
            SetAttribute(tag, AttrHeight, arrangement.height);
            if (!qFuzzyIsNull(arrangement.rotation))
            {
                SetAttribute(tag, AttrRotation, arrangement.rotation);
            }
            if (arrangement.turned_over)
            {
                SetAttribute(tag, AttrTurnedOver, arrangement.turned_over);
            }
            if (!arrangement.point.isEmpty())
            {
                SetAttribute(tag, AttrArrangementPoint, arrangement.point);
            }
            if (!qFuzzyIsNull(arrangement.distance))
            {
                SetAttribute(tag, AttrDistance, arrangement.distance);
            }
            if (!qFuzzyIsNull(arrangement.lean))
            {
                SetAttribute(tag, AttrLean, arrangement.lean);
            }
            if (!qFuzzyIsNull(arrangement.swing))
            {
                SetAttribute(tag, AttrSwing, arrangement.swing);
            }
            element.appendChild(tag);
        }
    }

    emit arrangementsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The layers the pieces are worn in, but for those in layer 0. A layer can name a piece that is gone; it is
/// kept, so undoing the deletion brings it back, and whoever uses it has to skip it.
QVector<VPieceLayer> VAbstractPattern::getLayers() const
{
    QVector<VPieceLayer> layers;
    QDomElement element = documentElement().firstChildElement(TagLayers).firstChildElement(TagLayer);
    while (!element.isNull())
    {
        VPieceLayer layer;
        layer.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        layer.layer = static_cast<int>(GetParametrUInt(element, AttrNumber, QStringLiteral("0")));
        layers.append(layer);

        element = element.nextSiblingElement(TagLayer);
    }
    return layers;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the layers the pieces are worn in, one per piece at most; pieces in layer 0 are left out. Meant to
/// be called by the SaveLayers undo command.
void VAbstractPattern::setLayers(const QVector<VPieceLayer>& layers)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagLayers);

    QVector<VPieceLayer> worn;
    for (const VPieceLayer& layer : layers)
    {
        if (layer.layer > 0)
        {
            worn.append(layer);
        }
    }

    if (worn.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagLayers);
        }
        else
        {
            RemoveAllChildren(element);
        }

        for (const VPieceLayer& layer : worn)
        {
            QDomElement tag = createElement(TagLayer);
            SetAttribute(tag, AttrPiece, layer.piece_id);
            SetAttribute(tag, AttrNumber, layer.layer);
            element.appendChild(tag);
        }
    }

    emit layersChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fabrics the garment is cut from.
VGarmentFabrics VAbstractPattern::getFabrics() const
{
    auto read_shrinkage = [](const QDomElement& element)
    {
        VFabricShrinkage shrinkage;
        if (!element.isNull())
        {
            shrinkage.weft = GetParametrDouble(element, AttrShrinkageWeft, QStringLiteral("0"));
            shrinkage.warp = GetParametrDouble(element, AttrShrinkageWarp, QStringLiteral("0"));
        }
        return shrinkage;
    };

    VGarmentFabrics fabrics;
    const QDomElement fabrics_element = documentElement().firstChildElement(TagFabrics);
    fabrics.garment = fabrics_element.isNull() ? QString() : GetParametrEmptyString(fabrics_element, AttrDefault);
    fabrics.texture = readFabricTexture(fabrics_element);
    fabrics.shrinkage = read_shrinkage(fabrics_element);
    QDomElement custom = fabrics_element.firstChildElement(TagCustomFabric);
    while (!custom.isNull())
    {
        VCustomFabric fabric;
        fabric.name = GetParametrEmptyString(custom, AttrName);
        fabric.weight = GetParametrDouble(custom, AttrWeight, QStringLiteral("0"));
        fabric.warp = GetParametrDouble(custom, AttrWarp, QStringLiteral("0"));
        fabric.weft = GetParametrDouble(custom, AttrWeft, QStringLiteral("0"));
        fabric.bias = GetParametrDouble(custom, AttrBias, QStringLiteral("0"));
        fabric.bending_warp = GetParametrDouble(custom, AttrBendingWarp, QStringLiteral("0"));
        fabric.bending_weft = GetParametrDouble(custom, AttrBendingWeft, QStringLiteral("0"));
        fabric.thickness = GetParametrDouble(custom, AttrThickness, QStringLiteral("0"));
        fabrics.custom.append(fabric);

        custom = custom.nextSiblingElement(TagCustomFabric);
    }
    QDomElement element = fabrics_element.firstChildElement(TagFabric);
    while (!element.isNull())
    {
        VPieceFabric fabric;
        fabric.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        fabric.fabric = GetParametrEmptyString(element, AttrName);
        fabric.texture = readFabricTexture(element);
        fabric.shrinkage = read_shrinkage(element);
        fabrics.pieces.append(fabric);

        element = element.nextSiblingElement(TagFabric);
    }
    return fabrics;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the fabrics, one per piece at most, and the pattern's own. Meant to be called by the SaveFabrics
/// undo command.
void VAbstractPattern::setFabrics(const VGarmentFabrics& fabrics)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagFabrics);

    auto set_shrinkage = [this](QDomElement& tag, const VFabricShrinkage& shrinkage)
    {
        if (shrinkage.isNull())
        {
            tag.removeAttribute(AttrShrinkageWeft);
            tag.removeAttribute(AttrShrinkageWarp);
        }
        else
        {
            SetAttribute(tag, AttrShrinkageWeft, shrinkage.weft);
            SetAttribute(tag, AttrShrinkageWarp, shrinkage.warp);
        }
    };
    auto append_texture = [this](QDomElement& parent, const VFabricTexture& texture)
    {
        if (!texture.isNull())
        {
            QDomElement tag = createElement(TagTexture);
            SetAttribute(tag, AttrExtension, texture.extension);
            SetAttribute(tag, AttrWidth, texture.width);
            tag.appendChild(createTextNode(QString::fromLatin1(texture.image.toBase64())));
            parent.appendChild(tag);
        }
    };

    if (fabrics.garment.isEmpty() && fabrics.texture.isNull() && fabrics.pieces.isEmpty() && fabrics.shrinkage.isNull()
        && fabrics.custom.isEmpty())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagFabrics);
        }
        else
        {
            RemoveAllChildren(element);
        }

        if (fabrics.garment.isEmpty())
        {
            element.removeAttribute(AttrDefault);
        }
        else
        {
            SetAttribute(element, AttrDefault, fabrics.garment);
        }
        set_shrinkage(element, fabrics.shrinkage);
        for (const VCustomFabric& fabric : fabrics.custom)
        {
            QDomElement tag = createElement(TagCustomFabric);
            SetAttribute(tag, AttrName, fabric.name);
            SetAttribute(tag, AttrWeight, fabric.weight);
            SetAttribute(tag, AttrWarp, fabric.warp);
            SetAttribute(tag, AttrWeft, fabric.weft);
            SetAttribute(tag, AttrBias, fabric.bias);
            SetAttribute(tag, AttrBendingWarp, fabric.bending_warp);
            SetAttribute(tag, AttrBendingWeft, fabric.bending_weft);
            SetAttribute(tag, AttrThickness, fabric.thickness);
            element.appendChild(tag);
        }
        append_texture(element, fabrics.texture);
        for (const VPieceFabric& fabric : fabrics.pieces)
        {
            QDomElement tag = createElement(TagFabric);
            SetAttribute(tag, AttrPiece, fabric.piece_id);
            if (!fabric.fabric.isEmpty())
            {
                SetAttribute(tag, AttrName, fabric.fabric);
            }
            set_shrinkage(tag, fabric.shrinkage);
            append_texture(tag, fabric.texture);
            element.appendChild(tag);
        }
    }

    emit fabricsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The garment's topstitching.
VTopstitches VAbstractPattern::getTopstitches() const
{
    VTopstitches topstitches;
    const QDomElement topstitches_element = documentElement().firstChildElement(TagTopstitches);
    topstitches.all = !topstitches_element.isNull()
                      && getParameterBool(topstitches_element, AttrAll, falseStr);
    if (!topstitches_element.isNull())
    {
        topstitches.style = GetParametrEmptyString(topstitches_element, AttrStyle);
        topstitches.color = GetParametrEmptyString(topstitches_element, AttrColor);
    }
    QDomElement element = topstitches_element.firstChildElement(TagTopstitch);
    while (!element.isNull())
    {
        VTopstitch segment;
        segment.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        segment.start_node = GetParametrUInt(element, AttrStart, NULL_ID_STR);
        segment.end_node = GetParametrUInt(element, AttrEnd, NULL_ID_STR);
        segment.stitched = getParameterBool(element, AttrStitched, trueStr);
        segment.style = GetParametrEmptyString(element, AttrStyle);
        topstitches.segments.append(segment);

        element = element.nextSiblingElement(TagTopstitch);
    }
    return topstitches;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the topstitching, one entry per segment at most. Meant to be called by the SaveTopstitches undo
/// command.
void VAbstractPattern::setTopstitches(const VTopstitches& topstitches)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagTopstitches);

    if (topstitches == VTopstitches())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagTopstitches);
        }
        else
        {
            RemoveAllChildren(element);
        }

        // Style and color are left out where they are the 3D View's defaults.
        auto set_or_leave_out = [this](QDomElement& tag, const QString& name, const QString& value)
        {
            if (value.isEmpty())
            {
                tag.removeAttribute(name);
            }
            else
            {
                SetAttribute(tag, name, value);
            }
        };
        SetAttribute(element, AttrAll, topstitches.all);
        set_or_leave_out(element, AttrStyle, topstitches.style);
        set_or_leave_out(element, AttrColor, topstitches.color);
        for (const VTopstitch& segment : topstitches.segments)
        {
            QDomElement tag = createElement(TagTopstitch);
            SetAttribute(tag, AttrPiece, segment.piece_id);
            SetAttribute(tag, AttrStart, segment.start_node);
            SetAttribute(tag, AttrEnd, segment.end_node);
            SetAttribute(tag, AttrStitched, segment.stitched);
            set_or_leave_out(tag, AttrStyle, segment.style);
            element.appendChild(tag);
        }
    }

    emit topstitchesChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar the 3D View shows the garment on when the pattern has no measurements; null if none was chosen.
VGarmentAvatar VAbstractPattern::getAvatar() const
{
    VGarmentAvatar avatar;
    const QDomElement element = documentElement().firstChildElement(TagAvatar);
    if (!element.isNull())
    {
        avatar.male = GetParametrString(element, AttrGender, female_gender) == male_gender;
        avatar.size = static_cast<int>(GetParametrUInt(element, AttrSize, QStringLiteral("0")));
        avatar.height = GetParametrDouble(element, AttrHeight, QStringLiteral("0"));
        avatar.bust = GetParametrDouble(element, AttrBust, QStringLiteral("0"));
        avatar.waist = GetParametrDouble(element, AttrWaist, QStringLiteral("0"));
        avatar.hip = GetParametrDouble(element, AttrHip, QStringLiteral("0"));
    }
    return avatar;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the avatar; a null one takes it out. Meant to be called by the SaveAvatar undo command.
void VAbstractPattern::setAvatar(const VGarmentAvatar& avatar)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagAvatar);

    if (avatar.isNull())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagAvatar);
        }
        SetAttribute(element, AttrGender, avatar.male ? male_gender : female_gender);
        SetAttribute(element, AttrSize, avatar.size);
        SetAttribute(element, AttrHeight, avatar.height);
        SetAttribute(element, AttrBust, avatar.bust);
        SetAttribute(element, AttrWaist, avatar.waist);
        SetAttribute(element, AttrHip, avatar.hip);
    }

    emit avatarChanged();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The garment as the 3D View last draped it; null if it wasn't draped, or was put back as arranged.
VGarmentDrape VAbstractPattern::getDrape() const
{
    VGarmentDrape drape;
    const QDomElement drape_element = documentElement().firstChildElement(TagDrape);
    if (!drape_element.isNull())
    {
        drape.avatar = GetParametrEmptyString(drape_element, AttrAvatar);
        drape.edge_length = GetParametrDouble(drape_element, AttrEdgeLength, QStringLiteral("0"));
    }

    QDomElement element = drape_element.firstChildElement(TagCloth);
    while (!element.isNull())
    {
        VDrapedCloth cloth;
        cloth.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        cloth.copy = getParameterBool(element, AttrCopy, falseStr);
        readClothBytes(QByteArray::fromBase64(element.text().toLatin1()), cloth);
        drape.cloths.append(cloth);

        element = element.nextSiblingElement(TagCloth);
    }

    element = drape_element.firstChildElement(TagClothPin);
    while (!element.isNull())
    {
        VClothPin pin;
        pin.piece_id = GetParametrUInt(element, AttrPiece, NULL_ID_STR);
        pin.copy = getParameterBool(element, AttrCopy, falseStr);
        pin.x = GetParametrDouble(element, AttrX, QStringLiteral("0"));
        pin.y = GetParametrDouble(element, AttrY, QStringLiteral("0"));
        pin.at_x = GetParametrDouble(element, AttrAtX, QStringLiteral("0"));
        pin.at_y = GetParametrDouble(element, AttrAtY, QStringLiteral("0"));
        pin.at_z = GetParametrDouble(element, AttrAtZ, QStringLiteral("0"));
        drape.pins.append(pin);

        element = element.nextSiblingElement(TagClothPin);
    }
    return drape;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Replaces the drape; a null one takes it out.
///
/// How the cloth hangs isn't an edit to undo, so this goes past the undo stack; but the pattern counts as changed, to
/// be saved with it.
void VAbstractPattern::setDrape(const VGarmentDrape& drape)
{
    QDomElement pattern = documentElement();
    QDomElement element = pattern.firstChildElement(TagDrape);

    if (drape.isNull())
    {
        if (!element.isNull())
        {
            pattern.removeChild(element);
        }
    }
    else
    {
        if (element.isNull())
        {
            element = createGarmentElement(TagDrape);
        }
        else
        {
            RemoveAllChildren(element);
        }

        SetAttribute(element, AttrAvatar, drape.avatar);
        SetAttribute(element, AttrEdgeLength, drape.edge_length);
        for (const VDrapedCloth& cloth : drape.cloths)
        {
            QDomElement tag = createElement(TagCloth);
            SetAttribute(tag, AttrPiece, cloth.piece_id);
            if (cloth.copy)
            {
                SetAttribute(tag, AttrCopy, cloth.copy);
            }
            tag.appendChild(createTextNode(QString::fromLatin1(clothBytes(cloth).toBase64())));
            element.appendChild(tag);
        }
        for (const VClothPin& pin : drape.pins)
        {
            QDomElement tag = createElement(TagClothPin);
            SetAttribute(tag, AttrPiece, pin.piece_id);
            if (pin.copy)
            {
                SetAttribute(tag, AttrCopy, pin.copy);
            }
            SetAttribute(tag, AttrX, pin.x);
            SetAttribute(tag, AttrY, pin.y);
            SetAttribute(tag, AttrAtX, pin.at_x);
            SetAttribute(tag, AttrAtY, pin.at_y);
            SetAttribute(tag, AttrAtZ, pin.at_z);
            element.appendChild(tag);
        }
    }

    modified = true;
    emit patternChanged(false);
}

//---------------------------------------------------------------------------------------------------------------------
// Adds an empty element for the 3D garment's data where the schema wants it: the seams, the folds, the elastics, the
// arrangements, the layers, the fabrics, the topstitching, the avatar, then the drape, all before the draft blocks,
// which are added at the end.
QDomElement VAbstractPattern::createGarmentElement(const QString& tag)
{
    const QStringList order = {TagSeams, TagFolds, TagElastics, TagArrangements, TagLayers, TagFabrics, TagTopstitches,
                               TagAvatar, TagDrape, TagDraftBlock};
    QDomElement pattern = documentElement();

    QDomElement before;
    for (int i = static_cast<int>(order.indexOf(tag)) + 1; i < order.size() && before.isNull(); ++i)
    {
        before = pattern.firstChildElement(order.at(i));
    }

    QDomElement element = createElement(tag);
    pattern.insertBefore(element, before);
    return element;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListOperationExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagOperation);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrAngle);
        ReadExpressionAttribute(expressions, dom, AttrLength);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListNodesExpressions(const QDomElement &nodes) const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;

    const QDomNodeList nodeList = nodes.childNodes();
    for (qint32 i = 0; i < nodeList.size(); ++i)
    {
        const QDomElement element = nodeList.at(i).toElement();
        if (!element.isNull() && element.tagName() == VAbstractPattern::TagNode)
        {
            ReadExpressionAttribute(expressions, element, VAbstractPattern::AttrSABefore);
            ReadExpressionAttribute(expressions, element, VAbstractPattern::AttrSAAfter);
        }
    }
    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListPathExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagPath);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();
        if (dom.isNull())
        {
            continue;
        }

        expressions << ListNodesExpressions(dom.firstChildElement(TagNodes));
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListGrainlineExpressions(const QDomElement &element) const
{
    QVector<VFormulaField> expressions;
    if (!element.isNull())
    {
        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, element, AttrRotation);
        ReadExpressionAttribute(expressions, element, AttrLength);
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<VFormulaField> VAbstractPattern::ListPieceExpressions() const
{
    // Check if new tool doesn't bring new attribute with a formula.
    // If no just increment number.
    // If new tool bring absolutely new type and has formula(s) create new method to cover it.
    Q_STATIC_ASSERT(static_cast<int>(Tool::LAST_ONE_DO_NOT_USE) == 54);

    QVector<VFormulaField> expressions;
    const QDomNodeList list = elementsByTagName(TagPiece);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();
        if (dom.isNull())
        {
            continue;
        }

        // Each tag can contains several attributes.
        ReadExpressionAttribute(expressions, dom, AttrWidth);

        expressions << ListNodesExpressions(dom.firstChildElement(TagNodes));
        expressions << ListGrainlineExpressions(dom.firstChildElement(TagGrainline));
    }

    return expressions;
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::IsVariable(const QString &token) const
{
    for (int i = 0; i < builInVariables.size(); ++i)
    {
        if (token.indexOf( builInVariables.at(i) ) == 0)
        {
            if (builInVariables.at(i) == currentLength || builInVariables.at(i) == currentSeamAllowance)
            {
                return token == builInVariables.at(i);
            }
            else
            {
                return true;
            }
        }
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::IsPostfixOperator(const QString &token) const
{
    for (int i = 0; i < builInPostfixOperators.size(); ++i)
    {
        if (token.indexOf( builInPostfixOperators.at(i) ) == 0)
        {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::IsFunction(const QString &token) const
{
    for (int i = 0; i < builInFunctions.size(); ++i)
    {
        if (token.indexOf( builInFunctions.at(i) ) == 0)
        {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
QPair<bool, QMap<quint32, quint32> > VAbstractPattern::parseItemElement(const QDomElement &domElement)
{
    Q_ASSERT_X(!domElement.isNull(), Q_FUNC_INFO, "domElement is null");

    try
    {
        const bool visible = getParameterBool(domElement, AttrVisible, trueStr);

        QMap<quint32, quint32> items;

        const QDomNodeList nodeList = domElement.childNodes();
        const qint32 num = nodeList.size();
        for (qint32 i = 0; i < num; ++i)
        {
            const QDomElement element = nodeList.at(i).toElement();
            if (!element.isNull() && element.tagName() == TagGroupItem)
            {
                const quint32 object = GetParametrUInt(element, AttrObject, NULL_ID_STR);
                const quint32 tool = GetParametrUInt(element, AttrTool, NULL_ID_STR);
                items.insert(object, tool);
            }
        }

        QPair<bool, QMap<quint32, quint32> > group;
        group.first = visible;
        group.second = items;

        return group;
    }
    catch (const VExceptionBadId &error)
    {
        VExceptionObjectError excep(tr("Error creating or updating group"), domElement);
        excep.AddMoreInformation(error.ErrorMessage());
        throw excep;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief IsModified state of the document for cases that do not cover QUndoStack.
/// @return true if the document was modified without using QUndoStack.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::IsModified() const
{
    return modified;
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::SetModified(bool modified)
{
    this->modified = modified;
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::createGroups()
{
    QDomElement draftBlock;
    if (getActiveDraftElement(draftBlock))
    {
        QDomElement groups = draftBlock.firstChildElement(TagGroups);

        if (groups.isNull())
        {
            groups = createElement(TagGroups);
            draftBlock.appendChild(groups);
        }

        return groups;
    }
    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::createDraftImages()
{
    QDomElement draftBlock;
    if (getActiveDraftElement(draftBlock))
    {
        QDomElement backgroundImages = draftBlock.firstChildElement(TagDraftImages);

        if (backgroundImages.isNull())
        {
            backgroundImages = createElement(TagDraftImages);
            draftBlock.appendChild(backgroundImages);
        }

        return backgroundImages;
    }
    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::createGroup(quint32 groupId, const QString &name, const QString &color, const QString &type,
                                          const QString &weight, const QMap<quint32, quint32> &groupData)
{
    if (groupId == NULL_ID)
    {
        return QDomElement();
    }

    //Create new empty group
    QDomElement group = createElement(TagGroup);
    SetAttribute(group, AttrId, groupId);
    SetAttribute(group, AttrName, name);
    SetAttribute(group, AttrVisible, true);
    SetAttribute(group, AttrGroupLocked, false);
    SetAttribute(group, AttrGroupColor, color);
    SetAttribute(group, AttrLineType, type);
    SetAttribute(group, AttrLineWeight, weight);

    //Add objects to group
    if (!groupData.isEmpty())
    {
        auto i = groupData.constBegin();
        while (i != groupData.constEnd())
        {
            QDomElement item = createElement(TagGroupItem);
            item.setAttribute(AttrObject, i.key());
            item.setAttribute(AttrTool, i.value());
            group.appendChild(item);
            ++i;
        }
        return group;
    }
    else
    {
    return group;
    }
}

QDomElement VAbstractPattern::addGroupItems(const QString &name, const QMap<quint32, quint32> &groupData)
{
    quint32 groupId = getGroupIdByName(name);
    const bool locked = getGroupLock(groupId);
    if (locked)
    {
      return QDomElement();
    }

    QDomElement group = getGroupByName(name);

    if (! getParameterBool(group, AttrGroupLocked, trueStr))
    {
        if (!groupData.isEmpty())
        {
            auto i = groupData.constBegin();
            while (i != groupData.constEnd())
            {
                QDomElement item = createElement(TagGroupItem);
                item.setAttribute(AttrObject, i.key());
                item.setAttribute(AttrTool, i.value());
                group.appendChild(item);
                ++i;
            }
            modified = true;
            emit patternChanged(false);

            emit updateGroups();
        }
    }
    return group;
}

//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getGroupName(quint32 id)
{
    QString name = tr("New group");
    QDomElement groups = createGroups();
    if (!groups.isNull())
    {
        QDomElement group = elementById(id, TagGroup);
        if (group.isElement())
        {
            name = GetParametrString(group, AttrName, name);
            return name;
        }
        else
        {
            if (groups.childNodes().isEmpty())
            {
                QDomNode parent = groups.parentNode();
                parent.removeChild(groups);
            }

            qDebug("Can't get group by id = %u.", id);
            return name;
        }
    }
    else
    {
        qDebug("Can't get tag Groups.");
        return name;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupName(quint32 id, const QString &name)
{
    QDomElement groups = createGroups();
    if (!groups.isNull())
    {
        QDomElement group = elementById(id, TagGroup);
        if (group.isElement())
        {
            group.setAttribute(AttrName, name);
            modified = true;
            emit patternChanged(false);
        }
        else
        {
            if (groups.childNodes().isEmpty())
            {
                QDomNode parent = groups.parentNode();
                parent.removeChild(groups);
            }

            qDebug("Can't get group by id = %u.", id);
        }
    }
    else
    {
        qDebug("Can't get tag Groups.");
    }
}

//---------------------------------------------------------------------------------------------------------------------
QMap<quint32, GroupAttributes> VAbstractPattern::getGroups()
{
    GroupAttributes groupData;
    QMap<quint32, GroupAttributes> data;

    try
    {
        QDomElement groups = createGroups();
        if (!groups.isNull())
        {
            QDomNode domNode = groups.firstChild();
            while (domNode.isNull() == false)
            {
                if (domNode.isElement())
                {
                    const QDomElement group = domNode.toElement();
                    if (group.isNull() == false)
                    {
                        if (group.tagName() == TagGroup)
                        {
                            const quint32 id = GetParametrUInt(group, AttrId, "0");
                            const QString name = GetParametrString(group, AttrName, tr("New group 2"));
                            const bool visible = getParameterBool(group, AttrVisible, trueStr);
                            const bool locked = getParameterBool(group, AttrGroupLocked, trueStr);
                            const QString color = GetParametrString(group, AttrGroupColor, ColorBlack);
                            const QString linetype = GetParametrString(group, AttrLineType, LineTypeSolidLine);
                            const QString lineweight = GetParametrString(group, AttrLineWeight, DefaultLineWeight);

                            groupData.name = name;
                            groupData.visible = visible;
                            groupData.locked = locked;
                            groupData.color = color;
                            groupData.linetype = linetype;
                            groupData.lineweight = lineweight;
                            data.insert(id, groupData);
                        }
                    }
                }
                domNode = domNode.nextSibling();
            }
            emit patternHasGroups(true);
        }
        else
        {
            emit patternHasGroups(false);
            qDebug("Can't get tag Groups.");
        }
    }
    catch (const VExceptionConversionError &)
    {
        return QMap<quint32, GroupAttributes>();
    }

    return data;
}


//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets List of Groups for the current Draft Block
///
/// @return  AttrName - Group Name.
/// @throw   VExceptionConversionError if group error.
//---------------------------------------------------------------------------------------------------------------------
QStringList VAbstractPattern::groupListByName()
{
    QStringList groupList;
    try
    {
        QDomElement groups = createGroups();
        if (!groups.isNull())
        {
            QDomNode domNode = groups.firstChild();
            if (domNode.isNull() == false)
            {
                while (domNode.isNull() == false)
                {
                    if (domNode.isElement())
                    {
                        const QDomElement group = domNode.toElement();
                        if (group.isNull() == false)
                        {
                            if (group.tagName() == TagGroup)
                            {

                                groupList <<  GetParametrString(group, AttrName);
                            }
                        }
                    }
                    domNode = domNode.nextSibling();
                }
                emit patternHasGroups(true);
            }
            else
            {
                emit patternHasGroups(false);
                qDebug("Can't get tag Groups.");
            }
        }

    }
    catch (const VExceptionConversionError &)
    {
        return groupList;
    }

    return groupList;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets Dom Element for a given group name.
///
/// @return  group - group Dom Element.
//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::getGroupByName(const QString &name)
{

    QDomElement groups = createGroups();
    if (!groups.isNull())
    {
        QDomNode domNode = groups.firstChild();
        if (domNode.isNull() == false)
        {
            while (domNode.isNull() == false)
            {
                if (domNode.isElement())
                {
                    const QDomElement group = domNode.toElement();
                    if (group.isNull() == false)
                    {
                        if (group.tagName() == TagGroup)
                        {
                            const QString groupName = GetParametrString(group, AttrName);
                            if (groupName == name)
                            {
                                return group;
                            }
                        }
                    }
                }
                domNode = domNode.nextSibling();
            }
            emit patternHasGroups(true);
        }
        else
        {
            emit patternHasGroups(false);
            qDebug("Can't get tag Groups.");
        }
    }
    return groups;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets groupId for a given group name.
///
/// @return  groupId - group Id.
//---------------------------------------------------------------------------------------------------------------------
quint32 VAbstractPattern::getGroupIdByName(const QString &name)
{

    QDomElement groups = createGroups();
    if (!groups.isNull())
    {
        QDomNode domNode = groups.firstChild();
        if (domNode.isNull() == false)
        {
            while (domNode.isNull() == false)
            {
                if (domNode.isElement())
                {
                    const QDomElement group = domNode.toElement();
                    if (group.isNull() == false)
                    {
                        if (group.tagName() == TagGroup)
                        {
                            const QString groupName = GetParametrString(group, AttrName);
                            if (groupName == name)
                            {
                                const quint32 groupId = GetParametrUInt(group, AttrId, "0");
                                return groupId;
                            }
                        }
                    }
                }
                domNode = domNode.nextSibling();
            }
        }
        else
        {
            qDebug("Can't get tag Groups.");
        }
    }
    return quint32();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Returns the groups that contain or do not contain the item identified by the toolid and the objectid
/// @param toolId
/// @param objectId
/// @param containsItem |true if the groups contain the given item, false if they don't contain the item
/// @return
//---------------------------------------------------------------------------------------------------------------------
QMap<quint32, QString> VAbstractPattern::getGroupsContainingItem(quint32 toolId, quint32 objectId, bool containsItem)
{
    QMap<quint32, QString> data;

    if(objectId == 0)
    {
        objectId = toolId;
    }

    QDomElement groups = createGroups();
    if (!groups.isNull())
    {
        QDomNode domNode = groups.firstChild();
        while (domNode.isNull() == false) // iterate through the groups
        {
            if (domNode.isElement())
            {
                const QDomElement group = domNode.toElement();
                if (group.isNull() == false)
                {
                    if (group.tagName() == TagGroup)
                    {
                        bool groupHasItem = hasGroupItem(group, toolId, objectId);
                        if((containsItem && groupHasItem) || (!containsItem && not groupHasItem))
                        {
                            const quint32 groupId = GetParametrUInt(group, AttrId, "0");
                            const QString name = GetParametrString(group, AttrName, tr("New group"));
                            data.insert(groupId, name);
                        }
                    }
                }
            }
            domNode = domNode.nextSibling();
        }
    }
    else
    {
        qDebug("Can't get tag Groups.");
    }

    return data;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Checks if the given group has the item with the given toolId and objectId
/// @param groupDomElement
/// @param toolId
/// @param objectId
/// @return
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::hasGroupItem(const QDomElement &groupDomElement, quint32 toolId, quint32 objectId)
{
    bool result = false;

    QDomNode itemNode = groupDomElement.firstChild();
    while (itemNode.isNull() == false) // iterate through the items of the group
    {
        if (itemNode.isElement())
        {
            const QDomElement item = itemNode.toElement();
            if (item.isNull() == false)
            {
                quint32 toolIdIterate= GetParametrUInt(item, AttrTool, "0");
                quint32 objectIdIterate= GetParametrUInt(item, AttrObject, "0");

                if(toolIdIterate == toolId && objectIdIterate == objectId)
                {
                    result = true;
                    break;
                }
            }
        }
        itemNode = itemNode.nextSibling();
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Deletes an item from the group containing the toolId
/// @param toolId
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::deleteToolFromGroup(quint32 toolId)
{
    QMap<quint32,QString> groupsContainingItem = getGroupsContainingItem(toolId, 0, true);
    QStringList list = QStringList(groupsContainingItem.values());
    QString  listGroupName = list.value(0);
    quint32 groupId = groupsContainingItem.key(listGroupName);

    const bool locked = getGroupLock(groupId);
    if (locked)
    {
      return;
    }

    QDomElement group = removeGroupItem(toolId, 0, groupId);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds an item to the given group with the given toolId and objectId
/// @param toolId
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::addToolToGroup(quint32 toolId, quint32 objectId, const QString &groupName)
{
    quint32 groupId = getGroupIdByName(groupName);
    const bool locked = getGroupLock(groupId);
    if (locked)
    {
        return;
    }

    //Delete tool if contained in an existing group
    QMap<quint32,QString> groupsContainingItem = getGroupsContainingItem(toolId, objectId, true);
    QStringList list = QStringList(groupsContainingItem.values());
    QString  listGroupName = list.value(0);
    groupId = groupsContainingItem.key(listGroupName);

    QDomElement group = removeGroupItem(toolId, objectId, groupId);

    //Add to new Group.
    QMap<quint32,QString> groupsNotContainingItem = getGroupsContainingItem(toolId, objectId, false);
    list = QStringList(groupsNotContainingItem.values());

    for(int i=0; i<list.count(); ++i)
    {
        const QString  listGroupName = list.value(i);
        if (groupName == listGroupName)
        {
            const quint32 groupId = groupsNotContainingItem.key(list[i]);
            QDomElement group = addGroupItem(toolId, objectId, groupId);
            modified = true;
            emit patternChanged(false);
            emit updateGroups();
            QDomElement groups = createGroups();
            if (!groups.isNull())
            {
                parseGroups(groups);
            }
            break;
        }
    }

}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds an item to the given group with the given toolId and objectId
/// @param toolId
/// @param objectId
/// @param groupId
//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::addGroupItem(quint32 toolId, quint32 objectId, quint32 groupId)
{

    const bool locked = getGroupLock(groupId);
    if (locked)
    {
      return QDomElement();
    }

    QDomElement group = elementById(groupId, TagGroup);

    if (!group.isNull())
    {
        if(objectId == 0)
        {
            objectId = toolId;
        }

        QDomElement item = createElement(TagGroupItem);
        item.setAttribute(AttrTool, toolId);
        item.setAttribute(AttrObject, objectId);
        group.appendChild(item);

        modified = true;
        emit patternChanged(false);

        emit updateGroups();

        QDomElement groups = createGroups();
        if (!groups.isNull())
        {
            parseGroups(groups);
        }

        return item;
    }
    else
    {
        qDebug() << "The group of id " << groupId << " doesn't exist";
    }

    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Removes the item of given toolId and objectId from the group of given groupId
/// @param toolId
/// @param objectId
/// @param groupId
//---------------------------------------------------------------------------------------------------------------------
QDomElement VAbstractPattern::removeGroupItem(quint32 toolId, quint32 objectId, quint32 groupId)
{
    QDomElement group = elementById(groupId, TagGroup);

    const bool locked = getGroupLock(groupId);
    if (locked)
    {
      return QDomElement();
    }

    if (!group.isNull())
    {
        if(objectId == 0)
        {
            objectId = toolId;
        }

        QDomNode itemNode = group.firstChild();
        while (itemNode.isNull() == false) // iterate through the items of the group
        {
            if (itemNode.isElement())
            {
                const QDomElement item = itemNode.toElement();
                if (item.isNull() == false)
                {
                    quint32 toolIdIterate= GetParametrUInt(item, AttrTool, "0");
                    quint32 objectIdIterate= GetParametrUInt(item, AttrObject, "0");

                    if(toolIdIterate == toolId && objectIdIterate == objectId)
                    {
                        group.removeChild(itemNode);

                        // to signalised that the pattern was changed and need to be saved
                        modified = true;
                        emit patternChanged(false);

                        // to update the group table of the gui
                        emit updateGroups();

                        // parse the groups to update the drawing, in case the item was removed from an invisible group
                        QDomElement groups = createGroups();
                        if (!groups.isNull())
                        {
                            parseGroups(groups);

                            VAbstractTool *tool = qobject_cast<VAbstractTool *>(VAbstractPattern::getTool(toolId));
                            tool->GroupVisibility(objectId, true);
                        }

                        return item;
                    }
                }
            }
            itemNode = itemNode.nextSibling();
        }
    }
    else
    {
        qDebug() << "The group of id " << groupId << " doesn't exist";
    }

    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Returns true if the given group is empty
/// @param id
/// @return
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::isGroupEmpty(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);

    if (group.isNull() == false)
    {
        return not group.hasChildNodes();
    }
    else
    {
        qDebug() << "The group of id " << id << " doesn't exist";
        return true;
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::getGroupVisibility(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        return getParameterBool(group, AttrVisible, trueStr);
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
        return true;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupVisibility(quint32 id, bool visible)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        SetAttribute(group, AttrVisible, visible);
        modified = true;
        emit patternChanged(false);

        QDomElement groups = createGroups();
        if (!groups.isNull())
        {
            parseGroups(groups);
        }
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
    }
}


//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets whether a Group is locked or not
/// @param  id - Tool Id.
/// @return AttrGroupLocked - True if locked False if unlocked.
/// @return True if can't find group by id.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::getGroupLock(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        return getParameterBool(group, AttrGroupLocked, trueStr);
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
        return true;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupLock(quint32 id, bool locked)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        SetAttribute(group, AttrGroupLocked, locked);
        modified = true;
        emit patternChanged(false);
        //qDebug("VAbstractPattern::setGroupLock - Group %u is locked.", id);

        QDomElement groups = createGroups();
        if (!groups.isNull())
        {
            parseGroups(groups);
        }
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets the Group color
/// @param  id - Tool Id.
/// @return AttrGroupColor - Color of the group.
/// @return color  - group color.
//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getGroupColor(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);
    QString color;
    if (group.isElement())
    {

        color = GetParametrString(group, AttrGroupColor, ColorBlack);
        return color;
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
        return ColorBlack;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupColor(quint32 id, QString color)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        SetAttribute(group, AttrGroupColor, color);
        modified = true;
        emit patternChanged(false);
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets the Group Line Type
/// @param  id - Tool Id.
/// @return AttrLineType - Line type of the group.
/// @return type  - group line type.
//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getGroupLineType(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);
    QString type;
    if (group.isElement())
    {

        type = GetParametrString(group, AttrLineType, LineTypeSolidLine);
        return type;
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
        return LineTypeSolidLine;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupLineType(quint32 id, QString type)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        SetAttribute(group, AttrLineType, type);
        modified = true;
        emit patternChanged(false);
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets the Group Line Weight
/// @param  id - Tool Id.
/// @return AttrLineWeight - Linw weight of the group.
/// @return weight  - group line weight.
//---------------------------------------------------------------------------------------------------------------------
QString VAbstractPattern::getGroupLineWeight(quint32 id)
{
    QDomElement group = elementById(id, TagGroup);
    QString weight;
    if (group.isElement())
    {

        weight = GetParametrString(group, AttrLineWeight, DefaultLineWeight);
        return weight;
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
        return ColorBlack;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::setGroupLineWeight(quint32 id, QString weight)
{
    QDomElement group = elementById(id, TagGroup);
    if (group.isElement())
    {
        SetAttribute(group, AttrLineWeight, weight);
        modified = true;
        emit patternChanged(false);
    }
    else
    {
        qDebug("Can't get group by id = %u.", id);
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief  Gets whether a group name already exists
/// @param  groupName - group name.
/// @return exists - True id name exists False if it does not.
//---------------------------------------------------------------------------------------------------------------------
bool VAbstractPattern::groupNameExists(const QString &groupName)
{
    QStringList groupList = groupListByName();
    bool exists = groupList.contains(groupName, Qt::CaseInsensitive);
    return exists;
}

QString VAbstractPattern::useGroupColor(quint32 toolId, QString color)
{
    QMap<quint32,QString> groupsContainingItem = getGroupsContainingItem(toolId, 0, true);
    QStringList list = QStringList(groupsContainingItem.values());
    QString  groupName = list.value(0);

    quint32 objectId = toolId;
    bool groupHasItem = hasGroupItem(getGroupByName(groupName), toolId, objectId);
    if ((color == ColorByGroup) && groupHasItem)
    {
        QString groupColor = getGroupColor(getGroupIdByName(groupName));
        return groupColor;
    }
    else
    {
        return color;
    }
}

QString VAbstractPattern::useGroupLineType(quint32 toolId, QString type)
{
    QMap<quint32,QString> groupsContainingItem = getGroupsContainingItem(toolId, 0, true);
    QStringList list = QStringList(groupsContainingItem.values());
    QString  groupName = list.value(0);

    quint32 objectId = toolId;
    bool groupHasItem = hasGroupItem(getGroupByName(groupName), toolId, objectId);

    if ((type == LineTypeByGroup) && groupHasItem)
    {
        QString groupLineType = getGroupLineType(getGroupIdByName(groupName));
        return groupLineType;
    }
    else
    {
        return type;
    }
}

QString VAbstractPattern::useGroupLineWeight(quint32 toolId, QString weight)
{
    QMap<quint32,QString> groupsContainingItem = getGroupsContainingItem(toolId, 0, true);
    QStringList list = QStringList(groupsContainingItem.values());
    QString  groupName = list.value(0);

    quint32 objectId = toolId;
    bool groupHasItem = hasGroupItem(getGroupByName(groupName), toolId, objectId);
    if ((weight == LineWeightByGroup) && groupHasItem)
    {
        QString groupLineWeight = getGroupLineWeight(getGroupIdByName(groupName));
        return groupLineWeight;
    }
    else
    {
        return weight;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getBackgroundImageMap Get the image map.
///
/// This method gets the image map of image ids and ImageItems.
///
/// @return Qmap of image ids and ImageItems.
//---------------------------------------------------------------------------------------------------------------------
QMap<qint32, ImageItem *> VAbstractPattern::getBackgroundImageMap()
{
    return m_imageMap;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief addBackgroundImage Add image to image map by id.
///
/// This method adds the image to image map.
///
/// @param id Id of image.
/// @param item ImageItem data struct of image.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::addBackgroundImage(qint32 id, ImageItem *item)
{
    m_imageMap.insert(id, item);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief removeBackgroundImage Remove image from image map by id.
///
/// This method removes the image from the image map.
///
/// @param id Id of image.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::removeBackgroundImage(qint32 id)
{
    m_imageMap.remove(id);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief getBackgroundImage Get image by id.
///
/// This method gets an ImageItem data struct by id.
///
/// @param id Image id.
/// @return ImgaeItem data struct for image.
//---------------------------------------------------------------------------------------------------------------------
ImageItem *VAbstractPattern::getBackgroundImage(qint32 id)
{
    return m_imageMap.value(id);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief clearBackgroundImageMap  Clear the image map.
///
/// This method clears the image map of all image items. Needed when closing a pattern.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::clearBackgroundImageMap()
{
    m_imageMap.clear();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief clearHistory Clear the tool history.
///
/// This method clears the History vector of all too records. Needed when closing a pattern.
//---------------------------------------------------------------------------------------------------------------------
void VAbstractPattern::clearHistory()
{
    m_history.clear();
}
