//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_garmentfit.cpp
//  @author Julius
//  @date   7 Oct, 2026
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

#include "tst_garmentfit.h"

#include <QtTest>

#include "../vgarment/garment_fit.h"
#include "../vgarment/piece_mesher.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// A floor of body 1 m square at y = 0, facing up.
BodyCollider floorBody()
{
    const QVector<QVector3D> positions = {QVector3D(-50, 0, -50), QVector3D(-50, 0, 50), QVector3D(50, 0, 50),
                                          QVector3D(50, 0, -50)};
    return BodyCollider(positions, {0, 1, 2, 0, 2, 3});
}

//---------------------------------------------------------------------------------------------------------------------
// A piece 20 by 20 cm lying flat over the floor at the given height.
QVector<QVector3D> lyingAt(const GarmentMesh& mesh, float height)
{
    QVector<QVector3D> positions;
    for (const QPointF& rest : mesh.rest_positions)
    {
        positions.append(QVector3D(static_cast<float>(rest.x()) - 10.0f, height, static_cast<float>(rest.y()) - 10.0f));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
GarmentMesh square()
{
    return PieceMesher().meshPolygon({QPointF(0, 0), QPointF(20, 0), QPointF(20, 20), QPointF(0, 20)});
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_GarmentFit::TST_GarmentFit(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// Resting on the body, its thickness off the skin, the cloth has no ease; 5 cm higher, 5 cm; far off, the most ease
// measured.
void TST_GarmentFit::easeIsTheGapOffTheBody() const
{
    const BodyCollider body = floorBody();
    const GarmentMesh mesh = square();
    const qreal thickness = ClothSettings().thickness;

    for (const qreal ease : GarmentFit::ease(body, lyingAt(mesh, static_cast<float>(thickness))))
    {
        QVERIFY(qAbs(ease) < 1e-5);
    }
    for (const qreal ease : GarmentFit::ease(body, lyingAt(mesh, static_cast<float>(thickness + 5.0))))
    {
        QVERIFY(qAbs(ease - 5.0) < 1e-4);
    }
    for (const qreal ease : GarmentFit::ease(body, lyingAt(mesh, 40.0f)))
    {
        QCOMPARE(ease, GarmentFit::farthestEase());
    }
    for (const qreal ease : GarmentFit::ease(body, lyingAt(mesh, -1.0f)))
    {
        QCOMPARE(ease, 0.0);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Pressed 0.01 cm into its thickness, each vertex is pushed back with the contact's stiffness times that; the pressure
// is that push back over the cloth around the vertex and its neighbours.
void TST_GarmentFit::pressureIsThePushBackOverTheCloth() const
{
    const BodyCollider body = floorBody();
    const GarmentMesh mesh = square();
    const ClothSettings settings;
    const qreal depth = 0.01;
    const QVector<QVector3D> positions = lyingAt(mesh, static_cast<float>(settings.thickness - depth));

    const QVector<qreal> pressure = GarmentFit::pressure(mesh, body, positions);
    QCOMPARE(pressure.size(), mesh.vertexCount());
    for (const qreal kilopascals : pressure)
    {
        QVERIFY(kilopascals > 0);
    }

    // In the middle, where the cloth is alike all round, that is one vertex's push back over its share of the cloth, a
    // third of each triangle it is a corner of.
    int middle = 0;
    for (int i = 1; i < mesh.vertexCount(); ++i)
    {
        const QPointF offset = mesh.rest_positions.at(i) - QPointF(10, 10);
        const QPointF best = mesh.rest_positions.at(middle) - QPointF(10, 10);
        middle = QPointF::dotProduct(offset, offset) < QPointF::dotProduct(best, best) ? i : middle;
    }
    qreal share = 0;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const int a = static_cast<int>(mesh.indices.at(i));
        const int b = static_cast<int>(mesh.indices.at(i + 1));
        const int c = static_cast<int>(mesh.indices.at(i + 2));
        if (a == middle || b == middle || c == middle)
        {
            share += QVector3D::crossProduct(positions.at(b) - positions.at(a),
                                             positions.at(c) - positions.at(a)).length() / 6.0;
        }
    }
    const qreal expected = settings.contact_stiffness * depth / share * 1.0e-4;
    QVERIFY2(qAbs(pressure.at(middle) - expected) / expected < 0.05,
             qPrintable(QStringLiteral("%1 kPa against %2").arg(pressure.at(middle)).arg(expected)));

    // Off the body, none.
    for (const qreal kilopascals : GarmentFit::pressure(mesh, body, lyingAt(mesh, 1.0f)))
    {
        QCOMPARE(kilopascals, 0.0);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentFit::noBodyNoPressure() const
{
    const GarmentMesh mesh = square();
    for (const qreal kilopascals : GarmentFit::pressure(mesh, BodyCollider(), lyingAt(mesh, 0.0f)))
    {
        QCOMPARE(kilopascals, 0.0);
    }
    QVERIFY(GarmentFit::pressure(mesh, floorBody(), QVector<QVector3D>()).size() == mesh.vertexCount());
    for (const qreal ease : GarmentFit::ease(BodyCollider(), lyingAt(mesh, 0.0f)))
    {
        QCOMPARE(ease, GarmentFit::farthestEase());
    }
}
