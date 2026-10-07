//---------------------------------------------------------------------------------------------------------------------
//  @file   avatar_dialog.cpp
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

#include "avatar_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "../vgarment/standard_sizes.h"

namespace
{
// How tall and how round, in cm, an avatar can be asked to be.
const qreal shortest = 100.0;
const qreal tallest = 220.0;
const qreal thinnest = 40.0;
const qreal roundest = 200.0;
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
AvatarDialog::AvatarDialog(const VGarmentAvatar& avatar, Unit unit, QWidget* parent)
    : QDialog(parent)
    , m_unit(unit)
    , m_body_box(new QComboBox(this))
    , m_size_box(new QComboBox(this))
    , m_height_box(lengthBox(shortest, tallest))
    , m_bust_label(new QLabel(this))
    , m_bust_box(lengthBox(thinnest, roundest))
    , m_waist_box(lengthBox(thinnest, roundest))
    , m_hip_box(lengthBox(thinnest, roundest))
{
    setWindowTitle(tr("Avatar"));

    m_body_box->addItem(tr("Woman"), false);
    m_body_box->addItem(tr("Man"), true);
    m_body_box->setCurrentIndex(avatar.male ? 1 : 0);
    fillSizes(avatar.male, avatar.size);
    m_bust_label->setText(avatar.male ? tr("Chest:") : tr("Bust:"));
    // Wide enough for either, so the form doesn't shift when the body changes.
    m_bust_label->setMinimumWidth(qMax(m_bust_label->fontMetrics().horizontalAdvance(tr("Bust:")),
                                       m_bust_label->fontMetrics().horizontalAdvance(tr("Chest:"))));

    BodyMeasurements measurements;
    measurements.height = avatar.height;
    measurements.bust = avatar.bust;
    measurements.waist = avatar.waist;
    measurements.hip = avatar.hip;
    showMeasurements(measurements);

    QLabel* note = new QLabel(tr("The pattern has no measurements, so the 3D View shows the garment on this body. A "
                                 "size fills in the usual measurements of European size charts; change them to the "
                                 "body you want."), this);
    note->setWordWrap(true);

    QFormLayout* form = new QFormLayout();
    form->addRow(tr("Body:"), m_body_box);
    form->addRow(tr("Size:"), m_size_box);
    form->addRow(tr("Height:"), m_height_box);
    form->addRow(m_bust_label, m_bust_box);
    form->addRow(tr("Waist:"), m_waist_box);
    form->addRow(tr("Hip:"), m_hip_box);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_body_box, QOverload<int>::of(&QComboBox::activated), this, &AvatarDialog::chooseBody);
    connect(m_size_box, QOverload<int>::of(&QComboBox::activated), this, &AvatarDialog::chooseSize);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The avatar as asked for, its measurements in cm.
VGarmentAvatar AvatarDialog::avatar() const
{
    VGarmentAvatar chosen;
    chosen.male = m_body_box->currentData().toBool();
    chosen.size = m_size_box->currentData().toInt();
    chosen.height = UnitConvertor(m_height_box->value(), m_unit, Unit::Cm);
    chosen.bust = UnitConvertor(m_bust_box->value(), m_unit, Unit::Cm);
    chosen.waist = UnitConvertor(m_waist_box->value(), m_unit, Unit::Cm);
    chosen.hip = UnitConvertor(m_hip_box->value(), m_unit, Unit::Cm);
    return chosen;
}

//---------------------------------------------------------------------------------------------------------------------
// A woman or a man was picked: the middle size of theirs, and its measurements.
void AvatarDialog::chooseBody()
{
    const bool male = m_body_box->currentData().toBool();
    fillSizes(male, StandardSizes::defaultSize(male));
    m_bust_label->setText(male ? tr("Chest:") : tr("Bust:"));
    chooseSize();
}

//---------------------------------------------------------------------------------------------------------------------
// A size was picked: its measurements.
void AvatarDialog::chooseSize()
{
    showMeasurements(StandardSizes::of(m_body_box->currentData().toBool(), m_size_box->currentData().toInt())
                         .measurements);
}

//---------------------------------------------------------------------------------------------------------------------
// A box for a length of the body, between the given ones in cm, in the pattern's unit.
QDoubleSpinBox* AvatarDialog::lengthBox(qreal smallest, qreal largest)
{
    QDoubleSpinBox* box = new QDoubleSpinBox(this);
    box->setDecimals(m_unit == Unit::Mm ? 0 : 1);
    box->setRange(UnitConvertor(smallest, Unit::Cm, m_unit), UnitConvertor(largest, Unit::Cm, m_unit));
    box->setSuffix(QLatin1Char(' ') + UnitsToStr(m_unit, true));
    return box;
}

//---------------------------------------------------------------------------------------------------------------------
// Offers a woman's or a man's sizes, with the given one, or the nearest one offered, picked.
void AvatarDialog::fillSizes(bool male, int size)
{
    const QSignalBlocker blocker(m_size_box);
    m_size_box->clear();
    for (const StandardSize& offered : male ? StandardSizes::men() : StandardSizes::women())
    {
        m_size_box->addItem(QString::number(offered.size), offered.size);
    }
    m_size_box->setCurrentIndex(qMax(0, m_size_box->findData(StandardSizes::of(male, size).size)));
}

//---------------------------------------------------------------------------------------------------------------------
// Shows the measurements, given in cm, in the pattern's unit.
void AvatarDialog::showMeasurements(const BodyMeasurements& measurements)
{
    m_height_box->setValue(UnitConvertor(measurements.height, Unit::Cm, m_unit));
    m_bust_box->setValue(UnitConvertor(measurements.bust, Unit::Cm, m_unit));
    m_waist_box->setValue(UnitConvertor(measurements.waist, Unit::Cm, m_unit));
    m_hip_box->setValue(UnitConvertor(measurements.hip, Unit::Cm, m_unit));
}
