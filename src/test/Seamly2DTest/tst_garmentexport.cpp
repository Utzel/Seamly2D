//---------------------------------------------------------------------------------------------------------------------
//  @file   tst_garmentexport.cpp
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

#include "tst_garmentexport.h"

#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include "../vgarment/garment_export.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// A piece 20 by 10 cm standing upright 100 cm up, facing +z, and a triangle for an avatar, with two names alike.
QVector<ExportMesh> sampleMeshes()
{
    ExportMesh piece;
    piece.name = QStringLiteral("Front piece");
    piece.color = QColor(255, 0, 0);
    piece.positions = {QVector3D(0, 110, 5), QVector3D(20, 110, 5), QVector3D(20, 100, 5), QVector3D(0, 100, 5)};
    piece.flat = {QPointF(0, 0), QPointF(20, 0), QPointF(20, 10), QPointF(0, 10)};
    piece.indices = {0, 3, 2, 0, 2, 1};

    ExportMesh avatar;
    avatar.name = QStringLiteral("Front piece");
    avatar.color = QColor(128, 128, 128);
    avatar.positions = {QVector3D(0, 0, 0), QVector3D(10, 0, 0), QVector3D(0, 10, 0)};
    avatar.indices = {0, 1, 2};

    ExportMesh empty;
    empty.name = QStringLiteral("Nothing");

    return {piece, avatar, empty};
}

//---------------------------------------------------------------------------------------------------------------------
QStringList linesStartingWith(const QStringList& lines, const QString& start)
{
    QStringList found;
    for (const QString& line : lines)
    {
        if (line.startsWith(start))
        {
            found.append(line);
        }
    }
    return found;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
TST_GarmentExport::TST_GarmentExport(QObject* parent)
    : QObject(parent)
{}

//---------------------------------------------------------------------------------------------------------------------
// Triangles running anticlockwise seen from +z have normals along +z.
void TST_GarmentExport::normalsPointOutOfTheFront() const
{
    const QVector<ExportMesh> meshes = sampleMeshes();
    for (const QVector3D& normal : GarmentExport::normals(meshes.at(0).positions, meshes.at(0).indices))
    {
        QVERIFY((normal - QVector3D(0, 0, 1)).length() < 1e-6f);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Every mesh with triangles is an object of its own, in metres, a piece with its flat shape as texture coordinates,
// and has its own material with its color.
void TST_GarmentExport::objHoldsEveryMesh() const
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY2(GarmentExport::writeObj(dir.filePath(QStringLiteral("drape.obj")), sampleMeshes(), &error),
             qUtf8Printable(error));

    QFile obj(dir.filePath(QStringLiteral("drape.obj")));
    QVERIFY(obj.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList lines = QTextStream(&obj).readAll().split(QLatin1Char('\n'));
    QVERIFY(lines.contains(QStringLiteral("mtllib drape.mtl")));
    QCOMPARE(linesStartingWith(lines, QStringLiteral("o ")),
             QStringList({QStringLiteral("o Front_piece"), QStringLiteral("o Front_piece_2")}));
    QCOMPARE(linesStartingWith(lines, QStringLiteral("v ")).size(), 7);
    QCOMPARE(linesStartingWith(lines, QStringLiteral("vt ")).size(), 4);
    QCOMPARE(linesStartingWith(lines, QStringLiteral("vn ")).size(), 7);
    QCOMPARE(linesStartingWith(lines, QStringLiteral("v ")).first(), QStringLiteral("v 0.00000 1.10000 0.05000"));
    QCOMPARE(linesStartingWith(lines, QStringLiteral("vt ")).at(2), QStringLiteral("vt 0.20000 -0.10000"));

    // The avatar's vertices come after the piece's, and it has no texture coordinates.
    const QStringList faces = linesStartingWith(lines, QStringLiteral("f "));
    QCOMPARE(faces.size(), 3);
    QCOMPARE(faces.first(), QStringLiteral("f 1/1/1 4/4/4 3/3/3"));
    QCOMPARE(faces.last(), QStringLiteral("f 5//5 6//6 7//7"));

    QFile mtl(dir.filePath(QStringLiteral("drape.mtl")));
    QVERIFY(mtl.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList materials = QTextStream(&mtl).readAll().split(QLatin1Char('\n'));
    QCOMPARE(linesStartingWith(materials, QStringLiteral("newmtl ")).size(), 2);
    QVERIFY(materials.contains(QStringLiteral("Kd 1.0000 0.0000 0.0000")));
    QCOMPARE(linesStartingWith(lines, QStringLiteral("usemtl ")),
             QStringList({QStringLiteral("usemtl Front_piece"), QStringLiteral("usemtl Front_piece_2")}));
}

//---------------------------------------------------------------------------------------------------------------------
// A glTF 2.0 binary: its header, a JSON chunk describing a node, mesh and material per mesh with triangles, and the
// data in a binary chunk where its accessors say.
void TST_GarmentExport::glbIsBinaryGltf() const
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("drape.glb"));
    QString error;
    QVERIFY2(GarmentExport::writeGlb(path, sampleMeshes(), &error), qUtf8Printable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0;
    quint32 version = 0;
    quint32 length = 0;
    quint32 json_length = 0;
    quint32 json_type = 0;
    stream >> magic >> version >> length >> json_length >> json_type;
    QCOMPARE(magic, 0x46546C67u);
    QCOMPARE(version, 2u);
    QCOMPARE(static_cast<int>(length), bytes.size());
    QCOMPARE(json_type, 0x4E4F534Au);
    QCOMPARE(json_length % 4, 0u);

    QJsonParseError parse_error;
    const QJsonObject root = QJsonDocument::fromJson(bytes.mid(20, static_cast<int>(json_length)), &parse_error)
                                 .object();
    QCOMPARE(parse_error.error, QJsonParseError::NoError);
    QCOMPARE(root.value(QStringLiteral("asset")).toObject().value(QStringLiteral("version")).toString(),
             QStringLiteral("2.0"));
    QCOMPARE(root.value(QStringLiteral("nodes")).toArray().size(), 2);
    QCOMPARE(root.value(QStringLiteral("meshes")).toArray().size(), 2);
    QCOMPARE(root.value(QStringLiteral("materials")).toArray().size(), 2);
    QVERIFY(root.value(QStringLiteral("materials")).toArray().at(0).toObject().value(QStringLiteral("doubleSided"))
                .toBool());

    const QByteArray binary = bytes.mid(20 + static_cast<int>(json_length) + 8);
    quint32 binary_length = 0;
    quint32 binary_type = 0;
    QDataStream chunk(bytes.mid(20 + static_cast<int>(json_length), 8));
    chunk.setByteOrder(QDataStream::LittleEndian);
    chunk >> binary_length >> binary_type;
    QCOMPARE(binary_type, 0x004E4942u);
    QCOMPARE(static_cast<int>(binary_length), binary.size());
    QCOMPARE(root.value(QStringLiteral("buffers")).toArray().at(0).toObject().value(QStringLiteral("byteLength"))
                 .toInt(), binary.size());

    // The piece has texture coordinates, the avatar not; the piece's first position is in metres, inside the box its
    // accessor gives.
    const QJsonObject piece_attributes = root.value(QStringLiteral("meshes")).toArray().at(0).toObject()
                                             .value(QStringLiteral("primitives")).toArray().at(0).toObject()
                                             .value(QStringLiteral("attributes")).toObject();
    const QJsonObject avatar_attributes = root.value(QStringLiteral("meshes")).toArray().at(1).toObject()
                                              .value(QStringLiteral("primitives")).toArray().at(0).toObject()
                                              .value(QStringLiteral("attributes")).toObject();
    QVERIFY(piece_attributes.contains(QStringLiteral("TEXCOORD_0")));
    QVERIFY(!avatar_attributes.contains(QStringLiteral("TEXCOORD_0")));

    const QJsonArray accessors = root.value(QStringLiteral("accessors")).toArray();
    const QJsonObject positions = accessors.at(piece_attributes.value(QStringLiteral("POSITION")).toInt()).toObject();
    QCOMPARE(positions.value(QStringLiteral("count")).toInt(), 4);
    QVERIFY(qAbs(positions.value(QStringLiteral("max")).toArray().at(1).toDouble() - 1.1) < 1e-6);
    const QJsonObject view = root.value(QStringLiteral("bufferViews")).toArray()
                                 .at(positions.value(QStringLiteral("bufferView")).toInt()).toObject();
    QDataStream data(binary.mid(view.value(QStringLiteral("byteOffset")).toInt(), 12));
    data.setByteOrder(QDataStream::LittleEndian);
    data.setFloatingPointPrecision(QDataStream::SinglePrecision);
    float x = 0;
    float y = 0;
    float z = 0;
    data >> x >> y >> z;
    QVERIFY(qAbs(x) < 1e-6f && qAbs(y - 1.1f) < 1e-6f && qAbs(z - 0.05f) < 1e-6f);
}
