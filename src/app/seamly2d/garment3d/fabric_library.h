//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_library.h
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

#ifndef FABRIC_LIBRARY_H
#define FABRIC_LIBRARY_H

#include <QObject>
#include <QString>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"

class QFileSystemWatcher;

/// @brief The fabric library, as CLO's: the fabric files in the folder the preferences name, one fabric each, which
/// any pattern can take its fabrics from and add its own to. The folder is watched, so fabric files put there or taken
/// away show at once; files that aren't fabric files are passed over.
class FabricLibrary : public QObject
{
    Q_OBJECT
public:
    /// A fabric of the library and the file it is kept in.
    struct Entry
    {
        QString       path;
        VCustomFabric fabric;
    };

    explicit           FabricLibrary(QObject* parent = nullptr);
    virtual           ~FabricLibrary() = default;

    QString            folder() const;
    QVector<Entry>     entries() const;
    Entry              entry(const QString& name) const;
    bool               add(const VCustomFabric& fabric, QString* error);
    void               update();

signals:
    void               changed();

private slots:
    void               read();

private:
    Q_DISABLE_COPY(FabricLibrary)

    QFileSystemWatcher* m_watcher;
    QString            m_folder;
    QVector<Entry>     m_entries;
};

#endif // FABRIC_LIBRARY_H
