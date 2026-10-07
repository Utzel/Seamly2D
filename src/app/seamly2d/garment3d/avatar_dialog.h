//---------------------------------------------------------------------------------------------------------------------
//  @file   avatar_dialog.h
//  @author Julius
//  @date   7 Oct, 2026
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

#ifndef AVATAR_DIALOG_H
#define AVATAR_DIALOG_H

#include <QDialog>

#include "../ifc/xml/vabstractpattern.h"
#include "../vmisc/def.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
struct BodyMeasurements;

/// @brief Asks for the avatar of a pattern without measurements: a woman or a man, a European clothing size, and the
/// height, bust or chest, waist and hip, which the size fills in and which can be changed from there, in the pattern's
/// unit.
class AvatarDialog : public QDialog
{
    Q_OBJECT
public:
    explicit           AvatarDialog(const VGarmentAvatar& avatar, Unit unit, QWidget* parent = nullptr);
    virtual           ~AvatarDialog() = default;

    VGarmentAvatar     avatar() const;

private slots:
    void               chooseBody();
    void               chooseSize();

private:
    Q_DISABLE_COPY(AvatarDialog)

    Unit               m_unit;
    QComboBox*         m_body_box;
    QComboBox*         m_size_box;
    QDoubleSpinBox*    m_height_box;
    QLabel*            m_bust_label;
    QDoubleSpinBox*    m_bust_box;
    QDoubleSpinBox*    m_waist_box;
    QDoubleSpinBox*    m_hip_box;

    QDoubleSpinBox*    lengthBox(qreal smallest, qreal largest);
    void               fillSizes(bool male, int size);
    void               showMeasurements(const BodyMeasurements& measurements);
};

#endif // AVATAR_DIALOG_H
