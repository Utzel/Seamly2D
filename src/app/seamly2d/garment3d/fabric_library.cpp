//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_library.cpp
//  @author Julius
//  @date   9 Oct, 2026
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

#include "fabric_library.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>

#include <algorithm>

#include "../ifc/exception/vexception.h"
#include "../ifc/xml/fabric_converter.h"
#include "../vformat/fabric_file.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// A file name for a fabric: its name, the characters file systems don't take in names replaced.
QString fileNameOf(const QString& name)
{
    const QString not_taken = QStringLiteral("\\/:*?\"<>|");
    QString file_name = name.trimmed();
    for (QChar& character : file_name)
    {
        if (character.unicode() < 0x20 || not_taken.contains(character))
        {
            character = QLatin1Char('_');
        }
    }
    return file_name.isEmpty() ? QStringLiteral("fabric") : file_name;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
FabricLibrary::FabricLibrary(QObject* parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_folder()
    , m_entries()
{
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FabricLibrary::read);
    update();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The folder the library's fabric files are in.
QString FabricLibrary::folder() const
{
    return m_folder;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The library's fabrics, in the order of their files' names.
QVector<FabricLibrary::Entry> FabricLibrary::entries() const
{
    return m_entries;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The library's fabric of that name, whatever its case; one with no file for none.
FabricLibrary::Entry FabricLibrary::entry(const QString& name) const
{
    for (const Entry& entry : m_entries)
    {
        if (entry.fabric.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return entry;
        }
    }
    return Entry();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Puts the fabric into the library: into the file of the library's fabric of its name, or into a new one named
/// after it. Says whether it could, and if not, why.
bool FabricLibrary::add(const VCustomFabric& fabric, QString* error)
{
    QString path = entry(fabric.name).path;
    if (path.isEmpty())
    {
        if (!QDir().mkpath(m_folder))
        {
            *error = tr("The folder %1 can't be made.").arg(QDir::toNativeSeparators(m_folder));
            return false;
        }
        const QString base = QDir(m_folder).filePath(fileNameOf(fabric.name));
        path = base + QLatin1Char('.') + VFabricFile::Extension;
        for (int number = 2; QFileInfo::exists(path); ++number)
        {
            path = QStringLiteral("%1 %2.%3").arg(base).arg(number).arg(VFabricFile::Extension);
        }
    }

    VFabricFile file;
    file.setFabric(fabric);
    if (!file.SaveDocument(path, *error))
    {
        return false;
    }
    read();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Takes the fabric files of the folder the preferences name now, if they name another.
void FabricLibrary::update()
{
    const QString folder = qApp->Settings()->getFabricPath();
    if (folder != m_folder)
    {
        const QStringList watched = m_watcher->directories();
        if (!watched.isEmpty())
        {
            m_watcher->removePaths(watched);
        }
        m_folder = folder;
        read();
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Reads the folder's fabric files, in the order of their names; of fabrics of the same name, whatever its case, the
// first is kept, and files that aren't fabric files, or are of a newer version, are passed over.
void FabricLibrary::read()
{
    if (m_watcher->directories().isEmpty() && QDir(m_folder).exists())
    {
        m_watcher->addPath(m_folder);
    }

    QVector<Entry> entries;
    const QFileInfoList files = QDir(m_folder).entryInfoList({QStringLiteral("*.") + VFabricFile::Extension},
                                                             QDir::Files | QDir::Readable,
                                                             QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& file : files)
    {
        VCustomFabric fabric;
        try
        {
            VFabricFile fabric_file;
            fabric_file.setXMLContent(VFabricConverter(file.absoluteFilePath()).Convert());
            fabric = fabric_file.fabric();
        }
        catch (const VException&)
        {
            continue;
        }
        const bool known = std::any_of(entries.cbegin(), entries.cend(), [&fabric](const Entry& entry)
        {
            return entry.fabric.name.compare(fabric.name, Qt::CaseInsensitive) == 0;
        });
        if (!fabric.name.isEmpty() && !known)
        {
            entries.append({file.absoluteFilePath(), fabric});
        }
    }
    m_entries = entries;
    emit changed();
}
