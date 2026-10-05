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
    const BodyMeasurements wanted = measurer.measure(model.evaluate(known));

    const BodyFit fit = BodyFitter(model).fit(wanted, known.gender, known.age);

    QVERIFY2(qAbs(fit.measured.height - wanted.height) < 0.5, qUtf8Printable(QString::number(fit.measured.height)));
    QVERIFY2(qAbs(fit.measured.bust - wanted.bust) < 1.0, qUtf8Printable(QString::number(fit.measured.bust)));
    QVERIFY2(qAbs(fit.measured.waist - wanted.waist) < 1.0, qUtf8Printable(QString::number(fit.measured.waist)));
    QVERIFY2(qAbs(fit.measured.hip - wanted.hip) < 1.0, qUtf8Printable(QString::number(fit.measured.hip)));
    QVERIFY2(qAbs(fit.measured.neck - wanted.neck) < 1.0, qUtf8Printable(QString::number(fit.measured.neck)));
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
    BodyMeasurements man;
    man.height = 178;
    man.bust = 100;
    man.waist = 86;
    man.hip = 100;

    for (const Case& body : {Case{0.0, woman}, Case{1.0, man}})
    {
        const BodyFit fit = BodyFitter(model).fit(body.wanted, body.gender, BodyShape::ageFromYears(35));
        const QString report = QStringLiteral("gender %1: height %2, bust %3, waist %4, hip %5")
                                   .arg(body.gender).arg(fit.measured.height).arg(fit.measured.bust)
                                   .arg(fit.measured.waist).arg(fit.measured.hip);
        QVERIFY2(qAbs(fit.measured.height - body.wanted.height) < 0.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.bust - body.wanted.bust) < 1.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.waist - body.wanted.waist) < 1.5, qUtf8Printable(report));
        QVERIFY2(qAbs(fit.measured.hip - body.wanted.hip) < 1.5, qUtf8Printable(report));
    }
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

    for (const BodyPart part : {BodyPart::Body, BodyPart::LeftLeg, BodyPart::RightLeg})
    {
        QVERIFY(BodyWrap::partFromName(BodyWrap::partName(part)) == part);
    }
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
