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

#include "ProfileEditor5.hpp"
#include "ui_ProfileEditor5.h"
#include <Core/Enum/DSType.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Language.hpp>
#include <Core/Gen5/IVCache.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/Gen5/States/ProfileSearcherState5.hpp>
#include <Core/Gen5/WhiteForestData.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Gen5/Profile/ProfileCalibrator5.hpp>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QLabel>

ProfileEditor5::ProfileEditor5(QWidget *parent) : QDialog(parent), ui(new Ui::ProfileEditor5)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    ui->textBoxTID->setValues(InputType::TIDSID);
    ui->textBoxSID->setValues(InputType::TIDSID);
    ui->textBoxMAC->setValues(0, 0xFFFFFFFFFFFF, 12, 16);
    ui->textBoxVCount->setValues(0, 0xFF, 2, 16);
    ui->textBoxGxStat->setValues(0, 99, 2, 16);
    ui->textBoxVFrame->setValues(0, 99, 2, 16);
    ui->textBoxTimer0Min->setValues(InputType::Seed16Bit);
    ui->textBoxTimer0Max->setValues(InputType::Seed16Bit);

    ui->comboBoxVersion->setup({ toInt(Game::Black), toInt(Game::White), toInt(Game::Black2), toInt(Game::White2) });

    ui->comboBoxDSType->setup({ toInt(DSType::DS), toInt(DSType::DSi), toInt(DSType::DS3) });

    ui->comboBoxLanguage->setup({ toInt(Language::English), toInt(Language::Spanish), toInt(Language::French), toInt(Language::Italian),
                                  toInt(Language::German), toInt(Language::Japanese), toInt(Language::Korean) });

    residentButton = new QPushButton(tr("Residents"), this);
    residentInfoButton = new QPushButton(tr("Resident Info"), this);
    ui->gridLayout->addWidget(residentButton, 0, 7);
    ui->gridLayout->addWidget(residentInfoButton, 1, 7);

    connect(ui->pushButtonOkay, &QPushButton::clicked, this, &ProfileEditor5::okay);
    connect(ui->pushButtonCancel, &QPushButton::clicked, this, &ProfileEditor5::reject);
    connect(ui->pushButtonFindParameters, &QPushButton::clicked, this, &ProfileEditor5::findParameters);
    connect(ui->comboBoxVersion, &QComboBox::currentIndexChanged, this, &ProfileEditor5::versionIndexChanged);
    connect(ui->pushButtonSelectIVCache, &QPushButton::clicked, this, &ProfileEditor5::selectIVCache);
    connect(ui->pushButtonSelectSHACache, &QPushButton::clicked, this, &ProfileEditor5::selectSHACache);
    connect(ui->pushButtonClearIVCache, &QPushButton::clicked, this, &ProfileEditor5::clearIVCache);
    connect(ui->pushButtonClearSHACache, &QPushButton::clicked, this, &ProfileEditor5::clearSHACache);
    connect(residentButton, &QPushButton::clicked, this, &ProfileEditor5::editWhiteForestResidents);
    connect(residentInfoButton, &QPushButton::clicked, this, &ProfileEditor5::showWhiteForestResidentInfo);
    connect(ui->checkBoxMemoryLink, &QCheckBox::toggled, this, [this](bool checked) {
        ui->checkBoxNsPokemonReleased->setEnabled(checked);
        if (!checked)
        {
            ui->checkBoxNsPokemonReleased->setChecked(false);
        }
    });
    connect(ui->checkBoxOvalCharm, &QCheckBox::toggled, this, [this](bool checked) {
        ui->checkBoxShinyCharm->setEnabled(checked);
        if (!checked)
        {
            ui->checkBoxShinyCharm->setChecked(false);
        }
    });

    versionIndexChanged(ui->comboBoxVersion->currentIndex());
}

ProfileEditor5::ProfileEditor5(const Profile5 &profile, QWidget *parent) : ProfileEditor5(parent)
{
    ui->lineEditProfile->setText(QString::fromStdString(profile.getName()));
    ui->textBoxTID->setText(QString::number(profile.getTID()));
    ui->textBoxSID->setText(QString::number(profile.getSID()));
    ui->lineEditIVCache->setText(QString::fromStdString(profile.getIVCache()));
    ui->lineEditSHACache->setText(QString::fromStdString(profile.getSHACache()));
    ui->textBoxMAC->setText(QString::number(profile.getMac(), 16));
    ui->textBoxVCount->setText(QString::number(profile.getVCount(), 16));
    ui->textBoxGxStat->setText(QString::number(profile.getGxStat(), 16));
    ui->textBoxVFrame->setText(QString::number(profile.getVFrame(), 16));
    ui->textBoxTimer0Min->setText(QString::number(profile.getTimer0Min(), 16));
    ui->textBoxTimer0Max->setText(QString::number(profile.getTimer0Max(), 16));

    ui->comboBoxVersion->setCurrentIndex(ui->comboBoxVersion->findData(toInt(profile.getVersion())));
    ui->comboBoxLanguage->setCurrentIndex(ui->comboBoxLanguage->findData(toInt(profile.getLanguage())));
    ui->comboBoxDSType->setCurrentIndex(ui->comboBoxDSType->findData(toInt(profile.getDSType())));

    ui->comboBoxKeypresses->setChecks(profile.getKeypresses());

    ui->checkBoxSkipLR->setChecked(profile.getSkipLR());
    ui->checkBoxMemoryLink->setChecked(profile.getMemoryLink());
    ui->checkBoxNsPokemonReleased->setChecked(profile.getMemoryLink() && profile.getNsPokemonReleased());
    ui->checkBoxOvalCharm->setChecked(profile.getOvalCharm());
    ui->checkBoxShinyCharm->setChecked(profile.getShinyCharm());
    whiteForestResidents = profile.getWhiteForestResidents();
}

ProfileEditor5::ProfileEditor5(Game version, Language language, DSType dsType, u64 mac, const ProfileSearcherState5 &calibration,
                               QWidget *parent) :
    ProfileEditor5(parent)
{
    ui->comboBoxVersion->setCurrentIndex(ui->comboBoxVersion->findData(toInt(version)));
    ui->comboBoxLanguage->setCurrentIndex(ui->comboBoxLanguage->findData(toInt(language)));
    ui->comboBoxDSType->setCurrentIndex(ui->comboBoxDSType->findData(toInt(dsType)));
    ui->textBoxMAC->setText(QString::number(mac, 16));
    ui->textBoxVCount->setText(QString::number(calibration.getVCount(), 16));
    ui->textBoxTimer0Min->setText(QString::number(calibration.getTimer0(), 16));
    ui->textBoxTimer0Max->setText(QString::number(calibration.getTimer0(), 16));
    ui->textBoxGxStat->setText(QString::number(calibration.getGxStat(), 16));
    ui->textBoxVFrame->setText(QString::number(calibration.getVFrame(), 16));
}

ProfileEditor5::~ProfileEditor5()
{
    delete ui;
}

Profile5 ProfileEditor5::getProfile()
{
    return Profile5(ui->lineEditProfile->text().toStdString(), ui->comboBoxVersion->getEnum<Game>(), ui->textBoxTID->getUShort(),
                    ui->textBoxSID->getUShort(), ui->lineEditIVCache->text().toStdString(), ui->lineEditSHACache->text().toStdString(),
                    ui->textBoxMAC->getULong(), ui->comboBoxKeypresses->getCheckedArray<9>(), ui->textBoxVCount->getUChar(),
                    ui->textBoxGxStat->getUChar(), ui->textBoxVFrame->getUChar(), ui->checkBoxSkipLR->isChecked(),
                    ui->textBoxTimer0Min->getUShort(), ui->textBoxTimer0Max->getUShort(), ui->checkBoxMemoryLink->isChecked(),
                    ui->checkBoxShinyCharm->isChecked(), ui->comboBoxDSType->getEnum<DSType>(), ui->comboBoxLanguage->getEnum<Language>(),
                    ui->checkBoxMemoryLink->isChecked() && ui->checkBoxNsPokemonReleased->isChecked(), ui->checkBoxOvalCharm->isChecked(),
                    whiteForestResidents);
}

void ProfileEditor5::editWhiteForestResidents()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("White Forest Residents"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *residentLayout = new QGridLayout;
    std::array<QComboBox *, 10> inputs;

    for (u8 residentSlot = 0; residentSlot < inputs.size(); residentSlot++)
    {
        residentLayout->addWidget(new QLabel(tr("Slot %1").arg(residentSlot + 1), &dialog), residentSlot / 5,
                                  (residentSlot % 5) * 2);
        inputs[residentSlot] = new QComboBox(&dialog);
        inputs[residentSlot]->addItem(tr("None"), 255);
        for (u8 resident : WhiteForest5::residentNameOrder)
            inputs[residentSlot]->addItem(QString::fromUtf8(WhiteForest5::residentNames[resident]), resident);
        inputs[residentSlot]->setCurrentIndex(inputs[residentSlot]->findData(whiteForestResidents[residentSlot]));
        residentLayout->addWidget(inputs[residentSlot], residentSlot / 5, (residentSlot % 5) * 2 + 1);
    }

    const auto updateResidentChoices = [&] {
        std::array<u8, 10> selected;
        for (u8 residentSlot = 0; residentSlot < selected.size(); residentSlot++)
            selected[residentSlot] = inputs[residentSlot]->currentData().toUInt();

        for (u8 residentSlot = 0; residentSlot < inputs.size(); residentSlot++)
        {
            const QSignalBlocker blocker(inputs[residentSlot]);
            inputs[residentSlot]->clear();
            inputs[residentSlot]->addItem(tr("None"), 255);
            for (u8 resident : WhiteForest5::residentNameOrder)
            {
                bool selectedElsewhere = false;
                for (u8 otherSlot = 0; otherSlot < selected.size(); otherSlot++)
                {
                    if (otherSlot != residentSlot && selected[otherSlot] == resident)
                    {
                        selectedElsewhere = true;
                        break;
                    }
                }
                if (!selectedElsewhere)
                    inputs[residentSlot]->addItem(QString::fromUtf8(WhiteForest5::residentNames[resident]), resident);
            }
            inputs[residentSlot]->setCurrentIndex(inputs[residentSlot]->findData(selected[residentSlot]));
        }
    };

    const auto updateResidentEnabledState = [&] {
        bool previousOccupied = true;
        for (auto *input : inputs)
        {
            input->setEnabled(previousOccupied);
            previousOccupied = previousOccupied && input->currentData().toUInt() != 255;
        }
    };

    for (auto *input : inputs)
    {
        connect(input, &QComboBox::currentIndexChanged, &dialog, [&] {
            updateResidentEnabledState();
            updateResidentChoices();
        });
    }
    updateResidentEnabledState();
    updateResidentChoices();
    layout->addLayout(residentLayout);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    for (u8 residentSlot = 0; residentSlot < whiteForestResidents.size(); residentSlot++)
        whiteForestResidents[residentSlot] = inputs[residentSlot]->currentData().toUInt();
}

void ProfileEditor5::showWhiteForestResidentInfo()
{
    std::array<bool, 30> selectedResidents {};
    for (u8 resident : whiteForestResidents)
        if (resident < selectedResidents.size()) selectedResidents[resident] = true;

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Resident Info"));
    auto *layout = new QVBoxLayout(dialog);
    auto *table = new QTableWidget(static_cast<int>(WhiteForest5::residents.size()), 7, dialog);
    table->setHorizontalHeaderLabels(
        { tr("Resident"), tr("Grass Pokémon"), tr("Weight"), tr("Surfing Pokémon"), tr("Weight"), tr("Item"), tr("Item Odds") });

    QSettings settings;
    const bool markEnabled = settings.value(QStringLiteral("settings/targetMarkEnabled"), true).toBool();
    const QColor mark = settings.value(QStringLiteral("settings/targetMarkColor"), QColor(Qt::red)).value<QColor>();
    const int alpha = settings.value(QStringLiteral("settings/targetMarkAlpha"), 128).toInt();
    const QColor base = table->palette().color(QPalette::Base);
    auto blend = [alpha](int foreground, int background) {
        return (foreground * alpha + background * (255 - alpha)) / 255;
    };
    const QColor markedBackground(blend(mark.red(), base.red()), blend(mark.green(), base.green()), blend(mark.blue(), base.blue()));

    for (u8 row = 0; row < WhiteForest5::residentNameOrder.size(); row++)
    {
        const u8 resident = WhiteForest5::residentNameOrder[row];
        const auto &data = WhiteForest5::residents[resident];
        const QStringList values = { QString::fromUtf8(WhiteForest5::residentNames[resident]),
                                     QString::fromStdString(Translator::getSpecie(data.grassSpecies)),
                                     QString::number(data.grassWeight),
                                     QString::fromStdString(Translator::getSpecie(data.waterSpecies)),
                                     QString::number(data.waterWeight), QString::fromStdString(Translator::getItem(data.item)),
                                     tr("%1%").arg(data.itemChance) };
        for (int column = 0; column < values.size(); column++)
        {
            auto *item = new QTableWidgetItem(values[column]);
            if (markEnabled && selectedResidents[resident]) item->setBackground(markedBackground);
            table->setItem(row, column, item);
        }
    }
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->resizeColumnsToContents();
    layout->addWidget(table);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog->resize(850, 350);
    dialog->show();
}

void ProfileEditor5::clearIVCache()
{
    ui->lineEditIVCache->clear();
    ui->lineEditSHACache->clear();
}

void ProfileEditor5::clearSHACache()
{
    ui->lineEditSHACache->clear();
}

void ProfileEditor5::findParameters()
{
    auto *calibrator = new ProfileCalibrator5();
    calibrator->show();

    done(QDialog::Rejected);
}

void ProfileEditor5::okay()
{
    QString input = ui->lineEditProfile->text().trimmed();
    if (input.isEmpty())
    {
        QMessageBox msg(QMessageBox::Warning, tr("Missing name"), tr("Enter a profile name"));
        msg.exec();
        return;
    }
    if (ui->comboBoxVersion->getEnum<Game>() == Game::White)
    {
        std::array<bool, 30> used {};
        for (u8 resident : whiteForestResidents)
        {
            if (resident < used.size() && used[resident])
            {
                QMessageBox msg(QMessageBox::Warning, tr("Duplicate resident"), tr("Each White Forest resident can only occupy one slot."));
                msg.exec();
                return;
            }
            if (resident < used.size()) used[resident] = true;
        }
    }

    done(QDialog::Accepted);
}

void ProfileEditor5::selectIVCache()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open IV Cache"), QDir::currentPath(), "ivcache (*.ivcache)");
    IVCache cache(file.toStdString(), false);
    if (cache.isValid())
    {
        ui->lineEditIVCache->setText(file);
        ui->lineEditSHACache->clear();
    }
    else
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid IV Cache"), tr("Provided file is not a valid IV Cache"));
        msg.exec();
        return;
    }
}

void ProfileEditor5::selectSHACache()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open SHA1 Cache"), QDir::currentPath(), "sha1cache (*.sha1cache)");
    SHA1Cache cache(file.toStdString());
    if (cache.isValid(getProfile()))
    {
        ui->lineEditSHACache->setText(file);
    }
    else
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid SHA Cache"),
                        tr("Provided file is not a valid SHA Cache or was not created from the profile"));
        msg.exec();
        return;
    }
}

void ProfileEditor5::versionIndexChanged(int index)
{
    if (index >= 0)
    {
        Game version = static_cast<Game>(ui->comboBoxVersion->currentData().toUInt());
        residentButton->setVisible(version == Game::White);
        residentInfoButton->setVisible(version == Game::White);
        if ((version & Game::BW2) != Game::None)
        {
            ui->checkBoxMemoryLink->show();
            ui->checkBoxNsPokemonReleased->show();
            ui->checkBoxNsPokemonReleased->setEnabled(ui->checkBoxMemoryLink->isChecked());
            ui->checkBoxOvalCharm->show();
            ui->checkBoxShinyCharm->show();
            ui->checkBoxShinyCharm->setEnabled(ui->checkBoxOvalCharm->isChecked());
        }
        else
        {
            ui->checkBoxMemoryLink->hide();
            ui->checkBoxMemoryLink->setChecked(false);
            ui->checkBoxNsPokemonReleased->hide();
            ui->checkBoxNsPokemonReleased->setChecked(false);
            ui->checkBoxOvalCharm->hide();
            ui->checkBoxOvalCharm->setChecked(false);
            ui->checkBoxShinyCharm->hide();
            ui->checkBoxShinyCharm->setChecked(false);
        }
    }
}
