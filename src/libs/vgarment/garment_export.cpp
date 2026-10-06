//---------------------------------------------------------------------------------------------------------------------
//  @file   garment_export.cpp
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

#include "garment_export.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QtMath>

#include <limits>

namespace
{
// Lengths are written in metres.
const float metres_per_cm = 0.01f;

// glTF's numbers for what a binary file holds and what its data is.
const quint32 glb_magic = 0x46546C67;       // "glTF"
const quint32 glb_version = 2;
const quint32 glb_json_chunk = 0x4E4F534A;  // "JSON"
const quint32 glb_bin_chunk = 0x004E4942;   // "BIN"
const int gl_float = 5126;
const int gl_unsigned_int = 5125;
const int gl_array_buffer = 34962;
const int gl_element_array_buffer = 34963;

// Cloth isn't shiny.
const double cloth_roughness = 0.9;

//---------------------------------------------------------------------------------------------------------------------
// A name each mesh can go by in the file: its own, without white space, and numbered where two would be the same.
QStringList uniqueNames(const QVector<ExportMesh>& meshes)
{
    QStringList names;
    QSet<QString> taken;
    for (int i = 0; i < meshes.size(); ++i)
    {
        QString name = meshes.at(i).name.simplified().replace(QRegularExpression(QStringLiteral("\\s")),
                                                               QStringLiteral("_"));
        if (name.isEmpty())
        {
            name = QStringLiteral("mesh");
        }
        QString unique = name;
        for (int number = 2; taken.contains(unique); ++number)
        {
            unique = QStringLiteral("%1_%2").arg(name).arg(number);
        }
        taken.insert(unique);
        names.append(unique);
    }
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
// The meshes with something in them.
QVector<ExportMesh> withTriangles(const QVector<ExportMesh>& meshes)
{
    QVector<ExportMesh> kept;
    for (const ExportMesh& mesh : meshes)
    {
        if (!mesh.positions.isEmpty() && mesh.indices.size() >= 3)
        {
            kept.append(mesh);
        }
    }
    return kept;
}

//---------------------------------------------------------------------------------------------------------------------
// A color as glTF wants it, in linear light.
double linear(qreal srgb)
{
    return srgb <= 0.04045 ? srgb / 12.92 : qPow((srgb + 0.055) / 1.055, 2.4);
}

//---------------------------------------------------------------------------------------------------------------------
bool commit(QSaveFile& file, QString* error)
{
    const bool written = file.commit();
    if (!written && error != nullptr)
    {
        *error = file.errorString();
    }
    return written;
}

//---------------------------------------------------------------------------------------------------------------------
bool open(QSaveFile& file, QString* error)
{
    const bool opened = file.open(QIODevice::WriteOnly);
    if (!opened && error != nullptr)
    {
        *error = file.errorString();
    }
    return opened;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Writes the meshes to an OBJ file, and their materials to an MTL file of the same name next to it; meshes
/// without triangles are left out. Says whether that worked; if not, why in error.
bool GarmentExport::writeObj(const QString& path, const QVector<ExportMesh>& all_meshes, QString* error)
{
    const QVector<ExportMesh> meshes = withTriangles(all_meshes);
    const QFileInfo info(path);
    const QString material_file = info.completeBaseName() + QStringLiteral(".mtl");
    const QStringList names = uniqueNames(meshes);

    QSaveFile materials(info.dir().filePath(material_file));
    if (!open(materials, error))
    {
        return false;
    }
    QTextStream material_stream(&materials);
    material_stream.setRealNumberNotation(QTextStream::FixedNotation);
    material_stream.setRealNumberPrecision(4);
    material_stream << "# " << QCoreApplication::applicationName() << " 3D View\n";
    for (int i = 0; i < meshes.size(); ++i)
    {
        const QColor& color = meshes.at(i).color;
        material_stream << "\nnewmtl " << names.at(i) << "\n"
                        << "Kd " << color.redF() << ' ' << color.greenF() << ' ' << color.blueF() << "\n"
                        << "Ka 0.0000 0.0000 0.0000\nKs 0.0000 0.0000 0.0000\nd 1.0000\nillum 1\n";
    }
    material_stream.flush();

    QSaveFile objects(path);
    if (!open(objects, error))
    {
        return false;
    }
    QTextStream stream(&objects);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(5);
    stream << "# " << QCoreApplication::applicationName() << " 3D View, in metres\n"
           << "mtllib " << material_file << "\n";

    // OBJ numbers vertices, texture coordinates and normals from 1 over the whole file.
    int first_vertex = 1;
    int first_flat = 1;
    for (int i = 0; i < meshes.size(); ++i)
    {
        const ExportMesh& mesh = meshes.at(i);
        const bool textured = mesh.flat.size() == mesh.positions.size();
        stream << "\no " << names.at(i) << "\n";
        for (const QVector3D& position : mesh.positions)
        {
            const QVector3D metres = position * metres_per_cm;
            stream << "v " << metres.x() << ' ' << metres.y() << ' ' << metres.z() << "\n";
        }
        for (const QPointF& point : textured ? mesh.flat : QVector<QPointF>())
        {
            stream << "vt " << point.x() * metres_per_cm << ' ' << -point.y() * metres_per_cm << "\n";
        }
        for (const QVector3D& normal : normals(mesh.positions, mesh.indices))
        {
            stream << "vn " << normal.x() << ' ' << normal.y() << ' ' << normal.z() << "\n";
        }
        stream << "usemtl " << names.at(i) << "\n";
        for (int t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            stream << 'f';
            for (int k = 0; k < 3; ++k)
            {
                const int vertex = first_vertex + static_cast<int>(mesh.indices.at(t + k));
                stream << ' ' << vertex << '/';
                if (textured)
                {
                    stream << first_flat + static_cast<int>(mesh.indices.at(t + k));
                }
                stream << '/' << vertex;
            }
            stream << "\n";
        }
        first_vertex += mesh.positions.size();
        first_flat += textured ? mesh.flat.size() : 0;
    }
    stream.flush();

    return commit(objects, error) && commit(materials, error);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Writes the meshes to a binary glTF file, each mesh a node of its own; meshes without triangles are left out.
/// Says whether that worked; if not, why in error.
bool GarmentExport::writeGlb(const QString& path, const QVector<ExportMesh>& all_meshes, QString* error)
{
    const QVector<ExportMesh> meshes = withTriangles(all_meshes);
    const QStringList names = uniqueNames(meshes);

    QByteArray binary;
    QBuffer binary_buffer(&binary);
    binary_buffer.open(QIODevice::WriteOnly);
    QDataStream data(&binary_buffer);
    data.setByteOrder(QDataStream::LittleEndian);
    data.setFloatingPointPrecision(QDataStream::SinglePrecision);

    QJsonArray buffer_views;
    QJsonArray accessors;
    // Adds what was written to the binary since start as a buffer view and an accessor of it, padded to 4 bytes.
    auto add_accessor = [&binary_buffer, &data, &buffer_views, &accessors](qint64 start, int target, int component,
                                                                         int count, const QString& type)
    {
        QJsonObject view;
        view.insert(QStringLiteral("buffer"), 0);
        view.insert(QStringLiteral("byteOffset"), start);
        view.insert(QStringLiteral("byteLength"), binary_buffer.pos() - start);
        view.insert(QStringLiteral("target"), target);
        buffer_views.append(view);
        while (binary_buffer.pos() % 4 != 0)
        {
            data << static_cast<quint8>(0);
        }

        QJsonObject accessor;
        accessor.insert(QStringLiteral("bufferView"), buffer_views.size() - 1);
        accessor.insert(QStringLiteral("componentType"), component);
        accessor.insert(QStringLiteral("count"), count);
        accessor.insert(QStringLiteral("type"), type);
        accessors.append(accessor);
        return accessors.size() - 1;
    };

    QJsonArray nodes;
    QJsonArray scene_nodes;
    QJsonArray gltf_meshes;
    QJsonArray materials;
    for (int i = 0; i < meshes.size(); ++i)
    {
        const ExportMesh& mesh = meshes.at(i);
        QJsonObject attributes;

        // Positions, with the box around them, which glTF asks for.
        QVector3D lowest(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max());
        QVector3D highest = -lowest;
        qint64 start = binary_buffer.pos();
        for (const QVector3D& position : mesh.positions)
        {
            const QVector3D metres = position * metres_per_cm;
            data << metres.x() << metres.y() << metres.z();
            for (int axis = 0; axis < 3; ++axis)
            {
                lowest[axis] = qMin(lowest[axis], metres[axis]);
                highest[axis] = qMax(highest[axis], metres[axis]);
            }
        }
        const int position_accessor = add_accessor(start, gl_array_buffer, gl_float,
                                                   static_cast<int>(mesh.positions.size()), QStringLiteral("VEC3"));
        QJsonObject bounded = accessors.at(position_accessor).toObject();
        bounded.insert(QStringLiteral("min"), QJsonArray{lowest.x(), lowest.y(), lowest.z()});
        bounded.insert(QStringLiteral("max"), QJsonArray{highest.x(), highest.y(), highest.z()});
        accessors.replace(position_accessor, bounded);
        attributes.insert(QStringLiteral("POSITION"), position_accessor);

        start = binary_buffer.pos();
        const QVector<QVector3D> mesh_normals = normals(mesh.positions, mesh.indices);
        for (const QVector3D& normal : mesh_normals)
        {
            data << normal.x() << normal.y() << normal.z();
        }
        attributes.insert(QStringLiteral("NORMAL"),
                          add_accessor(start, gl_array_buffer, gl_float, static_cast<int>(mesh_normals.size()),
                                       QStringLiteral("VEC3")));

        if (mesh.flat.size() == mesh.positions.size() && !mesh.flat.isEmpty())
        {
            start = binary_buffer.pos();
            for (const QPointF& point : mesh.flat)
            {
                data << static_cast<float>(point.x()) * metres_per_cm << static_cast<float>(point.y()) * metres_per_cm;
            }
            attributes.insert(QStringLiteral("TEXCOORD_0"),
                              add_accessor(start, gl_array_buffer, gl_float, static_cast<int>(mesh.flat.size()),
                                           QStringLiteral("VEC2")));
        }

        start = binary_buffer.pos();
        for (const quint32 index : mesh.indices)
        {
            data << index;
        }
        const int index_accessor = add_accessor(start, gl_element_array_buffer, gl_unsigned_int,
                                                static_cast<int>(mesh.indices.size()), QStringLiteral("SCALAR"));

        QJsonObject color;
        color.insert(QStringLiteral("baseColorFactor"),
                     QJsonArray{linear(mesh.color.redF()), linear(mesh.color.greenF()), linear(mesh.color.blueF()),
                                1.0});
        color.insert(QStringLiteral("metallicFactor"), 0.0);
        color.insert(QStringLiteral("roughnessFactor"), cloth_roughness);
        QJsonObject material;
        material.insert(QStringLiteral("name"), names.at(i));
        material.insert(QStringLiteral("pbrMetallicRoughness"), color);
        material.insert(QStringLiteral("doubleSided"), true);
        materials.append(material);

        QJsonObject primitive;
        primitive.insert(QStringLiteral("attributes"), attributes);
        primitive.insert(QStringLiteral("indices"), index_accessor);
        primitive.insert(QStringLiteral("material"), i);
        QJsonObject gltf_mesh;
        gltf_mesh.insert(QStringLiteral("name"), names.at(i));
        gltf_mesh.insert(QStringLiteral("primitives"), QJsonArray{primitive});
        gltf_meshes.append(gltf_mesh);

        QJsonObject node;
        node.insert(QStringLiteral("name"), names.at(i));
        node.insert(QStringLiteral("mesh"), i);
        nodes.append(node);
        scene_nodes.append(i);
    }
    binary_buffer.close();

    QJsonObject asset;
    asset.insert(QStringLiteral("version"), QStringLiteral("2.0"));
    asset.insert(QStringLiteral("generator"), QCoreApplication::applicationName());
    QJsonObject scene;
    scene.insert(QStringLiteral("nodes"), scene_nodes);
    QJsonObject buffer;
    buffer.insert(QStringLiteral("byteLength"), binary.size());

    QJsonObject root;
    root.insert(QStringLiteral("asset"), asset);
    root.insert(QStringLiteral("scene"), 0);
    root.insert(QStringLiteral("scenes"), QJsonArray{scene});
    root.insert(QStringLiteral("nodes"), nodes);
    root.insert(QStringLiteral("meshes"), gltf_meshes);
    root.insert(QStringLiteral("materials"), materials);
    root.insert(QStringLiteral("accessors"), accessors);
    root.insert(QStringLiteral("bufferViews"), buffer_views);
    if (!binary.isEmpty())
    {
        root.insert(QStringLiteral("buffers"), QJsonArray{buffer});
    }

    // Chunks are padded to 4 bytes, the JSON with spaces.
    QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    while (json.size() % 4 != 0)
    {
        json.append(' ');
    }

    QSaveFile file(path);
    if (!open(file, error))
    {
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    const quint32 length = static_cast<quint32>(12 + 8 + json.size() + (binary.isEmpty() ? 0 : 8 + binary.size()));
    out << glb_magic << glb_version << length;
    out << static_cast<quint32>(json.size()) << glb_json_chunk;
    out.writeRawData(json.constData(), static_cast<int>(json.size()));
    if (!binary.isEmpty())
    {
        out << static_cast<quint32>(binary.size()) << glb_bin_chunk;
        out.writeRawData(binary.constData(), static_cast<int>(binary.size()));
    }
    return commit(file, error);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Normals of a mesh's vertices: the triangles' around each vertex, weighted by their areas. They point out of
/// the side a triangle runs anticlockwise on.
QVector<QVector3D> GarmentExport::normals(const QVector<QVector3D>& positions, const QVector<quint32>& indices)
{
    QVector<QVector3D> sums(positions.size());
    for (int t = 0; t + 2 < indices.size(); t += 3)
    {
        const int a = static_cast<int>(indices.at(t));
        const int b = static_cast<int>(indices.at(t + 1));
        const int c = static_cast<int>(indices.at(t + 2));
        const QVector3D doubled_area = QVector3D::crossProduct(positions.at(b) - positions.at(a),
                                                               positions.at(c) - positions.at(a));
        sums[a] += doubled_area;
        sums[b] += doubled_area;
        sums[c] += doubled_area;
    }
    for (QVector3D& sum : sums)
    {
        sum = sum.isNull() ? QVector3D(0, 1, 0) : sum.normalized();
    }
    return sums;
}
