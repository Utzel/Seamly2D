//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_patternseams.cpp
//  @author Julius
//  @date   5 Oct, 2026
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

#include "tst_patternseams.h"

#include <QDomElement>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUndoStack>
#include <QtTest>

#include "../ifc/exception/vexception.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/xml/vpatternconverter.h"
#include "../vtools/undocommands/save_arrangements.h"
#include "../vtools/undocommands/save_fabrics.h"
#include "../vtools/undocommands/save_seams.h"
#include "../vtools/undocommands/save_topstitches.h"

namespace
{
// Just enough of a pattern document to hold seams; the pattern's own logic isn't needed here.
class SeamsPattern : public VAbstractPattern
{
public:
    explicit SeamsPattern(const QString& version = VPatternConverter::PatternMaxVerStr)
        : VAbstractPattern()
    {
        setContent(QStringLiteral("<?xml version='1.0' encoding='UTF-8'?>"
                                  "<pattern><version>%1</version><unit>cm</unit><measurements/>"
                                  "<finalMeasurements><finalMeasurement name=\"length\" formula=\"10\"/>"
                                  "</finalMeasurements>"
                                  "<draftBlock name=\"A\"><calculation/><modeling/><pieces/></draftBlock>"
                                  "<draftBlock name=\"B\"><calculation/><modeling/><pieces/></draftBlock>"
                                  "</pattern>").arg(version));
    }

    virtual void CreateEmptyFile() override
    {}

    virtual void IncrementReferens(quint32 id) const override
    {
        Q_UNUSED(id)
    }

    virtual void DecrementReferens(quint32 id) const override
    {
        Q_UNUSED(id)
    }

    virtual QStringList GetCurrentAlphabet() const override
    {
        return QStringList();
    }

    virtual QString GenerateLabel(const LabelType& type, const QString& reservedName) const override
    {
        Q_UNUSED(type)
        Q_UNUSED(reservedName)
        return QString();
    }

    virtual QString generateSuffix(const QString& type) const override
    {
        Q_UNUSED(type)
        return QString();
    }

    virtual void UpdateToolData(const quint32& id, VContainer* data) override
    {
        Q_UNUSED(id)
        Q_UNUSED(data)
    }

    virtual void LiteParseTree(const Document& parse) override
    {
        Q_UNUSED(parse)
    }
};

//---------------------------------------------------------------------------------------------------------------------
VSeam seam(quint32 first_piece, quint32 second_piece, bool reverse)
{
    VSeam made;
    made.first.piece_id = first_piece;
    made.first.start_node = first_piece + 1;
    made.first.end_node = first_piece + 2;
    made.second.piece_id = second_piece;
    made.second.start_node = second_piece + 1;
    made.second.end_node = second_piece + 2;
    made.reverse = reverse;
    return made;
}

//---------------------------------------------------------------------------------------------------------------------
VPieceArrangement arrangement(quint32 piece, const QString& part, qreal angle, qreal height)
{
    VPieceArrangement made;
    made.piece_id = piece;
    made.part = part;
    made.angle = angle;
    made.height = height;
    return made;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList childTags(const QDomElement& element)
{
    QStringList tags;
    for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
    {
        tags.append(child.tagName());
    }
    return tags;
}

//---------------------------------------------------------------------------------------------------------------------
bool writeFile(const QString& path, const QString& content)
{
    QFile file(path);
    const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    if (opened)
    {
        QTextStream(&file) << content;
    }
    return opened;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_PatternSeams::TST_PatternSeams(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::seamsAreReadBack() const
{
    SeamsPattern pattern;
    const QVector<VSeam> seams = {seam(10, 20, false), seam(30, 30, true)};
    pattern.setSeams(seams);

    QCOMPARE(pattern.getSeams(), seams);

    // Seams sewn the usual way don't spell it out.
    const QDomElement first = pattern.documentElement().firstChildElement(VAbstractPattern::TagSeams)
                                                       .firstChildElement(VAbstractPattern::TagSeam);
    QVERIFY(!first.hasAttribute(VAbstractPattern::AttrNodeReverse));
    QCOMPARE(first.nextSiblingElement().attribute(VAbstractPattern::AttrNodeReverse), QStringLiteral("true"));
}

//---------------------------------------------------------------------------------------------------------------------
// The seams have to go between the final measurements and the draft blocks, or the file doesn't open again.
void TST_PatternSeams::seamsFollowTheSchema() const
{
    SeamsPattern pattern;
    pattern.setSeams({seam(10, 20, false), seam(30, 40, true)});

    QCOMPARE(childTags(pattern.documentElement()),
             QStringList({QStringLiteral("version"), QStringLiteral("unit"), QStringLiteral("measurements"),
                          QStringLiteral("finalMeasurements"), QStringLiteral("seams"), QStringLiteral("draftBlock"),
                          QStringLiteral("draftBlock")}));

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath(QStringLiteral("seams.sm2d"));
    QVERIFY(writeFile(path, pattern.toString()));
    try
    {
        VDomDocument::ValidateXML(VPatternConverter::CurrentSchema, path);
    }
    catch (const VException& error)
    {
        QFAIL(qUtf8Printable(error.ErrorMessage()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::noSeamsLeaveNoElement() const
{
    SeamsPattern pattern;
    pattern.setSeams({seam(10, 20, false)});
    pattern.setSeams(QVector<VSeam>());

    QVERIFY(pattern.documentElement().firstChildElement(VAbstractPattern::TagSeams).isNull());
    QVERIFY(pattern.getSeams().isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::undoRestoresSeams() const
{
    SeamsPattern pattern;
    const QVector<VSeam> before = {seam(10, 20, false)};
    const QVector<VSeam> after = {seam(10, 20, false), seam(30, 40, true)};
    pattern.setSeams(before);

    QSignalSpy changes(&pattern, &VAbstractPattern::seamsChanged);
    QUndoStack stack;
    stack.push(new SaveSeams(QStringLiteral("sew"), before, after, &pattern));
    QCOMPARE(pattern.getSeams(), after);

    stack.undo();
    QCOMPARE(pattern.getSeams(), before);

    stack.redo();
    QCOMPARE(pattern.getSeams(), after);
    QCOMPARE(changes.count(), 3);
}

//---------------------------------------------------------------------------------------------------------------------
// Patterns saved before seams existed open as they are.
void TST_PatternSeams::olderPatternsAreConverted() const
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath(QStringLiteral("old.sm2d"));
    QVERIFY(writeFile(path, SeamsPattern(QStringLiteral("0.7.5")).toString()));

    try
    {
        VPatternConverter converter(path);
        SeamsPattern converted;
        converted.setXMLContent(converter.Convert());

        QCOMPARE(converted.documentElement().firstChildElement(VDomDocument::TagVersion).text(),
                 VPatternConverter::PatternMaxVerStr);
        QVERIFY(converted.getSeams().isEmpty());
    }
    catch (const VException& error)
    {
        QFAIL(qUtf8Printable(error.ErrorMessage()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::arrangementsAreReadBack() const
{
    SeamsPattern pattern;
    const QVector<VPieceArrangement> arrangements = {arrangement(10, QStringLiteral("body"), 0, 120.5),
                                                     arrangement(20, QStringLiteral("leftLeg"), 180, 60)};
    pattern.setArrangements(arrangements);

    QCOMPARE(pattern.getArrangements(), arrangements);
}

//---------------------------------------------------------------------------------------------------------------------
// Whichever is made first, the seams come before the arrangements, those before the fabrics, those before the
// topstitching, and all before the draft blocks. Pieces can be arranged on every part of the body.
void TST_PatternSeams::garmentDataKeepsSchemaOrder() const
{
    SeamsPattern pattern;
    VTopstitches topstitches;
    topstitches.all = true;
    topstitches.style = QStringLiteral("double");
    topstitches.color = QStringLiteral("#c8962d");
    topstitches.segments = {{10, 1, 2, false, QString()}, {20, 3, 4, true, QStringLiteral("jeans")}};
    pattern.setTopstitches(topstitches);
    VGarmentFabrics fabrics;
    fabrics.garment = QStringLiteral("denim");
    fabrics.pieces = {{20, QStringLiteral("chiffon")}};
    pattern.setFabrics(fabrics);
    pattern.setArrangements({arrangement(10, QStringLiteral("body"), 0, 120),
                             arrangement(20, QStringLiteral("leftLeg"), 0, 60),
                             arrangement(30, QStringLiteral("rightLeg"), 0, 60),
                             arrangement(40, QStringLiteral("leftArm"), 90, 115),
                             arrangement(50, QStringLiteral("rightArm"), -90, 115)});
    pattern.setSeams({seam(10, 20, false)});

    QCOMPARE(childTags(pattern.documentElement()),
             QStringList({QStringLiteral("version"), QStringLiteral("unit"), QStringLiteral("measurements"),
                          QStringLiteral("finalMeasurements"), QStringLiteral("seams"), QStringLiteral("arrangements"),
                          QStringLiteral("fabrics"), QStringLiteral("topstitches"), QStringLiteral("draftBlock"),
                          QStringLiteral("draftBlock")}));

    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString path = folder.filePath(QStringLiteral("garment.sm2d"));
    QVERIFY(writeFile(path, pattern.toString()));
    try
    {
        VDomDocument::ValidateXML(VPatternConverter::CurrentSchema, path);
    }
    catch (const VException& error)
    {
        QFAIL(qUtf8Printable(error.ErrorMessage()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::undoRestoresArrangements() const
{
    SeamsPattern pattern;
    const QVector<VPieceArrangement> before = {arrangement(10, QStringLiteral("body"), 0, 120)};
    const QVector<VPieceArrangement> after = {arrangement(10, QStringLiteral("body"), 90, 110)};
    pattern.setArrangements(before);

    QSignalSpy changes(&pattern, &VAbstractPattern::arrangementsChanged);
    QUndoStack stack;
    stack.push(new SaveArrangements(QStringLiteral("place"), before, after, &pattern));
    QCOMPARE(pattern.getArrangements(), after);

    stack.undo();
    QCOMPARE(pattern.getArrangements(), before);
    QCOMPARE(changes.count(), 2);
}

//---------------------------------------------------------------------------------------------------------------------
// A piece without a fabric of its own is cut from the garment's; with no fabrics at all, nothing is stored.
void TST_PatternSeams::fabricsAreReadBack() const
{
    SeamsPattern pattern;
    VGarmentFabrics fabrics;
    fabrics.garment = QStringLiteral("cottonJersey");
    fabrics.pieces = {{10, QStringLiteral("denim")}, {30, QStringLiteral("chiffon")}};
    pattern.setFabrics(fabrics);

    QCOMPARE(pattern.getFabrics(), fabrics);
    QCOMPARE(pattern.getFabrics().of(10), QStringLiteral("denim"));
    QCOMPARE(pattern.getFabrics().of(20), QStringLiteral("cottonJersey"));

    pattern.setFabrics(VGarmentFabrics());
    QVERIFY(pattern.documentElement().firstChildElement(QStringLiteral("fabrics")).isNull());
    QCOMPARE(pattern.getFabrics(), VGarmentFabrics());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::undoRestoresFabrics() const
{
    SeamsPattern pattern;
    VGarmentFabrics before;
    before.garment = QStringLiteral("cottonShirting");
    VGarmentFabrics after = before;
    after.pieces = {{10, QStringLiteral("denim")}};
    pattern.setFabrics(before);

    QSignalSpy changes(&pattern, &VAbstractPattern::fabricsChanged);
    QUndoStack stack;
    stack.push(new SaveFabrics(QStringLiteral("fabric"), before, after, &pattern));
    QCOMPARE(pattern.getFabrics(), after);

    stack.undo();
    QCOMPARE(pattern.getFabrics(), before);
    QCOMPARE(changes.count(), 2);
}

//---------------------------------------------------------------------------------------------------------------------
// A segment is stitched as its own entry says, or as the whole garment is, in its own style or the garment's; with
// nothing stitched, nothing is stored.
void TST_PatternSeams::topstitchesAreReadBack() const
{
    SeamsPattern pattern;
    VTopstitches topstitches;
    topstitches.segments = {{10, 1, 2, true, QString()}, {10, 2, 3, false, QString()},
                            {10, 4, 5, true, QStringLiteral("twinNeedle")}};
    pattern.setTopstitches(topstitches);

    QCOMPARE(pattern.getTopstitches(), topstitches);
    QVERIFY(pattern.getTopstitches().isStitched(10, 1, 2));
    QVERIFY(!pattern.getTopstitches().isStitched(10, 2, 3));
    QVERIFY(!pattern.getTopstitches().isStitched(10, 3, 4));
    QCOMPARE(pattern.getTopstitches().styleOf(10, 1, 2), QString());
    QCOMPARE(pattern.getTopstitches().styleOf(10, 4, 5), QStringLiteral("twinNeedle"));

    topstitches.all = true;
    topstitches.style = QStringLiteral("edge");
    topstitches.color = QStringLiteral("#202020");
    pattern.setTopstitches(topstitches);
    QCOMPARE(pattern.getTopstitches(), topstitches);
    QVERIFY(pattern.getTopstitches().isStitched(10, 3, 4));
    QVERIFY(!pattern.getTopstitches().isStitched(10, 2, 3));
    QCOMPARE(pattern.getTopstitches().styleOf(10, 3, 4), QStringLiteral("edge"));
    QCOMPARE(pattern.getTopstitches().styleOf(10, 4, 5), QStringLiteral("twinNeedle"));

    // Back to the 3D View's style and thread, they are left out.
    topstitches.style.clear();
    topstitches.color.clear();
    pattern.setTopstitches(topstitches);
    const QDomElement element = pattern.documentElement().firstChildElement(QStringLiteral("topstitches"));
    QVERIFY(!element.hasAttribute(QStringLiteral("style")) && !element.hasAttribute(QStringLiteral("color")));
    QCOMPARE(pattern.getTopstitches(), topstitches);

    pattern.setTopstitches(VTopstitches());
    QVERIFY(pattern.documentElement().firstChildElement(QStringLiteral("topstitches")).isNull());
    QCOMPARE(pattern.getTopstitches(), VTopstitches());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_PatternSeams::undoRestoresTopstitches() const
{
    SeamsPattern pattern;
    VTopstitches before;
    before.segments = {{10, 1, 2, true, QString()}};
    VTopstitches after = before;
    after.all = true;
    after.style = QStringLiteral("jeans");
    pattern.setTopstitches(before);

    QSignalSpy changes(&pattern, &VAbstractPattern::topstitchesChanged);
    QUndoStack stack;
    stack.push(new SaveTopstitches(QStringLiteral("topstitch"), before, after, &pattern));
    QCOMPARE(pattern.getTopstitches(), after);

    stack.undo();
    QCOMPARE(pattern.getTopstitches(), before);
    QCOMPARE(changes.count(), 2);
}
