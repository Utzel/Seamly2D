//---------------------------------------------------------------------------------------------------------------------
//  @file   stitch_editor.h
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

#ifndef STITCH_EDITOR_H
#define STITCH_EDITOR_H

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"
#include "../vgarment/topstitch.h"
#include "shown_piece.h"

/// @brief Works out the topstitching each piece in the 3D scene shows, and lets edges be stitched with the mouse, for
/// the scene's QML.
///
/// A piece is stitched along the segments of its seam line the pattern's topstitches name, or with the whole garment
/// stitched along every segment but a fold, each in its own topstitch style or the garment's, and along its internal
/// paths drawn dashed or dotted, which stand for stitching drawn on the pattern, in the garment's style's stitches.
/// The stitching is worked out on the piece as drafted and laid onto each mesh the scene shows of it: the drafted piece
/// on the board, and on the avatar the piece unfolded, or the piece and its mirrored copy.
///
/// While stitching, a click near an edge of a piece, on the board or on the avatar, stitches it in the garment's style,
/// the style chosen; a click on an edge stitched in another style stitches it in the chosen one instead, and a click on
/// one stitched in the chosen style takes its stitches out. Positions come from QML in the flat coordinates of the
/// mesh clicked. The editor only proposes changes (topstitchesEdited); they are made through the undo stack and come
/// back with setTopstitches().
class StitchEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool stitching READ isStitching NOTIFY stitchingChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)

public:
    explicit                       StitchEditor(QObject* parent = nullptr);

    void                           setPieces(const QVector<ShownPiece>& pieces, const VTopstitches& topstitches);
    void                           setTopstitches(const VTopstitches& topstitches);

    bool                           isStitching() const;
    void                           setStitching(bool stitching);
    QString                        hint() const;
    bool                           cancel();

    QHash<quint32, QVector<ThreadStitch>> stitches() const;
    QHash<quint32, QVector<ThreadStitch>> preview() const;

    Q_INVOKABLE void               hover(int id, qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void               leave();
    Q_INVOKABLE bool               click(int id, qreal x, qreal y, qreal tolerance);

signals:
    void                           stitchingChanged();
    void                           hintChanged();
    void                           stitchesChanged();
    void                           previewChanged();
    void                           topstitchesEdited(const VTopstitches& topstitches, const QString& text);

private:
    Q_DISABLE_COPY(StitchEditor)

    // A segment of a piece's seam line, as drafted.
    struct Edge
    {
        quint32 piece_id = 0;
        int     segment = -1;

        bool    isValid() const;
        bool    operator==(const Edge& other) const;
    };

    QVector<ShownPiece>            m_pieces;
    VTopstitches                   m_topstitches;
    bool                           m_stitching;
    Edge                           m_hovered;
    QHash<quint32, QVector<ThreadStitch>> m_stitches;
    QHash<quint32, QVector<ThreadStitch>> m_preview;

    const ShownPiece*              pieceShowing(quint32 id, const ShownMesh** shown) const;
    QVector<QString>               segmentStyles(const ShownPiece& piece) const;
    TopstitchStyle                 chosenStyle() const;
    Edge                           edgeAt(quint32 id, const QPointF& point, qreal tolerance) const;
    void                           setHovered(const Edge& edge);
    void                           workOutStitches();
    void                           workOutPreview();
};

#endif // STITCH_EDITOR_H
