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

#include "PhenomenonItem.hpp"
#include "ui_PhenomenonItem.h"
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Encounter.hpp>
#include <Core/Gen5/EncounterArea5.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/Generators/PhenomenonGenerator.hpp>
#include <Core/Gen5/Keypresses.hpp>
#include <Core/Gen5/PhenomenonArea.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/Searchers/PhenomenonSearcher.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/Controls.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <Form/Gen5/Tools/AdjacentSeeds.hpp>
#include <Model/Gen5/PhenomenonModel.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

static const QString settingPrefix = QStringLiteral("phenomenonItem");

PhenomenonItem::PhenomenonItem(QWidget *parent) : QWidget(parent), ui(new Ui::PhenomenonItem), currentProfile(nullptr)
{
    ui->setupUi(this);
    ui->gridLayout->setColumnStretch(0, 1);
    ui->gridLayout->setColumnStretch(1, 2);
    ui->gridLayout->setColumnStretch(2, 1);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->profileDisplay->setup(settingPrefix, Game::Gen5);

    searcherModel = new PhenomenonSearcherModel5(ui->tableViewSearcher);
    proxyModel = new SortFilterProxyModel(ui->tableViewSearcher, searcherModel);
    ui->tableViewSearcher->setModel(proxyModel);

    ui->textBoxSearcherInitialAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherMaxAdvances->setValues(InputType::Advance32Bit);

    ui->comboBoxSearcherLocation->enableAutoComplete();
    ui->comboBoxSearcherEncounter->addItem(tr("Dust Cloud"), toInt(Encounter::DustCloud));
    ui->comboBoxSearcherEncounter->addItem(tr("Flying Shadow"), toInt(Encounter::FlyingShadow));

    auto *adjacentSeeds = ui->tableViewSearcher->addAction(tr("Adjacent Seeds"));
    ui->tableViewSearcher->setPrimaryAction(adjacentSeeds);
    connect(adjacentSeeds, &QAction::triggered, this, &PhenomenonItem::openAdjacentSeeds);

    auto *phenomenonGenerator = ui->tableViewSearcher->addAction(tr("Open in Generator"));
    ui->tableViewSearcher->setSecondaryAction(phenomenonGenerator);
    connect(phenomenonGenerator, &QAction::triggered, this, &PhenomenonItem::openPhenomenonGenerator);

    connect(ui->profileDisplay, &ProfileDisplay5::profileChanged, this, &PhenomenonItem::profileChanged);
    connect(ui->profileDisplay, &ProfileDisplay5::profilesChanged, this, &PhenomenonItem::profilesChanged);
    connect(ui->comboBoxSearcherEncounter, &QComboBox::currentIndexChanged, this, &PhenomenonItem::searcherEncounterIndexChanged);
    connect(ui->comboBoxSearcherLocation, &QComboBox::currentIndexChanged, this, &PhenomenonItem::searcherLocationIndexChanged);
    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &PhenomenonItem::search);

    updateProfiles();

    QSettings setting;
    setting.beginGroup(settingPrefix);
    if (setting.contains("geometry"))
    {
        this->restoreGeometry(setting.value("geometry").toByteArray());
    }
    if (setting.contains("startDate"))
    {
        ui->dateEditSearcherStartDate->setDate(setting.value("startDate").toDate());
    }
    if (setting.contains("endDate"))
    {
        ui->dateEditSearcherEndDate->setDate(setting.value("endDate").toDate());
    }
    setting.endGroup();
}

PhenomenonItem::~PhenomenonItem()
{
    QSettings setting;
    setting.beginGroup(settingPrefix);
    setting.setValue("geometry", this->saveGeometry());
    setting.setValue("startDate", ui->dateEditSearcherStartDate->date());
    setting.setValue("endDate", ui->dateEditSearcherEndDate->date());
    setting.endGroup();

    delete ui;
}

bool PhenomenonItem::hasProfiles() const
{
    return ui->profileDisplay->hasProfiles();
}

void PhenomenonItem::updateProfiles()
{
    ui->profileDisplay->updateProfiles();
}

void PhenomenonItem::search()
{
    if (currentProfile == nullptr || ui->comboBoxSearcherLocation->currentIndex() < 0 || encounter.empty())
    {
        return;
    }

    Date start = ui->dateEditSearcherStartDate->getDate();
    Date end = ui->dateEditSearcherEndDate->getDate();
    if (start > end)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid date range"), tr("Start date is after end date"));
        msg.exec();
        return;
    }

    searcherModel->clearModel();
    ui->pushButtonSearch->setEnabled(false);
    ui->pushButtonCancel->setEnabled(true);

    u32 initialAdvances = ui->textBoxSearcherInitialAdvances->getUInt();
    u32 maxAdvances = ui->textBoxSearcherMaxAdvances->getUInt();

    PhenomenonFilter filter(ui->comboBoxSearcherItem->getCurrentUShort());
    PhenomenonGenerator generator(initialAdvances, maxAdvances, 0, encounter[ui->comboBoxSearcherLocation->currentIndex()], *currentProfile,
                                  filter, static_cast<u8>(ui->spinBoxSearcherAmount->value()),
                                  static_cast<u32>(ui->spinBoxSearcherMinItemDistance->value()),
                                  static_cast<u32>(ui->spinBoxSearcherPostItemPhenomenonDistance->value()),
                                  static_cast<u32>(ui->spinBoxSearcherPreItemPhenomenonDistance->value()));
    auto *searcher = new PhenomenonSearcher(generator, *currentProfile);

    searcher->setMaxProgress(searcher->getMaxProgress(start, end));

    QSettings settings;
    int threads = settings.value("settings/threads").toInt();

    auto *timer = new QTimer(this);
    connect(ui->pushButtonCancel, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        ui->pushButtonCancel->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer] {
        searcherModel->addItems(searcher->getResults());
        ui->progressBar->setValue(searcher->getProgress());

        if (!searcher->isSearching())
        {
            timer->stop();

            searcherModel->addItems(searcher->getResults());
            ui->progressBar->setValue(searcher->getProgress());

            ui->pushButtonSearch->setEnabled(true);
            ui->pushButtonCancel->setEnabled(false);

            delete searcher;
            timer->deleteLater();
        }
    });

    searcher->startSearch(threads, start, end);
    timer->start(1000);
}

void PhenomenonItem::searcherLocationIndexChanged(int index)
{
    if (index >= 0)
    {
        const auto &area = encounter[ui->comboBoxSearcherLocation->currentIndex()];

        std::vector<u16> items = area.getUniqueItems();
        std::vector<std::string> itemNames = area.getItemNames();

        std::vector<std::pair<QString, u16>> itemEntries;
        itemEntries.reserve(items.size());
        for (size_t i = 0; i < items.size(); i++)
        {
            itemEntries.emplace_back(QString::fromStdString(itemNames[i]), items[i]);
        }
        std::ranges::sort(itemEntries, [](const auto &left, const auto &right) {
            return QString::localeAwareCompare(left.first, right.first) < 0;
        });

        u16 currentItem = ui->comboBoxSearcherItem->getCurrentUShort();
        ui->comboBoxSearcherItem->clear();
        for (const auto &[name, item] : itemEntries)
        {
            ui->comboBoxSearcherItem->addItem(name, item);
        }
        int currentIndex = ui->comboBoxSearcherItem->findData(currentItem);
        if (currentIndex >= 0)
        {
            ui->comboBoxSearcherItem->setCurrentIndex(currentIndex);
        }
    }
}

void PhenomenonItem::openAdjacentSeeds()
{
    QModelIndex index = proxyModel->mapToSource(ui->tableViewSearcher->currentIndex());
    if (!index.isValid() || currentProfile == nullptr)
    {
        return;
    }

    const auto &state = searcherModel->getItem(index.row());
    auto *window = new AdjacentSeeds(false, state.getButtons(), state.getDateTime(), *currentProfile);
    window->show();
}

void PhenomenonItem::openPhenomenonGenerator()
{
    QModelIndex index = proxyModel->mapToSource(ui->tableViewSearcher->currentIndex());
    if (!index.isValid() || currentProfile == nullptr || ui->comboBoxSearcherLocation->currentIndex() < 0)
    {
        return;
    }

    const auto &state = searcherModel->getItem(index.row());
    emit openGenerator(*currentProfile, ui->comboBoxSearcherEncounter->getEnum<Encounter>(),
                       static_cast<u8>(ui->comboBoxSearcherLocation->getCurrentUShort()), state.getInitialSeed());
}

void PhenomenonItem::searcherEncounterIndexChanged(int index)
{
    if (index < 0 || currentProfile == nullptr)
    {
        return;
    }

    u16 currentLocation = ui->comboBoxSearcherLocation->getCurrentUShort();
    Encounter selected = ui->comboBoxSearcherEncounter->getEnum<Encounter>();
    PhenomenonType type = selected == Encounter::DustCloud ? PhenomenonType::Cave : PhenomenonType::Bridge;
    bool dustCloud = selected == Encounter::DustCloud;
    ui->spinBoxSearcherMinItemDistance->setValue(dustCloud ? 30 : 0);
    ui->spinBoxSearcherPostItemPhenomenonDistance->setValue(dustCloud ? 20 : 0);
    ui->spinBoxSearcherPreItemPhenomenonDistance->setValue(dustCloud ? 10 : 0);

    encounter.clear();
    EncounterSettings5 settings = { };
    for (const auto &area : Encounters5::getEncounters(selected, settings, currentProfile))
    {
        encounter.emplace_back(area.getLocation(), type);
    }

    std::vector<u16> locations;
    std::ranges::transform(encounter, std::back_inserter(locations), [](const PhenomenonArea &area) { return area.getLocation(); });
    ui->comboBoxSearcherLocation->clear();
    ui->comboBoxSearcherLocation->addItems(Translator::getLocations(locations, currentProfile->getVersion()), locations);
    ui->comboBoxSearcherLocation->setCurrentIndexByData(currentLocation);
}

void PhenomenonItem::profileChanged(const Profile5 &profile)
{
    currentProfile = &profile;

    searcherEncounterIndexChanged(ui->comboBoxSearcherEncounter->currentIndex());
}
