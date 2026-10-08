//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_bodymodel.cpp
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

#include "tst_bodymodel.h"

#include <QLineF>
#include <QSet>
#include <QtMath>
#include <QtTest>

#include <limits>

#include "../vgarment/body_collider.h"
#include "../vgarment/body_data.h"
#include "../vgarment/body_fitter.h"
#include "../vgarment/body_measurer.h"
#include "../vgarment/body_model.h"
#include "../vgarment/body_wrap.h"
#include "../vgarment/piece_mesher.h"
#include "../vgarment/standard_sizes.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
BodyShape female()
{
    BodyShape shape;
    shape.gender = 0.0;
    return shape;
}

//---------------------------------------------------------------------------------------------------------------------
// Adds an upright cylinder of `sides` flat faces, 0 to 100 cm high, to a mesh.
void addCylinder(QVector<QVector3D>& positions, QVector<quint32>& triangles, float center_x, float radius, int sides)
{
    const quint32 first = static_cast<quint32>(positions.size());
    for (int i = 0; i < sides; ++i)
    {
        const qreal angle = 2.0 * M_PI * i / sides;
        const float x = center_x + radius * static_cast<float>(qCos(angle));
        const float z = radius * static_cast<float>(qSin(angle));
        positions << QVector3D(x, 0, z) << QVector3D(x, 100, z);
    }
    for (int i = 0; i < sides; ++i)
    {
        const quint32 bottom = first + static_cast<quint32>(2 * i);
        const quint32 next_bottom = first + static_cast<quint32>(2 * ((i + 1) % sides));
        triangles << bottom << next_bottom << bottom + 1 << next_bottom << next_bottom + 1 << bottom + 1;
    }
}

//---------------------------------------------------------------------------------------------------------------------
qreal lowestSkinY(const BodyModel& model, const QVector<QVector3D>& positions)
{
    float lowest = std::numeric_limits<float>::max();
    for (int i = 0; i < model.skinVertexCount(); ++i)
    {
        lowest = qMin(lowest, positions.at(i).y());
    }
    return lowest;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_BodyModel::TST_BodyModel(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
void TST_BodyModel::bodyDataLoads() const
{
    const QSharedPointer<const BodyData> data = BodyData::standard();

    QVERIFY(!data.isNull());
    QCOMPARE(data->skin_vertex_count, 13380);
    QCOMPARE(static_cast<int>(data->triangles.size()), 3 * 26756);
    QVERIFY(data->joints.contains(QStringLiteral("pelvis")));
    QVERIFY(data->joints.contains(QStringLiteral("l-shoulder")));
    QVERIFY(data->targets.contains(QStringLiteral("macrodetails/caucasian-female-young")));
    QVERIFY(data->targets.contains(QStringLiteral("torso/measure-waist-circ-incr")));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_BodyModel::ageFromYears() const
{
    QCOMPARE(BodyShape::ageFromYears(1), 0.0);
    QCOMPARE(BodyShape::ageFromYears(11), 0.1875);
    QCOMPARE(BodyShape::ageFromYears(25), 0.5);
    QCOMPARE(BodyShape::ageFromYears(90), 1.0);
    QCOMPARE(BodyShape::ageFromYears(120), 1.0);
}

//---------------------------------------------------------------------------------------------------------------------
// A young adult woman is a third each of MakeHuman's three ethnic female shapes, and average breasts need no target.
void TST_BodyModel::macroTargetsFollowMpfb() const
{
    QHash<QString, qreal> weights;
    for (const QPair<QString, qreal>& target : BodyModel::macroTargets(female()))
    {
        weights.insert(target.first, target.second);
    }

    for (const QString& race : {QStringLiteral("african"), QStringLiteral("asian"), QStringLiteral("caucasian")})
    {
        const qreal weight = weights.value(QStringLiteral("macrodetails/%1-female-young").arg(race));
        QVERIFY2(qAbs(weight - 1.0 / 3.0) < 0.01, qUtf8Printable(QStringLiteral("%1: %2").arg(race).arg(weight)));
    }
    for (auto weight = weights.constBegin(); weight != weights.constEnd(); ++weight)
    {
        QVERIFY2(!weight.key().startsWith(QLatin1String("breast/")) || weight.value() < 0.01,
                 qUtf8Printable(weight.key()));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// MakeHuman's default woman and man are about 159 and 173 cm tall.
void TST_BodyModel::defaultBodiesHaveTheirHeights() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);

    BodyShape male;
    male.gender = 1.0;

    QVERIFY(qAbs(measurer.height(model.evaluate(female())) - 159.1) < 1.0);
    QVERIFY(qAbs(measurer.height(model.evaluate(male)) - 173.0) < 1.0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_BodyModel::bodyStandsOnTheFloor() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));

    QVERIFY(qAbs(lowestSkinY(model, positions)) < 0.001);
    QVERIFY(qAbs(pelvis.x()) < 0.001 && qAbs(pelvis.z()) < 0.001);
    QVERIFY(pelvis.y() > 70 && pelvis.y() < 100);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_BodyModel::scaleSetsHeight() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);

    BodyShape taller = female();
    taller.scale = 1.1;

    const qreal difference = measurer.height(model.evaluate(taller)) - measurer.height(model.evaluate(female())) * 1.1;
    QVERIFY2(qAbs(difference) < 0.001, qUtf8Printable(QString::number(difference)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_BodyModel::measureTargetChangesGirth() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);

    BodyShape wider = female();
    wider.measures.insert(QStringLiteral("torso/measure-waist-circ"), 1.0);
    BodyShape narrower = female();
    narrower.measures.insert(QStringLiteral("torso/measure-waist-circ"), -1.0);

    const qreal waist = measurer.waist(model.evaluate(female()));
    QVERIFY(waist > 55 && waist < 80);
    QVERIFY(measurer.waist(model.evaluate(wider)) > waist + 2);
    QVERIFY(measurer.waist(model.evaluate(narrower)) < waist - 2);
}

//---------------------------------------------------------------------------------------------------------------------
// A slice through a cylinder of flat faces is a regular polygon around its corners.
void TST_BodyModel::tapeGirthOfCylinder() const
{
    QVector<QVector3D> positions;
    QVector<quint32> triangles;
    addCylinder(positions, triangles, 0, 10, 24);

    const qreal polygon = 2 * 24 * 10 * qSin(M_PI / 24);
    QVERIFY(qAbs(BodyMeasurer::tapeGirth(positions, triangles, 50, 5) - polygon) < 1e-3);
}

//---------------------------------------------------------------------------------------------------------------------
// Slices of the arms are left out, slices of the torso kept. Kept arms would stretch the tape to about 142 cm: four
// tangents of 29.2 cm between torso and arms plus the arcs around them.
void TST_BodyModel::tapeLeavesOutArms() const
{
    QVector<QVector3D> positions;
    QVector<quint32> triangles;
    addCylinder(positions, triangles, 0, 10, 24);
    addCylinder(positions, triangles, 30, 3, 12);
    addCylinder(positions, triangles, -30, 3, 12);

    const qreal torso = 2 * 24 * 10 * qSin(M_PI / 24);
    QVERIFY(qAbs(BodyMeasurer::tapeGirth(positions, triangles, 50, 20) - torso) < 1e-3);

    const qreal with_arms = BodyMeasurer::tapeGirth(positions, triangles, 50, 40);
    QVERIFY2(qAbs(with_arms - 142) < 2, qUtf8Printable(QString::number(with_arms)));
}

//---------------------------------------------------------------------------------------------------------------------
// The arm is measured from the shoulder tip, on top of the shoulder over its joint, by the elbow to the wrist, and
// MakeHuman's arm length targets change the upper arm and the forearm on their own.
void TST_BodyModel::armLengthsRunFromTheShoulderTip() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);
    const QVector<QVector3D> positions = model.evaluate(female());
    const QVector3D shoulder = model.joint(positions, QStringLiteral("l-shoulder"));
    const QVector3D elbow = model.joint(positions, QStringLiteral("l-elbow"));
    const QVector3D wrist = model.joint(positions, QStringLiteral("l-hand"));

    const qreal upper_arm = measurer.upperArm(positions);
    const qreal joints_apart = (elbow - shoulder).length();
    QVERIFY2(upper_arm > joints_apart + 2 && upper_arm < joints_apart + 8, qUtf8Printable(QString::number(upper_arm)));
    QVERIFY(qAbs(measurer.lowerArm(positions) - (wrist - elbow).length()) < 1e-4);
    QVERIFY(qAbs(measurer.arm(positions) - upper_arm - measurer.lowerArm(positions)) < 1e-4);

    BodyShape longer_upper_arm = female();
    longer_upper_arm.measures.insert(QStringLiteral("arms/measure-upperarm-length"), 1.0);
    const QVector<QVector3D> long_upper = model.evaluate(longer_upper_arm);
    QVERIFY(measurer.upperArm(long_upper) > upper_arm + 3);
    QVERIFY(qAbs(measurer.lowerArm(long_upper) - measurer.lowerArm(positions)) < 0.1);

    BodyShape shorter_forearm = female();
    shorter_forearm.measures.insert(QStringLiteral("arms/measure-lowerarm-length"), -1.0);
    const QVector<QVector3D> short_forearm = model.evaluate(shorter_forearm);
    QVERIFY(measurer.lowerArm(short_forearm) < measurer.lowerArm(positions) - 3);
    QVERIFY(qAbs(measurer.upperArm(short_forearm) - upper_arm) < 0.1);
}

//---------------------------------------------------------------------------------------------------------------------
// The crotch is where the legs part, in the middle of the body, and stays there when full thighs touch below it.
// MakeHuman's leg targets change the heights of the knee and the crotch and the girths of the knee and the calf.
void TST_BodyModel::legsAreMeasuredFromTheFloor() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);
    const QVector<QVector3D> positions = model.evaluate(female());
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
    const QVector3D hip_joint = model.joint(positions, QStringLiteral("l-upper-leg"));
    const QVector3D knee = model.joint(positions, QStringLiteral("l-knee"));

    const QVector3D crotch = model.crotch(positions);
    QVERIFY(qAbs(crotch.x() - pelvis.x()) < 0.01f);
    QVERIFY2(crotch.y() < hip_joint.y() - 5 && crotch.y() > knee.y() + 15,
             qUtf8Printable(QStringLiteral("crotch at %1, hip joint at %2").arg(crotch.y()).arg(hip_joint.y())));
    const qreal floor = lowestSkinY(model, positions);
    QVERIFY(qAbs(measurer.crotch(positions) - (crotch.y() - floor)) < 1e-4);
    QVERIFY(qAbs(measurer.kneeHeight(positions) - (knee.y() - floor)) < 1e-4);

    auto changed = [&model](const QString& target, qreal value)
    {
        BodyShape shape = female();
        shape.measures.insert(target, value);
        return model.evaluate(shape);
    };

    const QVector<QVector3D> full_thighs = changed(QStringLiteral("legs/measure-thigh-circ"), 1.0);
    QVERIFY2(qAbs(measurer.crotch(full_thighs) - measurer.crotch(positions)) < 0.5,
             qUtf8Printable(QString::number(measurer.crotch(full_thighs))));

    const QVector<QVector3D> long_thighs = changed(QStringLiteral("legs/measure-upperleg-height"), 1.0);
    QVERIFY(measurer.crotch(long_thighs) > measurer.crotch(positions) + 3);
    QVERIFY(qAbs(measurer.kneeHeight(long_thighs) - measurer.kneeHeight(positions)) < 0.5);
    const QVector<QVector3D> long_shins = changed(QStringLiteral("legs/measure-lowerleg-height"), 1.0);
    QVERIFY(measurer.kneeHeight(long_shins) > measurer.kneeHeight(positions) + 3);

    const qreal knee_girth = measurer.knee(positions);
    const qreal calf = measurer.calf(positions);
    QVERIFY2(knee_girth > 25 && knee_girth < 45 && calf > 25 && calf < 45,
             qUtf8Printable(QStringLiteral("knee %1, calf %2").arg(knee_girth).arg(calf)));
    const QVector<QVector3D> full_calves = changed(QStringLiteral("legs/measure-calf-circ"), 1.0);
    QVERIFY(measurer.calf(full_calves) > calf + 3);
    const QVector<QVector3D> full_knees = changed(QStringLiteral("legs/measure-knee-circ"), 1.0);
    QVERIFY(measurer.knee(full_knees) > knee_girth + 3);
}

//---------------------------------------------------------------------------------------------------------------------
// Measure a known body and fit to those measurements: the fitted body has to measure the same.
void TST_BodyModel::fitMatchesMeasurements() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);

    BodyShape known = female();
    known.weight = 0.7;
    known.scale = 1.05;
    known.measures.insert(QStringLiteral("torso/measure-waist-circ"), 0.4);
    known.measures.insert(QStringLiteral("torso/measure-bust-circ"), -0.3);
    known.measures.insert(QStringLiteral("arms/measure-upperarm-length"), 0.4);
    known.measures.insert(QStringLiteral("arms/measure-lowerarm-length"), -0.3);
    known.measures.insert(QStringLiteral("legs/measure-upperleg-height"), 0.3);
    known.measures.insert(QStringLiteral("legs/measure-lowerleg-height"), -0.2);
    known.measures.insert(QStringLiteral("legs/measure-calf-circ"), -0.3);
    const BodyMeasurements wanted = measurer.measure(model.evaluate(known));

    const BodyFit fit = BodyFitter(model).fit(wanted, known.gender, known.age);

    QVERIFY2(qAbs(fit.measured.height - wanted.height) < 0.5, qUtf8Printable(QString::number(fit.measured.height)));
    QVERIFY2(qAbs(fit.measured.bust - wanted.bust) < 1.0, qUtf8Printable(QString::number(fit.measured.bust)));
    QVERIFY2(qAbs(fit.measured.waist - wanted.waist) < 1.0, qUtf8Printable(QString::number(fit.measured.waist)));
    QVERIFY2(qAbs(fit.measured.hip - wanted.hip) < 1.0, qUtf8Printable(QString::number(fit.measured.hip)));
    QVERIFY2(qAbs(fit.measured.neck - wanted.neck) < 1.0, qUtf8Printable(QString::number(fit.measured.neck)));
    QVERIFY2(qAbs(fit.measured.upper_arm - wanted.upper_arm) < 0.5,
             qUtf8Printable(QString::number(fit.measured.upper_arm)));
    QVERIFY2(qAbs(fit.measured.lower_arm - wanted.lower_arm) < 0.5,
             qUtf8Printable(QString::number(fit.measured.lower_arm)));
    QVERIFY2(qAbs(fit.measured.crotch - wanted.crotch) < 0.5, qUtf8Printable(QString::number(fit.measured.crotch)));
    QVERIFY2(qAbs(fit.measured.knee_height - wanted.knee_height) < 0.5,
             qUtf8Printable(QString::number(fit.measured.knee_height)));
    QVERIFY2(qAbs(fit.measured.knee - wanted.knee) < 1.0, qUtf8Printable(QString::number(fit.measured.knee)));
    QVERIFY2(qAbs(fit.measured.calf - wanted.calf) < 1.0, qUtf8Printable(QString::number(fit.measured.calf)));
}

//---------------------------------------------------------------------------------------------------------------------
// Everyday measurements from a size chart have to be reached. Measuring the model against itself (see above) can't
// catch a girth taken in the wrong place, this can.
void TST_BodyModel::fitReachesTypicalBodies() const
{
    const BodyModel model;

    struct Case
    {
        qreal gender;
        BodyMeasurements wanted;
    };
    BodyMeasurements woman;
    woman.height = 168;
    woman.bust = 92;
    woman.waist = 74;
    woman.hip = 100;
    woman.upper_arm = 32;
    woman.lower_arm = 24;
    woman.crotch = 77;
    woman.knee_height = 46;
    woman.knee = 37;
    woman.calf = 35;
    BodyMeasurements man;
    man.height = 178;
    man.bust = 100;
    man.waist = 86;
    man.hip = 100;
    man.upper_arm = 34;
    man.lower_arm = 26;
    man.crotch = 82;
    man.knee_height = 50;
    man.knee = 39;
    man.calf = 38;

    for (const Case& body : {Case{0.0, woman}, Case{1.0, man}})
    {
        const BodyFit fit = BodyFitter(model).fit(body.wanted, body.gender, BodyShape::ageFromYears(35));
        const QString report = QStringLiteral("gender %1: height %2, bust %3, waist %4, hip %5, arm %6 + %7, "
                                              "crotch %8, knee %9 high, %10 round, calf %11")
                                   .arg(body.gender).arg(fit.measured.height).arg(fit.measured.bust)
                                   .arg(fit.measured.waist).arg(fit.measured.hip).arg(fit.measured.upper_arm)
                                   .arg(fit.measured.lower_arm).arg(fit.measured.crotch).arg(fit.measured.knee_height)
                                   .arg(fit.measured.knee).arg(fit.measured.calf);
        QVERIFY2(qAbs(fit.measured.height - body.wanted.height) < 0.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.bust - body.wanted.bust) < 1.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.waist - body.wanted.waist) < 1.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.hip - body.wanted.hip) < 1.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.upper_arm - body.wanted.upper_arm) < 1.0, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.lower_arm - body.wanted.lower_arm) < 1.0, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.crotch - body.wanted.crotch) < 1.0, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.knee_height - body.wanted.knee_height) < 1.0, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.knee - body.wanted.knee) < 1.0, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.calf - body.wanted.calf) < 1.0, qUtf8Printable(report));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A woman's size 38 has a bust of 88, a man's size 50 a chest of 100; each size is larger all round than the one
// before; a size not offered is the nearest one that is.
void TST_BodyModel::standardSizesFollowTheGrading() const
{
    const StandardSize woman = StandardSizes::of(false, 38);
    QCOMPARE(woman.measurements.height, 168.0);
    QCOMPARE(woman.measurements.bust, 88.0);
    QCOMPARE(woman.measurements.waist, 72.0);
    QCOMPARE(woman.measurements.hip, 96.0);
    const StandardSize man = StandardSizes::of(true, 50);
    QCOMPARE(man.measurements.bust, 100.0);
    QCOMPARE(man.measurements.waist, 88.0);
    QCOMPARE(man.measurements.hip, 104.0);

    for (const QVector<StandardSize>& sizes : {StandardSizes::women(), StandardSizes::men()})
    {
        QCOMPARE(sizes.size(), 8);
        for (int i = 1; i < sizes.size(); ++i)
        {
            QVERIFY(sizes.at(i).size == sizes.at(i - 1).size + 2);
            QVERIFY(sizes.at(i).measurements.bust > sizes.at(i - 1).measurements.bust);
            QVERIFY(sizes.at(i).measurements.waist > sizes.at(i - 1).measurements.waist);
            QVERIFY(sizes.at(i).measurements.hip > sizes.at(i - 1).measurements.hip);
            QVERIFY(sizes.at(i).measurements.height >= sizes.at(i - 1).measurements.height);
        }
    }
    QCOMPARE(StandardSizes::of(false, 47).size, 46);
    QCOMPARE(StandardSizes::of(true, 30).size, 44);
    QCOMPARE(StandardSizes::of(false, StandardSizes::defaultSize(false)).size, 38);
    QCOMPARE(StandardSizes::of(true, StandardSizes::defaultSize(true)).size, 50);
}

//---------------------------------------------------------------------------------------------------------------------
// The avatar can be fitted to the smallest, the middle and the largest sizes offered. A woman's waist only goes up to
// about 86 cm in the body model, short of the largest women's sizes; theirs is as full as it goes.
void TST_BodyModel::standardSizesAreReached() const
{
    const BodyModel model;
    for (const bool male : {false, true})
    {
        const QVector<StandardSize> sizes = male ? StandardSizes::men() : StandardSizes::women();
        for (const StandardSize& size : {sizes.first(), StandardSizes::of(male, StandardSizes::defaultSize(male)),
                                         sizes.last()})
        {
            const BodyMeasurements& wanted = size.measurements;
            const BodyFit fit = BodyFitter(model).fit(wanted, male ? 1.0 : 0.0, BodyShape::ageFromYears(25));
            const QString report = QStringLiteral("%1 size %2: height %3, bust %4, waist %5, hip %6")
                                       .arg(male ? QStringLiteral("man") : QStringLiteral("woman")).arg(size.size)
                                       .arg(fit.measured.height).arg(fit.measured.bust).arg(fit.measured.waist)
                                       .arg(fit.measured.hip);
            QVERIFY2(qAbs(fit.measured.height - wanted.height) < 0.5, qUtf8Printable(report));
            QVERIFY2(qAbs(fit.measured.bust - wanted.bust) < 1.5, qUtf8Printable(report));
            QVERIFY2(qAbs(fit.measured.waist - qMin(wanted.waist, male ? wanted.waist : 85.5)) < 1.5,
                     qUtf8Printable(report));
            QVERIFY2(qAbs(fit.measured.hip - wanted.hip) < 1.5, qUtf8Printable(report));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// An arm known only from the shoulder tip to the wrist is fitted as a whole, keeping the body's own proportions; with
// one of its parts known as well, the other part is what is left.
void TST_BodyModel::fitKeepsTheArmsProportions() const
{
    const BodyModel model;
    const BodyMeasurer measurer(model);
    const qreal gender = 0.0;
    const qreal age = BodyShape::ageFromYears(35);

    BodyMeasurements wanted;
    wanted.height = 168;
    wanted.arm = 58;
    BodyMeasurements height_only;
    height_only.height = 168;
    const BodyFit unfitted = BodyFitter(model).fit(height_only, gender, age);
    const BodyFit whole = BodyFitter(model).fit(wanted, gender, age);
    QVERIFY2(qAbs(whole.measured.arm - 58) < 0.5, qUtf8Printable(QString::number(whole.measured.arm)));
    const qreal before = unfitted.measured.upper_arm / unfitted.measured.arm;
    const qreal after = whole.measured.upper_arm / whole.measured.arm;
    QVERIFY2(qAbs(after - before) < 0.02, qUtf8Printable(QStringLiteral("%1 before, %2 after").arg(before).arg(after)));

    wanted.upper_arm = 33;
    const BodyFit with_upper_arm = BodyFitter(model).fit(wanted, gender, age);
    QVERIFY2(qAbs(with_upper_arm.measured.upper_arm - 33) < 0.5,
             qUtf8Printable(QString::number(with_upper_arm.measured.upper_arm)));
    QVERIFY2(qAbs(with_upper_arm.measured.lower_arm - 25) < 0.5,
             qUtf8Printable(QString::number(with_upper_arm.measured.lower_arm)));
}

//---------------------------------------------------------------------------------------------------------------------
// A leg known only by its inside length keeps the body's own proportions: the knee goes up or down with the crotch.
void TST_BodyModel::fitKeepsTheLegsProportions() const
{
    const BodyModel model;
    const qreal gender = 0.0;
    const qreal age = BodyShape::ageFromYears(35);

    BodyMeasurements height_only;
    height_only.height = 168;
    BodyMeasurements wanted = height_only;
    wanted.crotch = 74;
    const BodyFit unfitted = BodyFitter(model).fit(height_only, gender, age);
    const BodyFit fitted = BodyFitter(model).fit(wanted, gender, age);
    QVERIFY2(qAbs(fitted.measured.crotch - 74) < 0.5, qUtf8Printable(QString::number(fitted.measured.crotch)));
    const qreal before = unfitted.measured.knee_height / unfitted.measured.crotch;
    const qreal after = fitted.measured.knee_height / fitted.measured.crotch;
    QVERIFY2(qAbs(after - before) < 0.02, qUtf8Printable(QStringLiteral("%1 before, %2 after").arg(before).arg(after)));
}

//---------------------------------------------------------------------------------------------------------------------
// Measurements beyond what the model can do give the closest body, not a broken one.
void TST_BodyModel::fitStaysInRange() const
{
    const BodyModel model;

    BodyMeasurements wanted;
    wanted.height = 170;
    wanted.waist = 300;

    const BodyFit fit = BodyFitter(model).fit(wanted, 0.0, 0.5);

    QVERIFY(fit.shape.weight >= 0 && fit.shape.weight <= 1);
    QCOMPARE(fit.shape.measures.value(QStringLiteral("torso/measure-waist-circ")), 1.0);
    QVERIFY(fit.measured.waist < 300);
    QVERIFY(qAbs(fit.measured.height - 170) < 0.5);
}

//---------------------------------------------------------------------------------------------------------------------
// A click in front of the chest or behind the hips goes on the body, one on the front of a thigh on that leg.
void TST_BodyModel::wrapFindsBodyParts() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
    const QVector3D chest = model.joint(positions, QStringLiteral("spine-3"));
    const QVector3D knee = model.joint(positions, QStringLiteral("l-knee"));

    const PieceArrangement front = wrap.arrangementAt(QVector3D(pelvis.x(), chest.y(), pelvis.z() + 15));
    QVERIFY(front.part == BodyPart::Body);
    QVERIFY(qAbs(front.angle) < 1.0);
    QCOMPARE(front.height, static_cast<qreal>(chest.y()));

    const PieceArrangement back = wrap.arrangementAt(QVector3D(pelvis.x(), pelvis.y(), pelvis.z() - 15));
    QVERIFY(back.part == BodyPart::Body);
    QVERIFY(qAbs(qAbs(back.angle) - 180.0) < 1.0);

    const PieceArrangement thigh = wrap.arrangementAt(knee + QVector3D(0, 10, 7));
    QVERIFY(thigh.part == BodyPart::LeftLeg);
    QVERIFY(qAbs(thigh.angle) < 20.0);

    // On an arm below the armpit pieces go around the arm, its top at 90 degrees on the left arm; above the armpit
    // they go around the body.
    const QVector3D shoulder = model.joint(positions, QStringLiteral("l-shoulder"));
    const QVector3D elbow = model.joint(positions, QStringLiteral("l-elbow"));
    const QVector3D upper_arm = shoulder + (elbow - shoulder) * 0.6f;
    const QVector3D arm_top = QVector3D::crossProduct(QVector3D(0, 0, 1), elbow - shoulder).normalized();
    const PieceArrangement sleeve = wrap.arrangementAt(upper_arm + arm_top * 6);
    QVERIFY(sleeve.part == BodyPart::LeftArm);
    QVERIFY2(qAbs(sleeve.angle - 90.0) < 15.0, qUtf8Printable(QString::number(sleeve.angle)));
    QVERIFY2(qAbs(sleeve.height - upper_arm.y()) < 1.0, qUtf8Printable(QString::number(sleeve.height)));

    const QVector3D right_forearm = (model.joint(positions, QStringLiteral("r-elbow"))
                                     + model.joint(positions, QStringLiteral("r-hand"))) / 2.0f;
    QVERIFY(wrap.arrangementAt(right_forearm + QVector3D(0, 0, 5)).part == BodyPart::RightArm);
    QVERIFY(wrap.arrangementAt(shoulder + QVector3D(0, 8, 0)).part == BodyPart::Body);
    QVERIFY(wrap.arrangementAt(QVector3D(pelvis.x() + 10, chest.y() + 15, pelvis.z())).part == BodyPart::Body);

    for (const BodyPart part : {BodyPart::Body, BodyPart::LeftLeg, BodyPart::RightLeg, BodyPart::LeftArm,
                                BodyPart::RightArm})
    {
        QVERIFY(BodyWrap::partFromName(BodyWrap::partName(part)) == part);
    }

    // Mirrored across the body, the left knee lands on the right one.
    const QVector3D right_knee = model.joint(positions, QStringLiteral("r-knee"));
    QVERIFY((wrap.mirrored(knee) - right_knee).length() < 0.5f);
    QVERIFY((wrap.mirrored(wrap.mirrored(knee)) - knee).length() < 1e-4f);
}

//---------------------------------------------------------------------------------------------------------------------
// On the part a point is nearest to, a piece goes where it would go put at that point. On another part it goes as far
// around that part as the point is: beside the hip, on the outside of the left leg, at the point's height.
void TST_BodyModel::wrapPlacesOnAGivenPart() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
    const QVector3D chest = model.joint(positions, QStringLiteral("spine-3"));
    const QVector3D knee = model.joint(positions, QStringLiteral("l-knee"));
    const QVector3D shoulder = model.joint(positions, QStringLiteral("l-shoulder"));
    const QVector3D elbow = model.joint(positions, QStringLiteral("l-elbow"));

    for (const QVector3D& point : {QVector3D(pelvis.x(), chest.y(), pelvis.z() + 15), knee + QVector3D(0, 10, 7),
                                   shoulder + (elbow - shoulder) * 0.6f + QVector3D(0, 6, 0)})
    {
        const PieceArrangement at = wrap.arrangementAt(point);
        const PieceArrangement on = wrap.arrangementOn(at.part, point);
        QVERIFY(on.part == at.part);
        QVERIFY(qFuzzyCompare(1.0 + on.angle, 1.0 + at.angle));
        QVERIFY(qFuzzyCompare(1.0 + on.height, 1.0 + at.height));
    }

    const QVector3D beside_hip(pelvis.x() + 25, pelvis.y(), pelvis.z());
    QVERIFY(wrap.arrangementAt(beside_hip).part == BodyPart::Body);
    const PieceArrangement on_leg = wrap.arrangementOn(BodyPart::LeftLeg, beside_hip);
    QVERIFY(on_leg.part == BodyPart::LeftLeg);
    QVERIFY2(qAbs(on_leg.angle - 90.0) < 10.0, qUtf8Printable(QString::number(on_leg.angle)));
    QCOMPARE(on_leg.height, static_cast<qreal>(pelvis.y()));
}

//---------------------------------------------------------------------------------------------------------------------
// Pieces placed on the body or a leg start outside the skin, bent but not stretched.
void TST_BodyModel::wrappedPiecesStartOutsideTheBody() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const BodyCollider skin(positions.mid(0, model.skinVertexCount()), model.triangles());
    const GarmentMesh mesh = PieceMesher().meshPolygon({QPointF(0, 0), QPointF(24, 0), QPointF(24, 30),
                                                        QPointF(0, 30)});

    PieceArrangement on_hips;
    on_hips.height = model.joint(positions, QStringLiteral("pelvis")).y();
    PieceArrangement on_thigh;
    on_thigh.part = BodyPart::LeftLeg;
    on_thigh.angle = 180;
    on_thigh.height = model.joint(positions, QStringLiteral("l-knee")).y() + 15;

    for (const PieceArrangement& arrangement : {on_hips, on_thigh})
    {
        const QVector<QVector3D> placed = wrap.place(mesh, arrangement);
        QCOMPARE(placed.size(), mesh.vertexCount());

        // Close around the hips, not out where the hands hang beside them.
        if (arrangement.part == BodyPart::Body)
        {
            const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
            const QVector3D middle = placed.at(0);
            const qreal reach = qSqrt(qPow(middle.x() - pelvis.x(), 2) + qPow(middle.z() - pelvis.z(), 2));
            QVERIFY2(reach < 26.0, qUtf8Printable(QStringLiteral("the hip piece starts %1 cm out").arg(reach)));
        }

        for (const QVector3D& point : placed)
        {
            BodyContact contact;
            if (skin.closest(point, skin.trianglesWithin(point, 30), &contact))
            {
                QVERIFY2(contact.distance > 0, qUtf8Printable(QStringLiteral("(%1, %2, %3) is %4 cm inside")
                    .arg(point.x()).arg(point.y()).arg(point.z()).arg(-contact.distance)));
            }
        }

        qreal worst_strain = 0;
        for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            const int a = static_cast<int>(mesh.indices.at(t));
            const int b = static_cast<int>(mesh.indices.at(t + 1));
            const qreal rest = QLineF(mesh.rest_positions.at(a), mesh.rest_positions.at(b)).length();
            worst_strain = qMax(worst_strain, qAbs((placed.at(a) - placed.at(b)).length() / rest - 1.0));
        }
        QVERIFY2(worst_strain < 0.01, qUtf8Printable(QStringLiteral("%1: an edge is %2 % off its length")
            .arg(BodyWrap::partName(arrangement.part)).arg(worst_strain * 100)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// A sleeve placed on an arm starts outside the skin and close around the arm, its cap on top of the arm and no higher
// up it than the shoulder tip, its cuff around the wrist when it is as long as the arm. It is stretched most around
// the elbow, where the arm bends. On the other arm a mirrored sleeve is placed as the mirror image.
void TST_BodyModel::sleevesStartAroundTheArm() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const BodyCollider skin(positions.mid(0, model.skinVertexCount()), model.triangles());
    const QVector3D shoulder = model.joint(positions, QStringLiteral("l-shoulder"));
    const QVector3D elbow = model.joint(positions, QStringLiteral("l-elbow"));
    const QVector3D wrist = model.joint(positions, QStringLiteral("l-hand"));

    // A long sleeve, its cap's top in the middle of its top edge, as long as the arm from the shoulder tip to the
    // wrist.
    const BodyMeasurer measurer(model);
    const QVector3D shoulder_tip = measurer.shoulderTip(positions);
    const qreal length = measurer.arm(positions);
    const QPointF cap_top(16, 0);
    const QPointF front_underarm(0, 13);
    const QPointF back_underarm(32, 13);
    const GarmentMesh sleeve = PieceMesher().meshPolygon({cap_top, QPointF(22, 1.5), QPointF(28, 5.5), back_underarm,
                                                         QPointF(27, length), QPointF(5, length), front_underarm,
                                                         QPointF(4, 5.5), QPointF(10, 1.5)});
    auto vertex = [&sleeve](const QPointF& rest)
    {
        int found = -1;
        for (int i = 0; i < sleeve.vertexCount(); ++i)
        {
            found = QLineF(sleeve.rest_positions.at(i), rest).length() < 1e-6 ? i : found;
        }
        return found;
    };

    PieceArrangement on_arm;
    on_arm.part = BodyPart::LeftArm;
    on_arm.angle = 90;
    on_arm.height = shoulder.y();  // as high up the arm as it goes
    const QVector<QVector3D> placed = wrap.place(sleeve, on_arm);
    QCOMPARE(placed.size(), sleeve.vertexCount());

    const QVector3D upper_arm = (elbow - shoulder).normalized();
    const QVector3D forearm = (wrist - elbow).normalized();
    for (const QVector3D& point : placed)
    {
        BodyContact contact;
        if (skin.closest(point, skin.trianglesWithin(point, 30), &contact))
        {
            QVERIFY2(contact.distance > 0, qUtf8Printable(QStringLiteral("(%1, %2, %3) is %4 cm inside")
                .arg(point.x()).arg(point.y()).arg(point.z()).arg(-contact.distance)));
        }
        const float from_bones = qMin((point - shoulder - upper_arm * QVector3D::dotProduct(point - shoulder,
                                                                                           upper_arm)).length(),
                                      (point - elbow - forearm * QVector3D::dotProduct(point - elbow,
                                                                                      forearm)).length());
        QVERIFY2(from_bones < 11.0f, qUtf8Printable(QStringLiteral("(%1, %2, %3) is %4 cm from the arm")
            .arg(point.x()).arg(point.y()).arg(point.z()).arg(from_bones)));
    }

    // The cap's top on top of the arm, level with the shoulder tip along the arm; the underarm in front and behind.
    const QVector3D top = placed.at(vertex(cap_top));
    QVERIFY(top.y() > shoulder.y() && top.x() > shoulder.x());
    QVERIFY2(qAbs(QVector3D::dotProduct(top - shoulder_tip, upper_arm)) < 0.5f,
             qUtf8Printable(QString::number(QVector3D::dotProduct(top - shoulder_tip, upper_arm))));
    QVERIFY(placed.at(vertex(front_underarm)).z() > shoulder.z() + 3);
    QVERIFY(placed.at(vertex(back_underarm)).z() < shoulder.z() - 3);

    // The sleeve narrows to the wrist, and so does the tube it starts on: the sides of the cuff meet under the wrist,
    // not across it.
    const QVector3D cuff_front = placed.at(vertex(QPointF(5, length)));
    const QVector3D cuff_back = placed.at(vertex(QPointF(27, length)));
    const QVector3D cuff_middle = (cuff_front + cuff_back) / 2.0f;
    QVERIFY2(qAbs(QVector3D::dotProduct(cuff_middle - wrist, forearm)) < 1.5f,
             qUtf8Printable(QStringLiteral("the cuff is %1 cm past the wrist")
                 .arg(QVector3D::dotProduct(cuff_middle - wrist, forearm))));
    const QVector3D below_forearm = cuff_middle - elbow - forearm * QVector3D::dotProduct(cuff_middle - elbow, forearm);
    QVERIFY2(below_forearm.length() > 1.5f && below_forearm.y() < -1.0f,
             qUtf8Printable(QStringLiteral("(%1, %2, %3) from the forearm").arg(below_forearm.x())
                 .arg(below_forearm.y()).arg(below_forearm.z())));

    // Hardly stretched above the elbow, where the tube only narrows slowly; most around the elbow, which the tube
    // bends around from 10 cm above to 10 cm below.
    const qreal elbow_along = (elbow - shoulder).length();
    qreal worst_strain = 0;
    for (int t = 0; t + 2 < sleeve.indices.size(); t += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const int a = static_cast<int>(sleeve.indices.at(t + k));
            const int b = static_cast<int>(sleeve.indices.at(t + (k + 1) % 3));
            const qreal rest = QLineF(sleeve.rest_positions.at(a), sleeve.rest_positions.at(b)).length();
            const qreal strain = qAbs((placed.at(a) - placed.at(b)).length() / rest - 1.0);
            const bool above_elbow = qMax(sleeve.rest_positions.at(a).y(), sleeve.rest_positions.at(b).y())
                                     < elbow_along - 12;
            QVERIFY2(!above_elbow || strain < 0.15,
                     qUtf8Printable(QStringLiteral("an edge above the elbow is %1 % off").arg(strain * 100)));
            worst_strain = qMax(worst_strain, strain);
        }
    }
    QVERIFY2(worst_strain < 0.4, qUtf8Printable(QStringLiteral("an edge is %1 % off").arg(worst_strain * 100)));

    // Seen from the other side of the body, a sleeve cut the other way round on the right arm is the same.
    PieceArrangement on_right_arm = on_arm;
    on_right_arm.part = BodyPart::RightArm;
    on_right_arm.angle = -90;
    const QVector<QVector3D> mirrored = wrap.place(sleeve.mirrored(), on_right_arm);
    QCOMPARE(mirrored.size(), placed.size());
    for (int i = 0; i < placed.size(); ++i)
    {
        QVERIFY2((wrap.mirrored(placed.at(i)) - mirrored.at(i)).length() < 0.05f, qUtf8Printable(QString::number(i)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The arrangement points go around the body at the neck, bust, waist, hip and thighs, and around the legs and the
// arms, each where a tape around the part would lie, facing out. A piece put at one goes where it is, whichever way
// round the piece is.
void TST_BodyModel::arrangementPointsSitOnTheBody() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const BodyCollider skin(positions.mid(0, model.skinVertexCount()), model.triangles());
    const QVector<ArrangementPoint>& points = wrap.points();
    QCOMPARE(points.size(), 5 * 8 + 2 * 3 * 4 + 2 * 3 * 4);

    QSet<QString> names;
    for (int i = 0; i < points.size(); ++i)
    {
        const ArrangementPoint& point = points.at(i);
        names.insert(point.name);
        QCOMPARE(wrap.pointNamed(point.name), i);
        QCOMPARE(point.arrangement.point, point.name);
        QCOMPARE(point.name, BodyWrap::partName(point.arrangement.part) + QLatin1Char('-') + point.level
                                 + QLatin1Char('-') + point.side);
        QVERIFY(qAbs(point.normal.length() - 1.0f) < 1e-4f);

        // On the skin, or over a hollow the tape bridges, never inside the body.
        BodyContact contact;
        QVERIFY(skin.closest(point.position, skin.trianglesWithin(point.position, 30), &contact));
        const float farthest = point.arrangement.part == BodyPart::Body ? 8.0f : 3.0f;
        QVERIFY2(contact.distance > -0.3f && contact.distance < farthest,
                 qUtf8Printable(QStringLiteral("%1 is %2 cm off the skin").arg(point.name).arg(contact.distance)));

        PieceArrangement put;
        put.point = point.name;
        put.rotation = 90;
        put.turned_over = true;
        const PieceArrangement there = wrap.resolved(put);
        QVERIFY(there.part == point.arrangement.part);
        QCOMPARE(there.angle, point.arrangement.angle);
        QCOMPARE(there.height, point.arrangement.height);
        QCOMPARE(there.rotation, 90.0);
        QVERIFY(there.turned_over);
    }
    QCOMPARE(names.size(), points.size());
    QCOMPARE(wrap.pointNamed(QStringLiteral("body-knee-front")), -1);
    QCOMPARE(wrap.pointNamed(QString()), -1);

    auto named = [&wrap](const QString& name)
    {
        const int index = wrap.pointNamed(name);
        return index >= 0 ? wrap.points().at(index) : ArrangementPoint();
    };

    // From the neck down, the waist where it is measured.
    const QStringList body_levels = {QStringLiteral("neck"), QStringLiteral("bust"), QStringLiteral("waist"),
                                     QStringLiteral("hip"), QStringLiteral("thigh")};
    for (int i = 1; i < body_levels.size(); ++i)
    {
        QVERIFY2(named(QStringLiteral("body-%1-front").arg(body_levels.at(i - 1))).arrangement.height
                     > named(QStringLiteral("body-%1-front").arg(body_levels.at(i))).arrangement.height,
                 qUtf8Printable(body_levels.at(i)));
    }
    QCOMPARE(named(QStringLiteral("body-waist-back")).arrangement.height, BodyMeasurer(model).waistLevel(positions));
    for (const QString& part : {QStringLiteral("leftLeg"), QStringLiteral("rightLeg")})
    {
        QVERIFY(named(part + QStringLiteral("-thigh-front")).arrangement.height
                > named(part + QStringLiteral("-knee-front")).arrangement.height);
        QVERIFY(named(part + QStringLiteral("-knee-front")).arrangement.height
                > named(part + QStringLiteral("-calf-front")).arrangement.height);
    }
    for (const QString& part : {QStringLiteral("leftArm"), QStringLiteral("rightArm")})
    {
        QVERIFY(named(part + QStringLiteral("-upperArm-front")).arrangement.height
                > named(part + QStringLiteral("-elbow-front")).arrangement.height);
        QVERIFY(named(part + QStringLiteral("-elbow-front")).arrangement.height
                > named(part + QStringLiteral("-wrist-front")).arrangement.height);
    }

    // Facing the way they are named: the front forwards, the avatar's left towards its left leg, the outside of a leg
    // away from the other leg, the outside of an arm up, where a sleeve's cap goes.
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
    const QVector3D left_hip = model.joint(positions, QStringLiteral("l-upper-leg"));
    const float left = left_hip.x() > pelvis.x() ? 1.0f : -1.0f;
    QVERIFY(named(QStringLiteral("body-bust-front")).normal.z() > 0.99f);
    QVERIFY(named(QStringLiteral("body-hip-back")).normal.z() < -0.99f);
    QVERIFY(named(QStringLiteral("body-waist-left")).normal.x() * left > 0.99f);
    QVERIFY(named(QStringLiteral("body-waist-right")).position.x() * left < pelvis.x() * left - 10);
    QVERIFY(named(QStringLiteral("leftLeg-knee-outside")).normal.x() * left > 0.99f);
    QVERIFY(named(QStringLiteral("rightLeg-knee-outside")).normal.x() * left < -0.99f);
    QVERIFY(named(QStringLiteral("leftLeg-knee-front")).position.x() * left > pelvis.x() * left);
    QVERIFY(named(QStringLiteral("leftArm-upperArm-outside")).normal.y() > 0.5f);
    QVERIFY(named(QStringLiteral("rightArm-elbow-outside")).normal.y() > 0.5f);
    QVERIFY(named(QStringLiteral("leftArm-elbow-inside")).normal.y() < -0.5f);
    QVERIFY2(qAbs(named(QStringLiteral("leftArm-upperArm-outside")).arrangement.angle - 90.0) < 15.0,
             qUtf8Printable(QString::number(named(QStringLiteral("leftArm-upperArm-outside")).arrangement.angle)));
    QVERIFY2(qAbs(named(QStringLiteral("rightArm-upperArm-outside")).arrangement.angle + 90.0) < 15.0,
             qUtf8Printable(QString::number(named(QStringLiteral("rightArm-upperArm-outside")).arrangement.angle)));
}

//---------------------------------------------------------------------------------------------------------------------
// On a taller avatar the waist is higher up: a piece put at the waist goes to its waist, a piece put anywhere else
// stays as high as it was put.
void TST_BodyModel::arrangementPointsFollowTheAvatar() const
{
    const BodyModel model;
    BodyShape tall_shape = female();
    tall_shape.scale = 1.1;
    const BodyWrap wrap(model, model.evaluate(female()));
    const BodyWrap tall(model, model.evaluate(tall_shape));

    const ArrangementPoint waist = wrap.points().at(wrap.pointNamed(QStringLiteral("body-waist-front")));
    const ArrangementPoint tall_waist = tall.points().at(tall.pointNamed(QStringLiteral("body-waist-front")));
    QVERIFY2(tall_waist.arrangement.height > waist.arrangement.height + 5,
             qUtf8Printable(QStringLiteral("%1 and %2").arg(waist.arrangement.height)
                                .arg(tall_waist.arrangement.height)));

    const PieceArrangement at_waist = waist.arrangement;
    QCOMPARE(tall.resolved(at_waist).height, tall_waist.arrangement.height);

    PieceArrangement anywhere = at_waist;
    anywhere.point.clear();
    QCOMPARE(tall.resolved(anywhere).height, waist.arrangement.height);

    PieceArrangement unknown = at_waist;
    unknown.point = QStringLiteral("body-shin-front");
    QCOMPARE(tall.resolved(unknown).height, waist.arrangement.height);
}

//---------------------------------------------------------------------------------------------------------------------
// A piece rotated goes on turned clockwise as seen from outside, a piece turned over as its mirror image; either way it
// is bent around the body as it is, not stretched.
void TST_BodyModel::piecesTurnAndTurnOver() const
{
    const BodyModel model;
    const QVector<QVector3D> positions = model.evaluate(female());
    const BodyWrap wrap(model, positions);
    const QPointF top_left(0, 0);
    const QPointF top_right(20, 0);
    const QPointF bottom_left(0, 30);
    const GarmentMesh mesh = PieceMesher().meshPolygon({top_left, top_right, QPointF(20, 30), bottom_left});
    auto vertex = [&mesh](const QPointF& rest)
    {
        int found = -1;
        for (int i = 0; i < mesh.vertexCount(); ++i)
        {
            found = QLineF(mesh.rest_positions.at(i), rest).length() < 1e-6 ? i : found;
        }
        return found;
    };

    // In front of the waist, where +x is on the right as seen from the front.
    const PieceArrangement front = wrap.points().at(wrap.pointNamed(QStringLiteral("body-waist-front"))).arrangement;
    auto placed = [&wrap, &mesh, &front](qreal rotation, bool turned_over)
    {
        PieceArrangement arrangement = front;
        arrangement.rotation = rotation;
        arrangement.turned_over = turned_over;
        return wrap.place(mesh, arrangement);
    };
    auto tall = [](const QVector<QVector3D>& points)
    {
        float lowest = std::numeric_limits<float>::infinity();
        float highest = -std::numeric_limits<float>::infinity();
        for (const QVector3D& point : points)
        {
            lowest = qMin(lowest, point.y());
            highest = qMax(highest, point.y());
        }
        return highest - lowest;
    };

    const QVector<QVector3D> as_drafted = placed(0, false);
    QVERIFY(as_drafted.at(vertex(top_left)).x() < as_drafted.at(vertex(top_right)).x());
    QVERIFY(as_drafted.at(vertex(top_left)).y() > as_drafted.at(vertex(bottom_left)).y() + 29);
    QVERIFY(qAbs(tall(as_drafted) - 30.0f) < 0.01f);

    // A quarter turn clockwise takes the top left corner to the top right, and the piece lies on its side.
    const QVector<QVector3D> rotated = placed(90, false);
    QVERIFY(rotated.at(vertex(top_left)).x() > rotated.at(vertex(bottom_left)).x());
    QVERIFY(rotated.at(vertex(top_left)).y() > rotated.at(vertex(top_right)).y() + 19);
    QVERIFY(qAbs(tall(rotated) - 20.0f) < 0.01f);

    // Turned over, its left side is on the right.
    const QVector<QVector3D> turned_over = placed(0, true);
    QVERIFY(turned_over.at(vertex(top_left)).x() > turned_over.at(vertex(top_right)).x());
    QVERIFY(turned_over.at(vertex(top_left)).y() > turned_over.at(vertex(bottom_left)).y() + 29);

    // Turned over and half way round, it is upside down.
    const QVector<QVector3D> upside_down = placed(180, true);
    QVERIFY(upside_down.at(vertex(top_left)).x() < upside_down.at(vertex(top_right)).x());
    QVERIFY(upside_down.at(vertex(top_left)).y() < upside_down.at(vertex(bottom_left)).y() - 29);

    for (const QVector<QVector3D>& placement : {as_drafted, rotated, turned_over, upside_down})
    {
        qreal worst_strain = 0;
        for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            for (int k = 0; k < 3; ++k)
            {
                const int a = static_cast<int>(mesh.indices.at(t + k));
                const int b = static_cast<int>(mesh.indices.at(t + (k + 1) % 3));
                const qreal rest = QLineF(mesh.rest_positions.at(a), mesh.rest_positions.at(b)).length();
                worst_strain = qMax(worst_strain, qAbs((placement.at(a) - placement.at(b)).length() / rest - 1.0));
            }
        }
        QVERIFY2(worst_strain < 0.01, qUtf8Printable(QStringLiteral("an edge is %1 % off").arg(worst_strain * 100)));
    }

    // Put further out, as a preview, it keeps its shape that much further off the body.
    const QVector3D pelvis = model.joint(positions, QStringLiteral("pelvis"));
    const QVector<QVector3D> further = wrap.place(mesh, front, 1.0);
    for (int i = 0; i < further.size(); ++i)
    {
        const qreal reach = qSqrt(qPow(as_drafted.at(i).x() - pelvis.x(), 2)
                                  + qPow(as_drafted.at(i).z() - pelvis.z(), 2));
        const qreal further_reach = qSqrt(qPow(further.at(i).x() - pelvis.x(), 2)
                                          + qPow(further.at(i).z() - pelvis.z(), 2));
        QVERIFY2(qAbs(further_reach - reach - 1.0) < 1e-3, qUtf8Printable(QString::number(further_reach - reach)));
        QVERIFY(qAbs(further.at(i).y() - as_drafted.at(i).y()) < 1e-4f);
    }

    // On an arm too: a piece rotated half way round has its top towards the hand.
    PieceArrangement on_arm = wrap.points().at(wrap.pointNamed(QStringLiteral("leftArm-elbow-outside"))).arrangement;
    const QVector<QVector3D> sleeve = wrap.place(mesh, on_arm);
    on_arm.rotation = 180;
    const QVector<QVector3D> sleeve_turned = wrap.place(mesh, on_arm);
    QVERIFY(sleeve.at(vertex(top_left)).y() > sleeve.at(vertex(bottom_left)).y());
    QVERIFY(sleeve_turned.at(vertex(top_left)).y() < sleeve_turned.at(vertex(bottom_left)).y());
}
