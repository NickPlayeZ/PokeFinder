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

#include "ProfileDisplay4.hpp"
#include "ui_ProfileDisplay4.h"
#include <Core/Enum/Game.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Gen4/Profile/ProfileManager4.hpp>
#include <QSettings>
#include <algorithm>

static const QString settingKey = QStringLiteral("%1/profile");

ProfileDisplay4::ProfileDisplay4(QWidget *parent) : QWidget(parent), ui(new Ui::ProfileDisplay4)
{
    ui->setupUi(this);

    connect(ui->comboBoxProfiles, &QComboBox::currentIndexChanged, this, &ProfileDisplay4::profileIndexChanged);
    connect(ui->pushButtonProfileManager, &QPushButton::clicked, this, &ProfileDisplay4::profileManager);
}

ProfileDisplay4::~ProfileDisplay4()
{
    QSettings setting;
    setting.setValue(settingKey.arg(prefix), ui->comboBoxProfiles->currentIndex());

    delete ui;
}

void ProfileDisplay4::setup(const QString &prefix, Game filter)
{
    this->prefix = prefix;
    this->filter = filter;
    includeDefaultProfile = true;
    defaultProfileVersion = Game::Diamond;
    defaultOnlyWhenEmpty = false;
}

void ProfileDisplay4::setup(const QString &prefix, Game filter, Game defaultProfileVersion, bool defaultOnlyWhenEmpty)
{
    this->prefix = prefix;
    this->filter = filter;
    includeDefaultProfile = true;
    this->defaultProfileVersion = defaultProfileVersion;
    this->defaultOnlyWhenEmpty = defaultOnlyWhenEmpty;
}

void ProfileDisplay4::updateProfiles()
{
    profiles = ProfileLoader4::getProfiles(filter);
    if (includeDefaultProfile && (!defaultOnlyWhenEmpty || profiles.empty()))
    {
        profiles.insert(profiles.begin(), Profile4("-", defaultProfileVersion, 12345, 54321, false));
    }

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

void ProfileDisplay4::setProfile(const Profile4 &profile)
{
    auto it = std::ranges::find(profiles, profile);
    if (it == profiles.end())
    {
        profiles.emplace_back(profile);
        ui->comboBoxProfiles->addItem(QString::fromStdString(profile.getName()));
        it = std::prev(profiles.end());
    }
    ui->comboBoxProfiles->setCurrentIndex(static_cast<int>(std::distance(profiles.begin(), it)));
}

void ProfileDisplay4::profileIndexChanged(int index)
{
    if (index >= 0)
    {
        const Profile4 &profile = profiles[index];

        ui->labelTIDValue->setText(QString::number(profile.getTID()));
        ui->labelSIDValue->setText(QString::number(profile.getSID()));
        ui->labelGameValue->setText(QString::fromStdString(Translator::getGame(profile.getVersion())));
        ui->labelNationalDexValue->setText(profile.getNationalDex() ? tr("Yes") : tr("No"));

        emit profileChanged(profile);
    }
}

void ProfileDisplay4::profileManager()
{
    auto *manager = new ProfileManager4();
    connect(manager, &ProfileManager4::profilesChanged, this, [this](int num) {
        updateProfiles();
        emit profilesChanged(num);
    });
    manager->show();
}
