//---------------------------------------------------------------------------------------------------------------------
//  @file   fabric_dialog.cpp
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

#include "fabric_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
// The values a fabric can be given, wide enough for anything from chiffon to canvas: how heavy, in g per square metre;
// how thick, in mm; how hard to stretch, in N/m; and how stiffly it bends, in micro newton metres.
const qreal lightest = 10.0;
const qreal heaviest = 2000.0;
const qreal thinnest = 0.05;
const qreal thickest = 10.0;
const qreal stretchiest = 1.0;
const qreal stiffest_stretch = 20000.0;
const qreal limpest = 0.1;
const qreal stiffest_bending = 1000.0;

//---------------------------------------------------------------------------------------------------------------------
QString gramsPerSquareMetre()
{
    return QStringLiteral("g/m") + QChar(0x00B2);
}

//---------------------------------------------------------------------------------------------------------------------
QString microNewtonMetres()
{
    return QString(QChar(0x00B5)) + QStringLiteral("N") + QChar(0x00B7) + QStringLiteral("m");
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief Asks for the fabric, starting from its values; its name must be none of those taken. One the pattern keeps
/// can be deleted.
FabricDialog::FabricDialog(const VCustomFabric& fabric, const QVector<Start>& starts, const QStringList& taken,
                           bool kept, QWidget* parent)
    : QDialog(parent)
    , m_starts(starts)
    , m_taken()
    , m_deleted(false)
    , m_name_edit(new QLineEdit(fabric.name, this))
    , m_start_box(new QComboBox(this))
    , m_weight_box(valueBox(QStringLiteral("fabricWeight"), lightest, heaviest, 0, gramsPerSquareMetre(),
                            tr("How heavy the fabric is, in grams per square metre: chiffon about 50, shirting 120, "
                               "denim 400")))
    , m_thickness_box(valueBox(QStringLiteral("fabricThickness"), thinnest, thickest, 2, QStringLiteral("mm"),
                               tr("How thick the fabric is, as the 3D View draws it: chiffon about 0.15 mm, "
                                  "shirting 0.3, denim 0.9")))
    , m_warp_box(valueBox(QStringLiteral("fabricWarp"), stretchiest, stiffest_stretch, 0, QStringLiteral("N/m"),
                          tr("How hard the fabric is to stretch along the grain, in newtons per metre of its width: "
                             "wovens about 1000 to 4000, knits 100 to 300. Cloth stiffer than 300 hardly stretches "
                             "under its own weight, and drapes as cloth that stiff does.")))
    , m_weft_box(valueBox(QStringLiteral("fabricWeft"), stretchiest, stiffest_stretch, 0, QStringLiteral("N/m"),
                          tr("How hard the fabric is to stretch across the grain: wovens give a little more there "
                             "than along it, knits often twice as much")))
    , m_bias_box(valueBox(QStringLiteral("fabricBias"), stretchiest, stiffest_stretch, 0, QStringLiteral("N/m"),
                          tr("How hard the fabric is to stretch on the bias, at 45 degrees to the grain: wovens give "
                             "there, about 100 to 300, as their threads only have to turn")))
    , m_bending_warp_box(valueBox(QStringLiteral("fabricBendingWarp"), limpest, stiffest_bending, 1,
                                  microNewtonMetres(),
                                  tr("How stiffly the fabric bends along the grain, as a strip cut along the grain "
                                     "droops, in micro newton metres: chiffon about 0.5, shirting 8, denim 60")))
    , m_bending_weft_box(valueBox(QStringLiteral("fabricBendingWeft"), limpest, stiffest_bending, 1,
                                  microNewtonMetres(),
                                  tr("How stiffly the fabric bends across the grain, as a strip cut across the grain "
                                     "droops")))
    , m_name_note(new QLabel(this))
    , m_ok_button(nullptr)
{
    setWindowTitle(kept ? tr("Edit Fabric") : tr("New Fabric"));
    for (const QString& name : taken)
    {
        m_taken.append(name.trimmed().toCaseFolded());
    }

    m_name_edit->setObjectName(QStringLiteral("fabricName"));
    m_start_box->setObjectName(QStringLiteral("fabricStart"));
    m_start_box->setToolTip(tr("Fill in the values of a fabric like it, then change what differs"));
    for (const Start& start : m_starts)
    {
        m_start_box->addItem(start.title);
    }
    m_start_box->setPlaceholderText(tr("The values below"));
    m_start_box->setCurrentIndex(-1);
    showValues(fabric);

    QLabel* note = new QLabel(tr("A fabric of the pattern's own, as fabric tests describe it. Start from a fabric "
                                 "like it and change what differs."), this);
    note->setWordWrap(true);
    m_name_note->setObjectName(QStringLiteral("fabricNameNote"));

    QFormLayout* form = new QFormLayout();
    form->addRow(tr("Name:"), m_name_edit);
    form->addRow(QString(), m_name_note);
    form->addRow(tr("Start from:"), m_start_box);
    form->addRow(tr("Weight:"), m_weight_box);
    form->addRow(tr("Thickness:"), m_thickness_box);
    form->addRow(tr("Stretch along the grain:"), m_warp_box);
    form->addRow(tr("Stretch across the grain:"), m_weft_box);
    form->addRow(tr("Stretch on the bias:"), m_bias_box);
    form->addRow(tr("Bending along the grain:"), m_bending_warp_box);
    form->addRow(tr("Bending across the grain:"), m_bending_weft_box);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok_button = buttons->button(QDialogButtonBox::Ok);
    if (kept)
    {
        QPushButton* remove = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
        remove->setObjectName(QStringLiteral("fabricDelete"));
        remove->setToolTip(tr("Delete the fabric: the pieces cut from it are cut from the garment's fabric again, and "
                              "the garment from the 3D View's default"));
        connect(remove, &QPushButton::clicked, this, &FabricDialog::deleteFabric);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_name_edit, &QLineEdit::textChanged, this, &FabricDialog::checkName);
    connect(m_start_box, QOverload<int>::of(&QComboBox::activated), this, &FabricDialog::chooseStart);
    checkName();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The fabric as asked for.
VCustomFabric FabricDialog::fabric() const
{
    VCustomFabric fabric;
    fabric.name = m_name_edit->text().trimmed();
    fabric.weight = m_weight_box->value();
    fabric.warp = m_warp_box->value();
    fabric.weft = m_weft_box->value();
    fabric.bias = m_bias_box->value();
    fabric.bending_warp = m_bending_warp_box->value();
    fabric.bending_weft = m_bending_weft_box->value();
    fabric.thickness = m_thickness_box->value();
    return fabric;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Whether the fabric is to be deleted.
bool FabricDialog::isDeleted() const
{
    return m_deleted;
}

//---------------------------------------------------------------------------------------------------------------------
// A fabric to start from was picked: its values.
void FabricDialog::chooseStart(int index)
{
    if (index >= 0 && index < m_starts.size())
    {
        showValues(m_starts.at(index).values);
    }
}

//---------------------------------------------------------------------------------------------------------------------
// The fabric needs a name no other fabric has, whatever its case.
void FabricDialog::checkName()
{
    const QString name = m_name_edit->text().trimmed();
    QString problem;
    if (name.isEmpty())
    {
        problem = tr("The fabric needs a name.");
    }
    else if (m_taken.contains(name.toCaseFolded()))
    {
        problem = tr("Another fabric already has this name.");
    }
    m_name_note->setText(problem);
    m_name_note->setVisible(!problem.isEmpty());
    m_ok_button->setEnabled(problem.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void FabricDialog::deleteFabric()
{
    m_deleted = true;
    accept();
}

//---------------------------------------------------------------------------------------------------------------------
// A box for one of the fabric's values, from least to most, in the unit given.
QDoubleSpinBox* FabricDialog::valueBox(const QString& name, qreal least, qreal most, int decimals,
                                       const QString& unit, const QString& tip)
{
    QDoubleSpinBox* box = new QDoubleSpinBox(this);
    box->setObjectName(name);
    box->setDecimals(decimals);
    box->setRange(least, most);
    box->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    box->setSuffix(QLatin1Char(' ') + unit);
    box->setToolTip(tip);
    return box;
}

//---------------------------------------------------------------------------------------------------------------------
void FabricDialog::showValues(const VCustomFabric& fabric)
{
    m_weight_box->setValue(fabric.weight);
    m_thickness_box->setValue(fabric.thickness);
    m_warp_box->setValue(fabric.warp);
    m_weft_box->setValue(fabric.weft);
    m_bias_box->setValue(fabric.bias);
    m_bending_warp_box->setValue(fabric.bending_warp);
    m_bending_weft_box->setValue(fabric.bending_weft);
}
