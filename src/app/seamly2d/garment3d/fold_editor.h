//---------------------------------------------------------------------------------------------------------------------
//  @file   fold_editor.h
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

#ifndef FOLD_EDITOR_H
#define FOLD_EDITOR_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"
#include "../vgarment/cloth_solver.h"
#include "piece_geometry.h"
#include "shown_piece.h"

/// @brief Lets the pieces in the 3D scene be folded along their internal paths with the mouse, for the scene's QML,
/// and says where the cloth folds.
///
/// While folding, the internal paths show on the pieces, the folds in a color of their own, on the board and on the
/// avatar. A click near a path folds the piece along it at the angle chosen, folds it at that angle instead if it is
/// folded at another, or unfolds it if it is folded at that angle. Positions come from QML in the flat coordinates of
/// the mesh clicked. The editor only proposes changes (foldsEdited); they are made through the undo stack and come back
/// with setFolds().
class FoldEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool folding READ isFolding NOTIFY foldingChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)

public:
    explicit                       FoldEditor(QObject* parent = nullptr);

    void                           setPieces(const QVector<ShownPiece>& pieces, const QVector<VFold>& folds);
    void                           setFolds(const QVector<VFold>& folds);
    QVector<VFold>                 folds() const;
    void                           setHighlightColor(const QColor& color);

    bool                           isFolding() const;
    void                           setFolding(bool folding);
    qreal                          angle() const;
    void                           setAngle(qreal angle);
    QString                        hint() const;
    bool                           cancel();

    QHash<quint32, QVector<DrawnLine>> lines() const;
    QVector<ClothFold>             clothFolds(quint32 piece_id, const GarmentMesh& mesh, qreal strength) const;

    Q_INVOKABLE void               hover(int id, qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void               leave();
    Q_INVOKABLE bool               click(int id, qreal x, qreal y, qreal tolerance);

signals:
    void                           foldingChanged();
    void                           hintChanged();
    void                           linesChanged();
    void                           foldsEdited(const QVector<VFold>& folds, const QString& text);

private:
    Q_DISABLE_COPY(FoldEditor)

    // An internal path of a piece.
    struct Path
    {
        quint32 piece_id = 0;
        quint32 path_id = 0;

        bool    isValid() const;
        bool    operator==(const Path& other) const;
    };

    QVector<ShownPiece>            m_pieces;
    QVector<VFold>                 m_folds;
    QColor                         m_highlight;
    bool                           m_folding;
    qreal                          m_angle;
    Path                           m_hovered;

    int                            foldOf(const Path& path) const;
    Path                           pathAt(quint32 id, const QPointF& point, qreal tolerance) const;
    bool                           hasPaths() const;
    void                           setHovered(const Path& path);
};

#endif // FOLD_EDITOR_H
