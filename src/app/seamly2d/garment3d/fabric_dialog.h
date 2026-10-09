//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_dialog.h
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

#ifndef FABRIC_DIALOG_H
#define FABRIC_DIALOG_H

#include <QDialog>
#include <QStringList>
#include <QVector>

#include "../ifc/xml/vabstractpattern.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// @brief Asks for a fabric of the pattern's own, as CLO's fabric properties: its name, how heavy and how thick it is,
/// how hard it is to stretch along its grain, across it and on the bias, and how stiffly it bends along its grain and
/// across it, starting from a fabric like it if wanted. One the pattern keeps can also be deleted.
class FabricDialog : public QDialog
{
    Q_OBJECT
public:
    /// A fabric to start from: what it is called and its values.
    struct Start
    {
        QString       title;
        VCustomFabric values;
    };

    explicit           FabricDialog(const VCustomFabric& fabric, const QVector<Start>& starts,
                                    const QStringList& taken, bool kept, QWidget* parent = nullptr);
    virtual           ~FabricDialog() = default;

    VCustomFabric      fabric() const;
    bool               isDeleted() const;

private slots:
    void               chooseStart(int index);
    void               checkName();
    void               deleteFabric();

private:
    Q_DISABLE_COPY(FabricDialog)

    QVector<Start>     m_starts;
    QStringList        m_taken;
    bool               m_deleted;
    QLineEdit*         m_name_edit;
    QComboBox*         m_start_box;
    QDoubleSpinBox*    m_weight_box;
    QDoubleSpinBox*    m_thickness_box;
    QDoubleSpinBox*    m_warp_box;
    QDoubleSpinBox*    m_weft_box;
    QDoubleSpinBox*    m_bias_box;
    QDoubleSpinBox*    m_bending_warp_box;
    QDoubleSpinBox*    m_bending_weft_box;
    QLabel*            m_name_note;
    QPushButton*       m_ok_button;

    QDoubleSpinBox*    valueBox(const QString& name, qreal least, qreal most, int decimals, const QString& unit,
                                const QString& tip);
    void               showValues(const VCustomFabric& fabric);
};

#endif // FABRIC_DIALOG_H
