/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ProfileEditor4.hpp"
#include "ui_ProfileEditor4.h"
#include <Core/Enum/Game.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

namespace
{
constexpr std::array<const char *, 16> plateNames = { "Flame Plate", "Splash Plate", "Zap Plate", "Meadow Plate", "Icicle Plate",
                                                      "Fist Plate",  "Toxic Plate",  "Earth Plate", "Sky Plate",    "Mind Plate",
                                                      "Insect Plate", "Stone Plate", "Spooky Plate", "Draco Plate", "Dread Plate",
                                                      "Iron Plate" };
}

ProfileEditor4::ProfileEditor4(QWidget *parent) : QDialog(parent), ui(new Ui::ProfileEditor4)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    ui->textBoxTID->setValues(InputType::TIDSID);
    ui->textBoxSID->setValues(InputType::TIDSID);

    ui->comboBoxVersion->setup(
        { toInt(Game::Diamond), toInt(Game::Pearl), toInt(Game::Platinum), toInt(Game::HeartGold), toInt(Game::SoulSilver) });

    ui->comboMenuPlates->setMultiSelect(true);
    for (int i = 0; i < 16; i++) ui->comboMenuPlates->addAction(tr(plateNames[i]), i);
    const auto updatePlateText = [this] {
        const size_t checked = ui->comboMenuPlates->getRawCheckedData().size();
        if (checked == 0) ui->comboMenuPlates->setText(tr("None"));
        else if (checked == plateNames.size()) ui->comboMenuPlates->setText(tr("All"));
        else
        {
            QStringList selected;
            for (int plate : ui->comboMenuPlates->getRawCheckedData()) selected.append(tr(plateNames[plate]));
            ui->comboMenuPlates->setText(ui->comboMenuPlates->fontMetrics().elidedText(
                selected.join(QStringLiteral(", ")), Qt::ElideRight, ui->comboMenuPlates->width() - 20));
        }
    };
    connect(ui->comboMenuPlates, &ComboMenu::checkedDataChanged, this, updatePlateText);
    ui->comboMenuPlates->setCheckedData({});
    QTimer::singleShot(0, ui->comboMenuPlates, [this, updatePlateText] {
        ui->comboMenuPlates->setFixedWidth(ui->comboBoxVersion->width());
        updatePlateText();
    });

    for (char letter = 'A'; letter <= 'Z'; letter++)
    {
        ui->checkListUnownDiscovered->addItem(QChar(letter));
    }

    connect(ui->pushButtonOkay, &QPushButton::clicked, this, &ProfileEditor4::okay);
    connect(ui->pushButtonCancel, &QPushButton::clicked, this, &ProfileEditor4::reject);
    connect(ui->comboBoxVersion, &QComboBox::currentIndexChanged, this, &ProfileEditor4::versionIndexChanged);

    versionIndexChanged(ui->comboBoxVersion->currentIndex());

    QSettings setting;
    if (setting.contains("profileEditor4/geometry"))
    {
        this->restoreGeometry(setting.value("profileEditor4/geometry").toByteArray());
    }
}

ProfileEditor4::ProfileEditor4(const Profile4 &profile, QWidget *parent) : ProfileEditor4(parent)
{
    ui->lineEditProfile->setText(QString::fromStdString(profile.getName()));
    ui->comboBoxVersion->setCurrentIndex(ui->comboBoxVersion->findData(toInt(profile.getVersion())));
    ui->textBoxTID->setText(QString::number(profile.getTID()));
    ui->textBoxSID->setText(QString::number(profile.getSID()));
    ui->checkBoxNationalDex->setChecked(profile.getNationalDex());
    ui->checkBoxUndergroundOrb->setChecked(profile.getUndergroundOrb());
    ui->checkListUnownPuzzles->setChecks(profile.getUnownPuzzle());
    ui->checkListUnownDiscovered->setChecks(profile.getUnownDiscovered());
    std::vector<int> plates;
    const auto minedPlates = profile.getMinedPlates();
    for (int i = 0; i < static_cast<int>(minedPlates.size()); i++)
        if (minedPlates[i]) plates.push_back(i);
    ui->comboMenuPlates->setCheckedData(plates);
}

ProfileEditor4::~ProfileEditor4()
{
    QSettings setting;
    setting.setValue("profileEditor4/geometry", this->saveGeometry());

    delete ui;
}

Profile4 ProfileEditor4::getProfile()
{
    return Profile4(ui->lineEditProfile->text().toStdString(), ui->comboBoxVersion->getEnum<Game>(), ui->textBoxTID->getUShort(),
                    ui->textBoxSID->getUShort(), ui->checkBoxNationalDex->isChecked(), ui->checkListUnownDiscovered->getCheckedArray<26>(),
                    ui->checkListUnownPuzzles->getCheckedArray<4>(), [&] {
                        std::array<bool, 16> plates = {};
                        for (int plate : ui->comboMenuPlates->getRawCheckedData()) plates[plate] = true;
                        return plates;
                    }(), ui->checkBoxUndergroundOrb->isChecked());
}

void ProfileEditor4::okay()
{
    QString input = ui->lineEditProfile->text().trimmed();
    if (input.isEmpty())
    {
        QMessageBox msg(QMessageBox::Warning, tr("Missing name"), tr("Enter a profile name"));
        msg.exec();
        return;
    }

    done(QDialog::Accepted);
}

void ProfileEditor4::versionIndexChanged(int index)
{
    if (index >= 0)
    {
        const Game version = ui->comboBoxVersion->getEnum<Game>();
        bool flag = (version & Game::HGSS) != Game::None;
        ui->labelPlates->setVisible(!flag);
        ui->comboMenuPlates->setVisible(!flag);
        const bool diamondPearl = version == Game::Diamond || version == Game::Pearl;
        ui->checkBoxUndergroundOrb->setVisible(diamondPearl);
        ui->checkBoxUndergroundOrb->setText(version == Game::Pearl ? tr("Picked up Lustrous Orb") : tr("Picked up Adamant Orb"));
        ui->labelUnownPuzzles->setVisible(flag);
        ui->checkListUnownPuzzles->setVisible(flag);
        ui->labelUnownDiscovered->setVisible(flag);
        ui->checkListUnownDiscovered->setVisible(flag);
    }
}
