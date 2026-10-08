//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_garmentmesh.cpp
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

#include "tst_garmentmesh.h"

#include <QMatrix4x4>
#include <QPair>
#include <QtMath>
#include <QtTest>

#include <algorithm>
#include <functional>

#include "../vgarment/garment_mesh.h"
#include "../vgarment/piece_mesher.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// A 30 x 20 cm piece.
GarmentMesh rectangleMesh()
{
    return PieceMesher().meshPolygon({QPointF(0, 0), QPointF(30, 0), QPointF(30, 20), QPointF(0, 20)});
}

//---------------------------------------------------------------------------------------------------------------------
// The piece's vertices put somewhere in 3D: a drafted point goes where the function puts it.
QVector<QVector3D> placed(const GarmentMesh& mesh, const std::function<QVector3D(const QPointF&)>& place)
{
    QVector<QVector3D> positions;
    for (const QPointF& point : mesh.rest_positions)
    {
        positions.append(place(point));
    }
    return positions;
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D flat(const QPointF& point)
{
    return QVector3D(static_cast<float>(point.x()), static_cast<float>(-point.y()), 0.0f);
}

//---------------------------------------------------------------------------------------------------------------------
// Whether every vertex has the wanted strain, and if not, which strains they have.
QString strainProblem(const QVector<qreal>& strain, qreal wanted, qreal tolerance)
{
    const auto extremes = std::minmax_element(strain.cbegin(), strain.cend());
    const bool all_wanted = qAbs(*extremes.first - wanted) < tolerance && qAbs(*extremes.second - wanted) < tolerance;
    return all_wanted ? QString()
                      : QStringLiteral("strain from %1 to %2 instead of %3").arg(*extremes.first)
                            .arg(*extremes.second).arg(wanted);
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_GarmentMesh::TST_GarmentMesh(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentMesh::clothAtRestIsNotStretched() const
{
    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain(placed(mesh, flat));

    QCOMPARE(strain.size(), mesh.vertexCount());
    const QString problem = strainProblem(strain, 0.0, 1e-6);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentMesh::movingAndTurningIsNotStretching() const
{
    QMatrix4x4 move;
    move.translate(40, 120, -15);
    move.rotate(35, QVector3D(1, 2, 0.5f).normalized());

    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain(placed(mesh, [&move](const QPointF& point)
    {
        return move.map(flat(point));
    }));

    const QString problem = strainProblem(strain, 0.0, 1e-4);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
// Wrapped around a body, the cloth keeps its length down the body; across, its flat triangles only get a little
// narrower than the curve they cut, and read as a few hundredths of a percent at most.
void TST_GarmentMesh::bendingIsNotStretching() const
{
    const qreal radius = 15;
    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain(placed(mesh, [radius](const QPointF& point)
    {
        const qreal angle = point.x() / radius;
        return QVector3D(static_cast<float>(radius * qSin(angle)), static_cast<float>(-point.y()),
                         static_cast<float>(radius * qCos(angle)));
    }));

    const QString problem = strainProblem(strain, 0.0, 1e-3);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentMesh::stretchIsMeasured() const
{
    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain(placed(mesh, [](const QPointF& point)
    {
        return QVector3D(static_cast<float>(1.1 * point.x()), static_cast<float>(-point.y()), 0.0f);
    }));

    const QString problem = strainProblem(strain, 0.1, 1e-5);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
// Cloth pulled 5% longer one way and pushed 10% together the other way is stretched 5%.
void TST_GarmentMesh::largestStretchCounts() const
{
    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain(placed(mesh, [](const QPointF& point)
    {
        return QVector3D(static_cast<float>(0.9 * point.x()), static_cast<float>(-1.05 * point.y()), 0.0f);
    }));

    const QString problem = strainProblem(strain, 0.05, 1e-5);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_GarmentMesh::positionsOfAnotherMeshAreIgnored() const
{
    const GarmentMesh mesh = rectangleMesh();
    const QVector<qreal> strain = mesh.strain({QVector3D(0, 0, 0), QVector3D(100, 0, 0), QVector3D(0, 100, 0)});

    QCOMPARE(strain.size(), mesh.vertexCount());
    const QString problem = strainProblem(strain, 0.0, 1e-12);
    QVERIFY2(problem.isEmpty(), qUtf8Printable(problem));
}

//---------------------------------------------------------------------------------------------------------------------
// A piece wrapped around a body is lengthened by 4 cm: the new vertices go where the cloth was, and the new hem goes on
// round the body below it.
void TST_GarmentMesh::drapeCarriesOverToAChangedPiece() const
{
    const qreal radius = 15;
    auto wrapped = [radius](const QPointF& point)
    {
        const qreal angle = point.x() / radius;
        return QVector3D(static_cast<float>(radius * qSin(angle)), static_cast<float>(-point.y()),
                         static_cast<float>(radius * qCos(angle)));
    };
    const GarmentMesh before = rectangleMesh();
    const GarmentMesh after = PieceMesher().meshPolygon({QPointF(0, 0), QPointF(30, 0), QPointF(30, 24),
                                                         QPointF(0, 24)});

    const QVector<QVector3D> carried = before.carry(placed(before, wrapped), after);
    QCOMPARE(carried.size(), after.vertexCount());

    // Flat triangles cut the curve a little short, by at most a few hundredths of a cm with 2 cm triangles.
    qreal worst = 0;
    for (int i = 0; i < after.vertexCount(); ++i)
    {
        worst = qMax(worst, static_cast<qreal>((carried.at(i) - wrapped(after.rest_positions.at(i))).length()));
    }
    QVERIFY2(worst < 0.1, qUtf8Printable(QStringLiteral("a vertex is %1 cm off").arg(worst)));

    QVERIFY(before.carry(QVector<QVector3D>(), after).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
// The same piece meshed again with 1 cm triangles, as for a final drape: every new vertex goes where the drape had the
// cloth, and the finer mesh has about four times the vertices.
void TST_GarmentMesh::drapeCarriesOverToAFinerMesh() const
{
    const qreal radius = 15;
    auto wrapped = [radius](const QPointF& point)
    {
        const qreal angle = point.x() / radius;
        return QVector3D(static_cast<float>(radius * qSin(angle)), static_cast<float>(-point.y()),
                         static_cast<float>(radius * qCos(angle)));
    };
    const GarmentMesh coarse = rectangleMesh();
    const GarmentMesh fine = PieceMesher(1.0).meshPolygon({QPointF(0, 0), QPointF(30, 0), QPointF(30, 20),
                                                           QPointF(0, 20)});
    QVERIFY2(fine.vertexCount() > 3 * coarse.vertexCount(),
             qUtf8Printable(QStringLiteral("%1 and %2 vertices").arg(coarse.vertexCount()).arg(fine.vertexCount())));

    const QVector<QVector3D> carried = coarse.carry(placed(coarse, wrapped), fine);
    QCOMPARE(carried.size(), fine.vertexCount());
    qreal worst = 0;
    for (int i = 0; i < fine.vertexCount(); ++i)
    {
        worst = qMax(worst, static_cast<qreal>((carried.at(i) - wrapped(fine.rest_positions.at(i))).length()));
    }
    QVERIFY2(worst < 0.1, qUtf8Printable(QStringLiteral("a vertex is %1 cm off").arg(worst)));
}

//---------------------------------------------------------------------------------------------------------------------
// A drape kept only as where its vertices were in the flat and draped, as a pattern keeps it, carries over to the piece
// lengthened by 4 cm as well as from the mesh itself: the vertices' Delaunay triangles stand in for the mesh's.
void TST_GarmentMesh::drapeKnownByItsVerticesCarriesOver() const
{
    const qreal radius = 15;
    auto wrapped = [radius](const QPointF& point)
    {
        const qreal angle = point.x() / radius;
        return QVector3D(static_cast<float>(radius * qSin(angle)), static_cast<float>(-point.y()),
                         static_cast<float>(radius * qCos(angle)));
    };
    const GarmentMesh before = rectangleMesh();
    const GarmentMesh known = PieceMesher::meshPoints(before.rest_positions);
    QCOMPARE(known.vertexCount(), before.vertexCount());
    QVERIFY(known.triangleCount() > 0);

    const GarmentMesh after = PieceMesher().meshPolygon({QPointF(0, 0), QPointF(30, 0), QPointF(30, 24),
                                                         QPointF(0, 24)});
    const QVector<QVector3D> carried = known.carry(placed(before, wrapped), after);
    QCOMPARE(carried.size(), after.vertexCount());
    qreal worst = 0;
    for (int i = 0; i < after.vertexCount(); ++i)
    {
        worst = qMax(worst, static_cast<qreal>((carried.at(i) - wrapped(after.rest_positions.at(i))).length()));
    }
    QVERIFY2(worst < 0.1, qUtf8Printable(QStringLiteral("a vertex is %1 cm off").arg(worst)));

    QVERIFY(PieceMesher::meshPoints({QPointF(0, 0), QPointF(1, 0)}).indices.isEmpty());
}
