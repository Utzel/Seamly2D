//---------------------------------------------------------------------------------------------------------------------
//  @file   elastic_editor.h
//  @author Julius
//  @date   8 Oct, 2026
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

#ifndef ELASTIC_EDITOR_H
#define ELASTIC_EDITOR_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"
#include "../vgarment/cloth_solver.h"
#include "../vgarment/garment_symmetry.h"
#include "piece_geometry.h"
#include "shown_piece.h"

/// @brief Lets elastic be sewn along the edges and internal paths of the pieces in the 3D scene with the mouse, as
/// CLO's elastic, for the scene's QML, and says where the cloth gathers.
///
/// While sewing elastic, the elastics show on the pieces in a color of their own, on the board and on the avatar, and
/// the internal paths without in grey. A click near an edge or an internal path sews elastic along it, as long as the
/// ratio chosen says, its share of the line's length; sews it that long instead if it has elastic of another ratio, or
/// takes the elastic out if it has that ratio. On a piece cut on the fold, an edge's elastic goes along its mirror image
/// too. Positions come from QML in the flat coordinates of the mesh clicked. The editor only proposes changes
/// (elasticsEdited); they are made through the undo stack and come back with setElastics().
class ElasticEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool editing READ isEditing NOTIFY editingChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)

public:
    explicit                       ElasticEditor(QObject* parent = nullptr);

    void                           setPieces(const QVector<ShownPiece>& pieces, const QVector<VElastic>& elastics);
    void                           setElastics(const QVector<VElastic>& elastics);
    QVector<VElastic>              elastics() const;
    void                           setHighlightColor(const QColor& color);

    bool                           isEditing() const;
    void                           setEditing(bool editing);
    qreal                          ratio() const;
    void                           setRatio(qreal ratio);
    QString                        hint() const;
    bool                           cancel();

    QHash<quint32, QVector<DrawnLine>> lines() const;
    QVector<ClothElastic>          clothElastics(quint32 piece_id, const GarmentMesh& mesh, quint32 offset) const;

    Q_INVOKABLE void               hover(int id, qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void               leave();
    Q_INVOKABLE bool               click(int id, qreal x, qreal y, qreal tolerance);

signals:
    void                           editingChanged();
    void                           hintChanged();
    void                           linesChanged();
    void                           elasticsEdited(const QVector<VElastic>& elastics, const QString& text);

private:
    Q_DISABLE_COPY(ElasticEditor)

    // A segment of a piece's seam line, by the path points it runs between, or one of its internal paths.
    struct Line
    {
        quint32 piece_id = 0;
        quint32 start_node = NULL_ID;
        quint32 end_node = NULL_ID;
        quint32 path_id = NULL_ID;

        bool    isValid() const;
        bool    operator==(const Line& other) const;
    };

    QVector<ShownPiece>            m_pieces;
    QVector<VElastic>              m_elastics;
    GarmentSymmetry                m_symmetry;
    QColor                         m_highlight;
    bool                           m_editing;
    qreal                          m_ratio;
    Line                           m_hovered;

    int                            elasticOf(const Line& line) const;
    Line                           lineAt(quint32 id, const QPointF& point, qreal tolerance) const;
    QVector<QVector<quint32>>      meshLines(const GarmentMesh& mesh, const Line& line) const;
    void                           setHovered(const Line& line);
};

#endif // ELASTIC_EDITOR_H
