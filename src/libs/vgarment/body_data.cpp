//---------------------------------------------------------------------------------------------------------------------
//  @file   body_data.cpp
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

#include "body_data.h"

#include <QByteArray>
#include <QDataStream>
#include <QFile>

#include <algorithm>

namespace
{
const char  file_magic[4] = {'S', 'M', 'B', 'D'};
const quint32 file_version = 1;

// Target offsets are stored as 1/100 cm.
const float offset_scale = 0.01f;

//---------------------------------------------------------------------------------------------------------------------
QString readName(QDataStream& stream)
{
    quint16 length = 0;
    stream >> length;
    QByteArray utf8(length, Qt::Uninitialized);
    stream.readRawData(utf8.data(), length);
    return QString::fromUtf8(utf8);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<quint32> readIndices(QDataStream& stream, quint32 count)
{
    QVector<quint32> indices(static_cast<int>(count));
    for (quint32& index : indices)
    {
        stream >> index;
    }
    return indices;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Reads a body.dat file. Returns null if it can't be read or isn't a body file of a version this code knows.
QSharedPointer<const BodyData> BodyData::load(const QString& path)
{
    QSharedPointer<BodyData> data;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
    {
        const QByteArray content = qUncompress(file.readAll());

        QDataStream stream(content);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

        char magic[4] = {};
        quint32 version = 0;
        stream.readRawData(magic, sizeof(magic));
        stream >> version;

        if (std::equal(magic, magic + sizeof(magic), file_magic) && version == file_version)
        {
            data.reset(new BodyData);

            quint32 vertex_count = 0;
            stream >> vertex_count;
            data->base_positions.resize(static_cast<int>(vertex_count));
            for (QVector3D& position : data->base_positions)
            {
                float x = 0;
                float y = 0;
                float z = 0;
                stream >> x >> y >> z;
                position = QVector3D(x, y, z);
            }

            quint32 skin_vertex_count = 0;
            quint32 triangle_count = 0;
            stream >> skin_vertex_count >> triangle_count;
            data->skin_vertex_count = static_cast<int>(skin_vertex_count);
            data->triangles = readIndices(stream, 3 * triangle_count);

            quint32 joint_count = 0;
            stream >> joint_count;
            for (quint32 i = 0; i < joint_count; ++i)
            {
                const QString name = readName(stream);
                quint32 count = 0;
                stream >> count;
                data->joints.insert(name, readIndices(stream, count));
            }

            quint32 target_count = 0;
            stream >> target_count;
            for (quint32 i = 0; i < target_count; ++i)
            {
                const QString name = readName(stream);
                quint32 count = 0;
                stream >> count;

                BodyTarget target;
                target.indices = readIndices(stream, count);
                target.offsets.resize(static_cast<int>(count));
                for (QVector3D& offset : target.offsets)
                {
                    qint16 x = 0;
                    qint16 y = 0;
                    qint16 z = 0;
                    stream >> x >> y >> z;
                    offset = QVector3D(x, y, z) * offset_scale;
                }
                data->targets.insert(name, target);
            }

            if (stream.status() != QDataStream::Ok)
            {
                data.reset();
            }
        }
    }
    return data;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The body data built into Seamly2D, loaded on first use.
QSharedPointer<const BodyData> BodyData::standard()
{
    static const QSharedPointer<const BodyData> data = []()
    {
        Q_INIT_RESOURCE(avatar);
        return load(QStringLiteral(":/avatar/body.dat"));
    }();
    return data;
}
