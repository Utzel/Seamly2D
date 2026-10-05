//---------------------------------------------------------------------------------------------------------------------
//  @file   seam_editor.h
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

#ifndef SEAM_EDITOR_H
#define SEAM_EDITOR_H

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"
#include "../vgarment/piece_outline.h"
#include "../vgarment/seam_stretch.h"
#include "seam_geometry.h"

/// @brief Shows the seams on the board of pieces and lets them be sewn with the mouse, for the scene's QML.
///
/// Sewing works like segment sewing elsewhere: click a segment of a piece's seam line near the end where the seam
/// starts, then the segment it is sewn to near the end that meets that start. Lines join the places that meet, so a
/// twisted seam shows as crossing lines. Positions come from QML in the board's coordinates: cm, y up.
///
/// The editor only proposes seams (seamSewn); they are made through the undo stack and come back with setSeams().
class SeamEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sewing READ isSewing NOTIFY sewingChanged)
    Q_PROPERTY(int selectedSeam READ selectedSeam NOTIFY selectedSeamChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)
    Q_PROPERTY(QObject* seamBands READ seamBands CONSTANT)
    Q_PROPERTY(QObject* seamLines READ seamLines CONSTANT)
    Q_PROPERTY(QObject* previewBands READ previewBands CONSTANT)
    Q_PROPERTY(QObject* previewLines READ previewLines CONSTANT)

public:
    struct Piece
    {
        quint32      id = 0;
        PieceOutline outline;
    };

    explicit             SeamEditor(QObject* parent = nullptr);

    void                 setPieces(const QVector<Piece>& pieces);
    void                 setSeams(const QVector<VSeam>& seams);
    void                 setHighlightColor(const QColor& color);

    bool                 isSewing() const;
    void                 setSewing(bool sewing);
    int                  selectedSeam() const;
    void                 setSelectedSeam(int index);
    QString              hint() const;
    bool                 cancel();

    QObject*             seamBands() const;
    QObject*             seamLines() const;
    QObject*             previewBands() const;
    QObject*             previewLines() const;

    Q_INVOKABLE void     hover(qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void     leave();
    Q_INVOKABLE bool     click(qreal x, qreal y, qreal tolerance);

signals:
    void                 sewingChanged();
    void                 selectedSeamChanged();
    void                 hintChanged();
    void                 seamSewn(const VSeam& seam);

private:
    Q_DISABLE_COPY(SeamEditor)

    // A segment of a shown piece, and which of its ends the seam starts at.
    struct Edge
    {
        quint32 piece_id = 0;
        int     segment = -1;
        bool    from_start = true;

        bool    isValid() const;
        bool    sameSegment(const Edge& other) const;
    };

    // A seam that can be drawn, its second side turned to run like the first.
    struct ShownSeam
    {
        int         index = -1;
        SeamStretch first;
        SeamStretch second;
    };

    QVector<Piece>       m_pieces;
    QVector<VSeam>       m_seams;
    QVector<ShownSeam>   m_shown_seams;
    QColor               m_highlight_color;
    bool                 m_sewing;
    int                  m_selected_seam;
    Edge                 m_started;
    Edge                 m_hovered;
    SeamGeometry*        m_seam_bands;
    SeamGeometry*        m_seam_lines;
    SeamGeometry*        m_preview_bands;
    SeamGeometry*        m_preview_lines;

    const PieceOutline*  outline(quint32 piece_id) const;
    Edge                 edgeAt(const QPointF& point, qreal tolerance) const;
    SeamStretch          edgeStretch(const Edge& edge) const;
    VSeam                seamBetween(const Edge& first, const Edge& second) const;
    bool                 seamStretches(const VSeam& seam, SeamStretch* first, SeamStretch* second) const;
    bool                 knowsSeam(const VSeam& seam) const;
    void                 updateShownSeams();
    void                 updateSeamGeometry();
    void                 updatePreview();
    void                 setStarted(const Edge& edge);

    static QColor        seamColor(int index);
};

#endif // SEAM_EDITOR_H
