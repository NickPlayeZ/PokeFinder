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

#include "IDs5.hpp"
#include "ui_IDs5.h"
#include <Core/Enum/Game.hpp>
#include <Core/Gen5/Generators/IDGenerator5.hpp>
#include <Core/Gen5/Keypresses.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/Searchers/IDSearcher5.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/RNG/SHA1.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Util/Utilities.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <Model/Gen5/IDModel5.hpp>
#include <QAction>
#include <QDate>
#include <QMessageBox>
#include <QSettings>
#include <QTime>
#include <QTimer>

static const QString settingPrefix = QStringLiteral("ids5");

IDs5::IDs5(QWidget *parent) : QWidget(parent), ui(new Ui::IDs5)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    ui->profileDisplay->setup(settingPrefix, Game::Gen5);
    ui->profileDisplay->setIDsEditable(false);

    model = new IDModel5(ui->tableView);
    ui->tableView->setModel(model);

    auto *fillSeedFinder = ui->tableView->addAction(tr("Fill out Seed Finder"));
    ui->tableView->setPrimaryAction(fillSeedFinder);
    connect(fillSeedFinder, &QAction::triggered, this, &IDs5::fillSeedFinder);

    ui->textBoxPID->setValues(InputType::Seed32Bit);
    ui->textBoxTID->setValues(InputType::TIDSID);
    ui->textBoxSID->setValues(InputType::TIDSID);

    ui->textBoxMaxAdvances->setValues(InputType::Advance32Bit);

    ui->textBoxSeedFinderTID->setValues(InputType::TIDSID);
    ui->textBoxSeedFinderMaxAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxTimer0Min->setValues(InputType::Seed16Bit);
    ui->textBoxTimer0Max->setValues(InputType::Seed16Bit);

    connect(ui->profileDisplay, &ProfileDisplay5::profileChanged, this, &IDs5::profileChanged);
    connect(ui->profileDisplay, &ProfileDisplay5::profilesChanged, this, &IDs5::profilesChanged);
    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &IDs5::search);
    connect(ui->pushButtonFind, &QPushButton::clicked, this, &IDs5::find);
    connect(ui->checkBoxPID, &QCheckBox::clicked, this, &IDs5::setXOR);

    updateProfiles();

    QSettings setting;
    setting.beginGroup(settingPrefix);
    if (setting.contains("geometry"))
    {
        this->restoreGeometry(setting.value("geometry").toByteArray());
    }
    if (setting.contains("startDate"))
    {
        ui->dateEditStart->setDate(setting.value("startDate").toDate());
    }
    if (setting.contains("endDate"))
    {
        ui->dateEditEnd->setDate(setting.value("endDate").toDate());
    }
    setting.endGroup();
}

IDs5::~IDs5()
{
    QSettings setting;
    setting.beginGroup(settingPrefix);
    setting.setValue("geometry", this->saveGeometry());
    setting.setValue("startDate", ui->dateEditStart->date());
    setting.setValue("endDate", ui->dateEditEnd->date());
    setting.endGroup();

    delete ui;
}

bool IDs5::hasProfiles() const
{
    return ui->profileDisplay->hasProfiles();
}

void IDs5::updateProfiles()
{
    ui->profileDisplay->updateProfiles();
}

void IDs5::fillSeedFinder()
{
    int row = ui->tableView->currentIndex().row();
    if (row < 0)
    {
        return;
    }

    const auto &result = model->getItem(row);
    DateTime dateTime = result.getDateTime();
    DateParts date = dateTime.getDate().getParts();
    Time time = dateTime.getTime();
    ui->dateEdit->setDate(QDate(date.year, date.month, date.day));
    ui->timeEdit->setTime(QTime(time.hour(), time.minute(), time.second()));

    u32 initialAdvances = Utilities5::initialAdvancesID(result.getInitialSeed(), currentProfile->getVersion());
    u32 advances = result.getState().getAdvances();
    ui->textBoxSeedFinderMaxAdvances->setText(QString::number(advances - initialAdvances + 2));
}

void IDs5::find()
{
    model->clearModel();
    model->setGame(currentProfile->getVersion());

    u16 tid = ui->textBoxSeedFinderTID->getUShort();
    Date date = ui->dateEdit->getDate();
    QTime time = ui->timeEdit->time();
    DateTime dateTime(date, Time(time.hour(), time.minute(), time.second()));
    int seconds = ui->spinBoxSeconds->value();
    u16 timer0Min = ui->textBoxTimer0Min->getUShort();
    u16 timer0Max = ui->textBoxTimer0Max->getUShort();
    u32 maxAdvance = ui->textBoxSeedFinderMaxAdvances->getUInt();

    if (timer0Min > timer0Max)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid Timer0 range"), tr("Minimum Timer0 is greater than maximum Timer0"));
        msg.exec();
        return;
    }

    IDFilter filter({ tid }, { }, { }, { }, { }, { });
    IDGenerator5 generator(0, maxAdvance, 0, false, false, *currentProfile, filter);
    IDSearcher5 searcher(generator, *currentProfile);

    auto states = searcher.search(generator, dateTime, seconds, timer0Min, timer0Max);
    model->addItems(states);
}

void IDs5::search()
{
    Date start = ui->dateEditStart->getDate();
    Date end = ui->dateEditEnd->getDate();
    if (start > end)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid date range"), tr("Start date is after end date"));
        msg.exec();
        return;
    }

    model->clearModel();
    model->setGame(currentProfile->getVersion());

    ui->pushButtonSearch->setEnabled(false);
    ui->pushButtonFind->setEnabled(false);
    ui->pushButtonCancel->setEnabled(true);

    u32 pid = ui->textBoxPID->getUInt();
    bool usePID = ui->checkBoxPID->isChecked();
    bool useXOR = ui->checkBoxXOR->isChecked();

    std::vector<u16> tid;
    if (ui->checkBoxTID->isChecked())
    {
        tid.emplace_back(ui->textBoxTID->getUShort());
    }

    std::vector<u16> sid;
    if (ui->checkBoxSID->isChecked())
    {
        sid.emplace_back(ui->textBoxSID->getUShort());
    }

    IDFilter filter(tid, sid, { }, { }, { }, { });
    IDGenerator5 generator(0, ui->textBoxMaxAdvances->getUInt(), pid, usePID, useXOR, *currentProfile, filter);

    auto *searcher = new IDSearcher5(generator, *currentProfile);
    searcher->setMaxProgress(searcher->getMaxProgress(start, end));

    QSettings settings;
    int threads = settings.value("settings/threads").toInt();

    auto *timer = new QTimer(this);
    connect(ui->pushButtonCancel, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        ui->pushButtonCancel->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer] {
        model->addItems(searcher->getResults());
        ui->progressBar->setValue(searcher->getProgress());

        if (!searcher->isSearching())
        {
            timer->stop();

            model->addItems(searcher->getResults());
            ui->progressBar->setValue(searcher->getProgress());

            ui->pushButtonSearch->setEnabled(true);
            ui->pushButtonFind->setEnabled(true);
            ui->pushButtonCancel->setEnabled(false);

            delete searcher;
            timer->deleteLater();
        }
    });

    searcher->startSearch(threads, start, end);
    timer->start(1000);
}

void IDs5::setXOR(bool checked)
{
    ui->checkBoxXOR->setChecked(false);
    ui->checkBoxXOR->setEnabled(checked);
}

void IDs5::profileChanged(const Profile5 &profile)
{
    currentProfile = &profile;
    ui->textBoxTimer0Min->setText(QString::number(profile.getTimer0Min(), 16));
    ui->textBoxTimer0Max->setText(QString::number(profile.getTimer0Max(), 16));
}
