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
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVector3D>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"
#include "../vmisc/def.h"
#include "../vgarment/garment_symmetry.h"
#include "../vgarment/seam_stretch.h"
#include "seam_geometry.h"
#include "shown_piece.h"

/// @brief Shows the seams on the pieces, on the board and on the avatar, and lets them be sewn with the mouse, for the
/// scene's QML.
///
/// Sewing works like segment sewing elsewhere: click a segment of a piece's seam line near the end where the seam
/// starts, then the segment it is sewn to near the end that meets that start. As CLO's M:N sewing, either side can go
/// on over several segments, of one piece or more, one after the other: Shift+click each but the last near the end the
/// side comes in at, and click the last. The other side is eased onto all of them evenly, so the longer one gathers, as
/// a skirt's panels into a waistband. Each seam shows in a color of its own
/// along both its sides, and lines join the places that meet, so a twisted seam shows as crossing lines. On the avatar
/// the seams follow the drape, a seam of pieces made up on both sides of the body shows on both, and the lines shrink
/// away as the seams close. Positions come from QML in the board's coordinates, cm with y up, or, on the avatar, in the
/// flat coordinates of the mesh clicked.
///
/// A piece cut twice has a mirror image; sewing one of its segments to itself sews it to the mirror image, as a centre
/// back seam. A side going on over several segments takes the one clicked, on the piece or its mirror image, so a
/// waistband can go all around the body.
///
/// Both sides of a seam should be as long as each other, or the longer eased in on purpose: the hints say how long they
/// are, and the seams can be labelled with how much they differ, in the pattern's unit.
///
/// The editor only proposes seams (seamSewn); they are made through the undo stack and come back with setSeams().
class SeamEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sewing READ isSewing NOTIFY sewingChanged)
    Q_PROPERTY(int selectedSeam READ selectedSeam NOTIFY selectedSeamChanged)
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged)
    Q_PROPERTY(bool garmentSeamsShown READ isGarmentSeamsShown NOTIFY garmentSeamsShownChanged)
    Q_PROPERTY(bool lengthsShown READ isLengthsShown NOTIFY lengthsShownChanged)
    Q_PROPERTY(QVariantList lengthLabels READ lengthLabels NOTIFY lengthLabelsChanged)
    Q_PROPERTY(QObject* seamBands READ seamBands CONSTANT)
    Q_PROPERTY(QObject* seamLines READ seamLines CONSTANT)
    Q_PROPERTY(QObject* previewBands READ previewBands CONSTANT)
    Q_PROPERTY(QObject* previewLines READ previewLines CONSTANT)
    Q_PROPERTY(QObject* garmentSeams READ garmentSeams CONSTANT)
    Q_PROPERTY(QObject* garmentLines READ garmentLines CONSTANT)
    Q_PROPERTY(QObject* garmentPreview READ garmentPreview CONSTANT)
    Q_PROPERTY(QObject* garmentPreviewLines READ garmentPreviewLines CONSTANT)

public:
    explicit             SeamEditor(QObject* parent = nullptr);

    void                 setPieces(const QVector<ShownPiece>& pieces);
    void                 setPositions(const QHash<quint32, QVector<QVector3D>>& positions);
    void                 setSeams(const QVector<VSeam>& seams);
    void                 setHighlightColor(const QColor& color);
    void                 setUnit(Unit unit);

    bool                 isSewing() const;
    void                 setSewing(bool sewing);
    int                  selectedSeam() const;
    void                 setSelectedSeam(int index);
    QString              hint() const;
    bool                 cancel();

    bool                 isGarmentSeamsShown() const;
    void                 setGarmentSeamsShown(bool shown);
    bool                 isLengthsShown() const;
    void                 setLengthsShown(bool shown);
    QVariantList         lengthLabels() const;

    QObject*             seamBands() const;
    QObject*             seamLines() const;
    QObject*             previewBands() const;
    QObject*             previewLines() const;
    QObject*             garmentSeams() const;
    QObject*             garmentLines() const;
    QObject*             garmentPreview() const;
    QObject*             garmentPreviewLines() const;

    Q_INVOKABLE void     hover(qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void     hoverPiece(int id, qreal x, qreal y, qreal tolerance);
    Q_INVOKABLE void     leave();
    Q_INVOKABLE bool     click(qreal x, qreal y, qreal tolerance, bool more = false);
    Q_INVOKABLE bool     clickPiece(int id, qreal x, qreal y, qreal tolerance, bool more = false);

signals:
    void                 sewingChanged();
    void                 selectedSeamChanged();
    void                 hintChanged();
    void                 garmentSeamsShownChanged();
    void                 lengthsShownChanged();
    void                 lengthLabelsChanged();
    void                 seamSewn(const VSeam& seam);

private:
    Q_DISABLE_COPY(SeamEditor)

    // A segment of a piece as drafted, the piece or its mirror image it was picked on, by the scene's id, and which of
    // its ends the seam starts at.
    struct Edge
    {
        quint32 piece_id = 0;
        quint32 shown_id = 0;
        int     segment = -1;
        bool    from_start = true;

        bool    isValid() const;
        bool    sameSegment(const Edge& other) const;
        bool    sameStretch(const Edge& other) const;
    };

    // A side of a seam on the pieces as drafted: its stretches one after the other, each with the piece it is on, and
    // all of them joined into one.
    struct DraftedSide
    {
        QVector<quint32>     pieces;
        QVector<SeamStretch> stretches;
        SeamStretch          joined;

        QPointF              pointAt(qreal distance, quint32* piece) const;
        void                 reverse();
    };

    // A seam of pieces the scene shows, with its sides on the pieces as drafted, the second turned to run like the
    // first.
    struct ShownSeam
    {
        int         index = -1;
        DraftedSide first;
        DraftedSide second;
    };

    // A stretch of seam line on a piece on the avatar, as the piece's mesh has it, with its vertices where they are
    // now.
    struct PlacedStretch
    {
        SeamStretch        stretch;
        QVector<QVector3D> points;

        bool               isEmpty() const;
        void               reverse();
        QVector3D          pointAt(qreal distance) const;
        QVector<QVector3D> startMark() const;
    };

    QVector<ShownPiece>  m_pieces;
    QHash<quint32, QVector<QVector3D>> m_positions;
    GarmentSymmetry      m_symmetry;
    QVector<VSeam>       m_seams;
    QVector<ShownSeam>   m_shown_seams;
    QColor               m_highlight_color;
    Unit                 m_unit;
    bool                 m_lengths_shown;
    QVariantList         m_board_labels;
    QVariantList         m_garment_labels;
    bool                 m_sewing;
    int                  m_selected_seam;
    bool                 m_garment_shown;
    QVector<Edge>        m_first_edges;   // the first side of the seam being sewn, as far as it is picked
    QVector<Edge>        m_second_edges;  // the second side, once the first is
    bool                 m_first_done;
    Edge                 m_hovered;
    SeamGeometry*        m_seam_bands;
    SeamGeometry*        m_seam_lines;
    SeamGeometry*        m_preview_bands;
    SeamGeometry*        m_preview_lines;
    SeamGeometry*        m_garment_seams;
    SeamGeometry*        m_garment_lines;
    SeamGeometry*        m_garment_preview;
    SeamGeometry*        m_garment_preview_lines;

    const ShownPiece*    piece(quint32 piece_id) const;
    const ShownPiece*    pieceShowing(quint32 id, const ShownMesh** shown) const;
    bool                 isOnBoard(quint32 piece_id) const;
    bool                 isMirrored(quint32 piece_id) const;
    Edge                 edgeAt(const QPointF& point, qreal tolerance) const;
    Edge                 edgeOn(quint32 id, const QPointF& point, qreal tolerance) const;
    SeamStretch          edgeStretch(const Edge& edge) const;
    SeamStretch          edgesStretch(const QVector<Edge>& edges) const;
    VSeam                seamBetween(const Edge& first, const Edge& second) const;
    VSeam                seamOver(const QVector<Edge>& first, const QVector<Edge>& second) const;
    bool                 draftedSide(const QVector<VSeamSide>& side, DraftedSide* drafted) const;
    bool                 seamStretches(const VSeam& seam, DraftedSide* first, DraftedSide* second) const;
    bool                 knowsSeam(const VSeam& seam) const;
    int                  seamNear(const QPointF& point, qreal tolerance, quint32 piece_id) const;
    QVector<GarmentSeam> garmentSeams(const VSeam& seam) const;
    PlacedStretch        placedStretch(const GarmentSeamSide& side) const;
    bool                 placedSide(const QVector<GarmentSeamSide>& side, QVector<PlacedStretch>* stretches,
                                    PlacedStretch* joined) const;
    QVector<Edge>        secondEdgesShown() const;
    void                 sewEdge(const Edge& edge, bool more);
    void                 hoverEdge(const Edge& edge);
    void                 updateShownSeams();
    void                 updateSeamGeometry();
    void                 updateGarmentGeometry();
    void                 updatePreview();
    void                 startOver();
    QString              length(qreal cm) const;
    QString              lengthsHint(const SeamStretch& first, const SeamStretch& second) const;
    QVariantMap          lengthLabel(const ShownSeam& seam, const QVector3D& position, bool on_board) const;

    static QColor        seamColor(int index);
};

GarmentSeam toGarmentSeam(const VSeam& seam);

#endif // SEAM_EDITOR_H
