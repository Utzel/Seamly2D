//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_limbline.cpp
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

#include "tst_limbline.h"

#include <QtMath>
#include <QtTest>

#include "../vgarment/limb_line.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
bool near(const QVector3D& a, const QVector3D& b, float tolerance = 1e-3f)
{
    return (a - b).length() < tolerance;
}

//---------------------------------------------------------------------------------------------------------------------
QString text(const QVector3D& vector)
{
    return QStringLiteral("(%1, %2, %3)").arg(vector.x()).arg(vector.y()).arg(vector.z());
}

//---------------------------------------------------------------------------------------------------------------------
// An upper arm hanging straight down, then a forearm bent 45 degrees forwards at the elbow.
QVector<QVector3D> bentArm()
{
    return {QVector3D(0, 0, 0), QVector3D(0, -20, 0), QVector3D(0, -35, 15)};
}

//---------------------------------------------------------------------------------------------------------------------
// A left arm held out and down as the avatar holds it, and the same arm mirrored to the right.
QVector<QVector3D> heldOutArm(float side)
{
    return {QVector3D(side * 16, 133, 0), QVector3D(side * 33, 115, 0), QVector3D(side * 44, 102, 14)};
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_LimbLine::TST_LimbLine(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// A limb with a single bone is a straight line through it, going on beyond its ends.
void TST_LimbLine::straightLimbFollowsItsBone() const
{
    const LimbLine line({QVector3D(0, 100, 0), QVector3D(0, 60, 0)}, 5, 10, 10);

    QVERIFY(!line.isEmpty());
    QCOMPARE(line.length(), 40.0);
    QVERIFY(near(line.pointAt(0), QVector3D(0, 100, 0)));
    QVERIFY(near(line.pointAt(40), QVector3D(0, 60, 0)));
    QVERIFY(near(line.pointAt(-10), QVector3D(0, 110, 0)));
    QVERIFY(near(line.pointAt(-15), QVector3D(0, 115, 0)));
    QVERIFY(near(line.pointAt(65), QVector3D(0, 35, 0)));
    QVERIFY(near(line.directionAt(20), QVector3D(0, -1, 0)));

    qreal distance = 0;
    QVERIFY(qAbs(line.alongNearest(QVector3D(5, 80, 0), &distance) - 20.0) < 1e-3);
    QVERIFY(qAbs(distance - 5.0) < 1e-3);
}

//---------------------------------------------------------------------------------------------------------------------
// At the elbow the line bends in an arc, smoothly and without stretching, and away from it runs along the bones.
void TST_LimbLine::bendIsAnArc() const
{
    const QVector<QVector3D> joints = bentArm();
    const qreal bend = 5;
    const LimbLine line(joints, bend, 10, 10);
    const qreal upper = 20;
    const QVector3D forearm = (joints.at(2) - joints.at(1)).normalized();

    QVERIFY(qAbs(line.length() - (upper + (joints.at(2) - joints.at(1)).length())) < 1e-4);
    QVERIFY(near(line.pointAt(upper - bend), QVector3D(0, -15, 0)));
    QVERIFY(near(line.directionAt(upper + bend + 1), forearm));
    QVERIFY2((line.pointAt(line.length()) - joints.at(2)).length() < 1.0,
             qUtf8Printable(text(line.pointAt(line.length()))));
    QVERIFY((line.pointAt(upper) - joints.at(1)).length() < 1.0);

    // The direction turns a little at a time, all the way round the arc, and a cm along is a cm away.
    const qreal turn_per_cm = qDegreesToRadians(45.0) / (2.0 * bend);
    for (qreal along = -10; along < line.length() + 10; along += 0.25)
    {
        const qreal turn = qAcos(qBound(-1.0f, QVector3D::dotProduct(line.directionAt(along),
                                                                       line.directionAt(along + 0.25)), 1.0f));
        QVERIFY2(turn < turn_per_cm * 0.25 * 1.5 + 1e-3, qUtf8Printable(QStringLiteral("at %1").arg(along)));
        QVERIFY(qAbs((line.pointAt(along + 1) - line.pointAt(along)).length() - 1.0) < 0.01);
    }

    // An arc never reaches past the middle of a bone.
    const LimbLine wide(joints, 100, 0, 0);
    QVERIFY(near(wide.pointAt(9), QVector3D(0, -9, 0)));
}

//---------------------------------------------------------------------------------------------------------------------
// The front stays square to the line and turns with it at the elbow; a mirrored limb has a mirrored front.
void TST_LimbLine::frontTurnsWithTheLine() const
{
    const LimbLine line(bentArm(), 5, 10, 10);
    for (qreal along = -10; along < line.length() + 10; along += 0.5)
    {
        QVERIFY(qAbs(line.frontAt(along).length() - 1.0f) < 1e-4f);
        QVERIFY(qAbs(QVector3D::dotProduct(line.frontAt(along), line.directionAt(along))) < 1e-3f);
    }
    QVERIFY(near(line.frontAt(5), QVector3D(0, 0, 1)));
    QVERIFY2(near(line.frontAt(35), QVector3D(0, 1, 1).normalized(), 1e-2f), qUtf8Printable(text(line.frontAt(35))));

    const LimbLine left(heldOutArm(1), 5, 10, 10);
    const LimbLine right(heldOutArm(-1), 5, 10, 10);
    for (qreal along : {-5.0, 10.0, 24.0, 40.0, 55.0})
    {
        const QVector3D mirrored = right.frontAt(along) * QVector3D(-1, 1, 1);
        QVERIFY2(near(left.frontAt(along), mirrored), qUtf8Printable(QStringLiteral("at %1").arg(along)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Around a limb hanging straight down the angles are where they are around the body: 0 in front, 90 towards +x.
void TST_LimbLine::anglesGoRoundLikeTheBody() const
{
    const LimbLine line({QVector3D(0, 100, 0), QVector3D(0, 60, 0)}, 5, 0, 0);
    QVERIFY(near(line.aroundAt(20, 0), QVector3D(0, 0, 1)));
    QVERIFY(near(line.aroundAt(20, 90), QVector3D(1, 0, 0)));
    QVERIFY(near(line.aroundAt(20, 180), QVector3D(0, 0, -1)));
    QVERIFY(near(line.aroundAt(20, -90), QVector3D(-1, 0, 0)));
}

//---------------------------------------------------------------------------------------------------------------------
// On a limb pointing downwards, each height is at one place along it, beyond its ends too.
void TST_LimbLine::heightFindsThePlaceAlong() const
{
    const LimbLine line(heldOutArm(1), 8, 10, 25);
    for (qreal height : {145.0, 133.0, 120.0, 108.0, 102.0, 90.0, 60.0})
    {
        const qreal along = line.alongAtHeight(height);
        QVERIFY2(qAbs(line.pointAt(along).y() - height) < 0.01,
                 qUtf8Printable(QStringLiteral("%1 cm high is at %2").arg(height).arg(line.pointAt(along).y())));
    }
    QVERIFY(line.alongAtHeight(133) < 0.01);
    QVERIFY(line.alongAtHeight(120) < line.alongAtHeight(108));
}

//---------------------------------------------------------------------------------------------------------------------
// Without a bone there is no line, and asking about it does no harm.
void TST_LimbLine::noJointsNoLine() const
{
    QVERIFY(LimbLine().isEmpty());
    QVERIFY(LimbLine({QVector3D(1, 2, 3)}, 5, 10, 10).isEmpty());
    QVERIFY(LimbLine({QVector3D(1, 2, 3), QVector3D(1, 2, 3)}, 5, 10, 10).isEmpty());

    const LimbLine empty;
    QCOMPARE(empty.length(), 0.0);
    QVERIFY(empty.pointAt(5).isNull());
    QVERIFY(empty.frontAt(5).isNull());
    QCOMPARE(empty.alongNearest(QVector3D(1, 1, 1)), 0.0);
    QCOMPARE(empty.alongAtHeight(50), 0.0);
}
