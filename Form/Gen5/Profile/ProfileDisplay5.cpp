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

#include "ProfileDisplay5.hpp"
#include "ui_ProfileDisplay5.h"
#include <Core/Enum/Game.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <QSettings>
#include <QSpinBox>

static const QString settingKey = QStringLiteral("%1/profile");

ProfileDisplay5::ProfileDisplay5(QWidget *parent) :
    QWidget(parent), ui(new Ui::ProfileDisplay5), tidInput(nullptr), sidInput(nullptr), idsEditable(true)
{
    ui->setupUi(this);

    tidInput = new QSpinBox(this);
    sidInput = new QSpinBox(this);

    for (QSpinBox *input : { tidInput, sidInput })
    {
        input->setRange(0, 65535);
        input->setButtonSymbols(QAbstractSpinBox::NoButtons);
        input->setFocusPolicy(Qt::StrongFocus);
    }
    ui->gridLayout->addWidget(tidInput, 0, 3);
    ui->gridLayout->addWidget(sidInput, 1, 3);
    ui->labelTIDValue->hide();
    ui->labelSIDValue->hide();
    setFocusProxy(ui->comboBoxProfiles);
    setTabOrder(ui->comboBoxProfiles, tidInput);
    setTabOrder(tidInput, sidInput);
    setTabOrder(sidInput, ui->pushButtonProfileManager);

    connect(ui->comboBoxProfiles, &QComboBox::currentIndexChanged, this, &ProfileDisplay5::profileIndexChanged);
    connect(ui->pushButtonProfileManager, &QPushButton::clicked, this, &ProfileDisplay5::profileManager);
    connect(tidInput, &QSpinBox::editingFinished, this, &ProfileDisplay5::updateIDs);
    connect(sidInput, &QSpinBox::editingFinished, this, &ProfileDisplay5::updateIDs);
}

ProfileDisplay5::~ProfileDisplay5()
{
    QSettings setting;
    setting.setValue(settingKey.arg(prefix), ui->comboBoxProfiles->currentIndex());

    delete ui;
}

bool ProfileDisplay5::hasProfiles() const
{
    return !profiles.empty();
}

void ProfileDisplay5::setProfile(const Profile5 &profile)
{
    for (int i = 0; i < profiles.size(); i++)
    {
        if (profiles[i] == profile)
        {
            ui->comboBoxProfiles->setCurrentIndex(i);
            break;
        }
    }
}

void ProfileDisplay5::setup(const QString &prefix, Game filter)
{
    this->prefix = prefix;
    this->filter = filter;
}

void ProfileDisplay5::setIDsEditable(bool enabled)
{
    idsEditable = enabled;
    tidInput->setVisible(enabled);
    sidInput->setVisible(enabled);
    ui->labelTIDValue->setVisible(!enabled);
    ui->labelSIDValue->setVisible(!enabled);
}

void ProfileDisplay5::updateProfiles()
{
    profiles = ProfileLoader5::getProfiles(filter);

    ui->comboBoxProfiles->clear();
    for (const auto &profile : profiles)
    {
        ui->comboBoxProfiles->addItem(QString::fromStdString(profile.getName()));
    }

    QSettings setting;
    int val = setting.value(settingKey.arg(prefix), 0).toInt();
    if (val < ui->comboBoxProfiles->count())
    {
        ui->comboBoxProfiles->setCurrentIndex(val);
    }
}

void ProfileDisplay5::profileIndexChanged(int index)
{
    if (index >= 0)
    {
        const Profile5 &profile = profiles[index];

        ui->labelTIDValue->setText(QString::number(profile.getTID()));
        ui->labelSIDValue->setText(QString::number(profile.getSID()));
        tidInput->setValue(profile.getTID());
        sidInput->setValue(profile.getSID());
        tidInput->setEnabled(idsEditable);
        sidInput->setEnabled(idsEditable);
        ui->labelMACAddressValue->setText(QString::number(profile.getMac(), 16));
        ui->labelDSTypeValue->setText(QString::fromStdString(profile.getDSTypeString()));
        ui->labelVCountValue->setText(QString::number(profile.getVCount(), 16));
        ui->labelTimer0Value->setText(QString::number(profile.getTimer0Min(), 16) + "-"
                                             + QString::number(profile.getTimer0Max(), 16));
        ui->labelGxStatValue->setText(QString::number(profile.getGxStat()));
        ui->labelVFrameValue->setText(QString::number(profile.getVFrame()));
        ui->labelKeypressesValue->setText(QString::fromStdString(profile.getKeypressesString()));
        ui->labelGameValue->setText(QString::fromStdString(Translator::getGame(profile.getVersion())));

        emit profileChanged(profile);
    }
}

void ProfileDisplay5::updateIDs()
{
    const int index = ui->comboBoxProfiles->currentIndex();
    if (!idsEditable || index < 0) return;

    const Profile5 original = profiles[index];
    const u16 tid = static_cast<u16>(tidInput->value());
    const u16 sid = static_cast<u16>(sidInput->value());
    if (tid == original.getTID() && sid == original.getSID()) return;

    const Profile5 update(original.getName(), original.getVersion(), tid, sid, original.getIVCache(), original.getSHACache(),
                          original.getMac(), original.getKeypresses(), original.getVCount(), original.getGxStat(), original.getVFrame(),
                          original.getSkipLR(), original.getTimer0Min(), original.getTimer0Max(), original.getMemoryLink(),
                          original.getShinyCharm(), original.getDSType(), original.getLanguage(), original.getNsPokemonReleased(),
                          original.getOvalCharm());
    ProfileLoader5::updateProfile(update, original);
    profiles[index] = update;
    QSettings setting;
    setting.setValue(settingKey.arg(prefix), index);
    emit profileChanged(profiles[index]);
    emit profilesChanged(5);
}

void ProfileDisplay5::profileManager()
{
    auto *manager = new ProfileManager5();
    connect(manager, &ProfileManager5::profilesChanged, this, [this](int num) {
        updateProfiles();
        emit profilesChanged(num);
    });
    manager->show();
}
