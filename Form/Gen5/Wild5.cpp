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

#include "Wild5.hpp"
#include "ui_Wild5.h"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen5/EncounterArea5.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Gen5/IVCache.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/Gen5/Searchers/WildSearcher5.hpp>
#include <Core/Parents/Filters/StateFilter.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/Controls.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/Filter.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <Form/Gen5/Tools/AdjacentSeeds.hpp>
#include <Form/Util/AdvanceFinder.hpp>
#include <Model/Gen5/WildModel5.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <QAction>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <algorithm>
#include <vector>

static bool supportsMovingTrigger(Encounter encounter, const Profile5 *profile)
{
    return profile != nullptr && (profile->getVersion() & Game::Gen5) != Game::None
        && (encounter == Encounter::Grass || encounter == Encounter::GrassDark || encounter == Encounter::Surfing);
}

static std::vector<Lead> getSearcherLeads(ComboMenu *comboMenu)
{
    auto data = comboMenu->getCheckedData();
    std::vector<Lead> leads;
    for (int lead : data)
    {
        Lead value = static_cast<Lead>(lead);
        if (!std::ranges::contains(leads, value))
        {
            leads.emplace_back(value);
        }
    }
    if (leads.empty())
    {
        leads.emplace_back(Lead::None);
    }
    return leads;
}

static const QString settingPrefix = QStringLiteral("static5");

static std::vector<u8> getCheckedUChars(const ComboMenu *comboMenu)
{
    auto data = comboMenu->getCheckedData();
    std::vector<u8> values;
    values.reserve(data.size());
    for (int value : data)
    {
        values.emplace_back(value);
    }
    return values;
}

static bool hasPassPower(const std::vector<u8> &powers)
{
    return std::ranges::find_if(powers, [](u8 power) { return power != PassPower5::None; }) != powers.end();
}

static bool isLuckyPower(u8 passPower)
{
    u8 luckyPower = PassPower5::getLuckyPower(passPower);
    return luckyPower == PassPower5::Lucky1 || luckyPower == PassPower5::Lucky2 || luckyPower == PassPower5::Lucky3;
}

static bool isEncounterPower(u8 passPower)
{
    u8 encounterPower = PassPower5::getEncounterPower(passPower);
    return encounterPower == PassPower5::getEncounterPower(PassPower5::Encounter1)
        || encounterPower == PassPower5::getEncounterPower(PassPower5::Encounter2)
        || encounterPower == PassPower5::getEncounterPower(PassPower5::Encounter3);
}

static void addPassPowerOption(ComboMenu *comboMenu, const QString &text, u8 passPower)
{
    comboMenu->addAction(text, passPower);
}

static std::vector<int> getPassPowerMenuOptions(u8 passPower)
{
    if (passPower == PassPower5::None)
    {
        return { PassPower5::None };
    }

    std::vector<int> options;
    if (PassPower5::getLuckyPower(passPower) != PassPower5::None)
    {
        options.emplace_back(PassPower5::getLuckyPower(passPower));
    }
    if (PassPower5::getEncounterPower(passPower) != PassPower5::None)
    {
        options.emplace_back(PassPower5::getEncounterPower(passPower) << PassPower5::EncounterShift);
    }
    return options;
}

static std::vector<u8> getPassPowers(const std::vector<u8> &powerOptions, bool searchMovingTrigger, bool bw)
{
    std::vector<u8> activeLuckyPowers;
    std::vector<u8> activeEncounterPowers = { PassPower5::None };
    for (u8 powerOption : powerOptions)
    {
        if (powerOption == PassPower5::None)
        {
            activeLuckyPowers.emplace_back(PassPower5::None);
        }
        else if (isLuckyPower(powerOption))
        {
            activeLuckyPowers.emplace_back(PassPower5::getLuckyPower(powerOption));
        }
        else if (searchMovingTrigger && isEncounterPower(powerOption))
        {
            activeEncounterPowers.emplace_back(PassPower5::getEncounterPower(powerOption));
        }
    }

    if (activeLuckyPowers.empty())
    {
        activeLuckyPowers = { PassPower5::None };
    }
    else if (bw)
    {
        activeLuckyPowers.emplace_back(PassPower5::None);
    }

    std::ranges::sort(activeLuckyPowers);
    activeLuckyPowers.erase(std::ranges::unique(activeLuckyPowers).begin(), activeLuckyPowers.end());
    std::ranges::sort(activeEncounterPowers);
    activeEncounterPowers.erase(std::ranges::unique(activeEncounterPowers).begin(), activeEncounterPowers.end());

    std::vector<u8> passPowers;
    for (u8 luckyPower : activeLuckyPowers)
    {
        for (u8 encounterPower : activeEncounterPowers)
        {
            passPowers.emplace_back(PassPower5::combine(luckyPower, encounterPower));
        }
    }
    return passPowers;
}

static std::vector<int> getGeneratorPassPowerProperty(const ComboMenu *comboMenu)
{
    auto values = comboMenu->property("generatorPassPowerOptions").toList();
    std::vector<int> powerOptions;
    powerOptions.reserve(values.size());
    for (const auto &value : values)
    {
        powerOptions.emplace_back(value.toInt());
    }
    return powerOptions;
}

static void setGeneratorPassPowerProperty(ComboMenu *comboMenu, const std::vector<int> &powerOptions)
{
    QVariantList values;
    for (int powerOption : powerOptions)
    {
        values.emplace_back(powerOption);
    }
    comboMenu->setProperty("generatorPassPowerOptions", values);
}

static std::vector<int> normalizeGeneratorPassPowerOptions(const std::vector<int> &powerOptions, const std::vector<int> &previousPowerOptions)
{
    std::vector<int> luckyOptions;
    std::vector<int> encounterOptions;
    for (int powerOption : powerOptions)
    {
        if (isLuckyPower(powerOption))
        {
            luckyOptions.emplace_back(powerOption);
        }
        else if (isEncounterPower(powerOption))
        {
            encounterOptions.emplace_back(powerOption);
        }
    }

    if (std::ranges::contains(powerOptions, static_cast<int>(PassPower5::None))
        && !std::ranges::contains(previousPowerOptions, static_cast<int>(PassPower5::None)))
    {
        return { PassPower5::None };
    }

    u8 luckyPower = PassPower5::None;
    u8 encounterPower = PassPower5::None;
    for (int previousPowerOption : previousPowerOptions)
    {
        if (isLuckyPower(previousPowerOption) && std::ranges::contains(luckyOptions, previousPowerOption))
        {
            luckyPower = PassPower5::getLuckyPower(previousPowerOption);
        }
        else if (isEncounterPower(previousPowerOption) && std::ranges::contains(encounterOptions, previousPowerOption))
        {
            encounterPower = PassPower5::getEncounterPower(previousPowerOption);
        }
    }

    if (luckyPower == PassPower5::None && !std::ranges::any_of(previousPowerOptions, isLuckyPower) && !luckyOptions.empty())
    {
        luckyPower = PassPower5::getLuckyPower(luckyOptions.front());
    }

    if (encounterPower == PassPower5::None && !std::ranges::any_of(previousPowerOptions, isEncounterPower) && !encounterOptions.empty())
    {
        encounterPower = PassPower5::getEncounterPower(encounterOptions.front());
    }

    std::vector<int> normalized;
    if (luckyPower != PassPower5::None)
    {
        normalized.emplace_back(luckyPower);
    }
    if (encounterPower != PassPower5::None)
    {
        normalized.emplace_back(encounterPower << PassPower5::EncounterShift);
    }
    if (normalized.empty())
    {
        normalized.emplace_back(PassPower5::None);
    }
    return normalized;
}

static u8 getGeneratorPassPower(const ComboMenu *comboMenu)
{
    auto powerOptions = normalizeGeneratorPassPowerOptions(comboMenu->getRawCheckedData(), getGeneratorPassPowerProperty(comboMenu));
    u8 luckyPower = PassPower5::None;
    u8 encounterPower = PassPower5::None;
    for (int powerOption : powerOptions)
    {
        if (isLuckyPower(powerOption))
        {
            luckyPower = PassPower5::getLuckyPower(powerOption);
        }
        else if (isEncounterPower(powerOption))
        {
            encounterPower = PassPower5::getEncounterPower(powerOption);
        }
    }
    return PassPower5::combine(luckyPower, encounterPower);
}

static void updateGeneratorPassPowerActions(ComboMenu *comboMenu, bool bw)
{
    auto powerOptions = normalizeGeneratorPassPowerOptions(comboMenu->getRawCheckedData(), getGeneratorPassPowerProperty(comboMenu));
    if (bw)
    {
        std::erase_if(powerOptions, [](int powerOption) { return isLuckyPower(powerOption); });
        if (powerOptions.empty())
        {
            powerOptions.emplace_back(PassPower5::None);
        }
    }
    setGeneratorPassPowerProperty(comboMenu, powerOptions);
    comboMenu->setCheckedData(powerOptions);
}

Wild5::Wild5(QWidget *parent) : QWidget(parent), ui(new Ui::Wild5), ivCache(nullptr), shaCache(nullptr)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    ui->profileDisplay->setup(settingPrefix, Game::Gen5);

    generatorModel = new WildGeneratorModel5(ui->tableViewGenerator);
    searcherModel = new WildSearcherModel5(ui->tableViewSearcher);
    proxyModel = new SortFilterProxyModel(ui->tableViewSearcher, searcherModel);

    ui->tableViewGenerator->setModel(generatorModel);
    ui->tableViewSearcher->setModel(proxyModel);

    ui->textBoxGeneratorSeed->setValues(InputType::Seed64Bit);
    ui->textBoxGeneratorIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorInitialAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorMaxAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorOffset->setValues(InputType::Advance32Bit);

    ui->textBoxSearcherInitialIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherMaxIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherInitialAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherMaxAdvances->setValues(InputType::Advance32Bit);

    ui->comboBoxGeneratorEncounter->setup(
        { toInt(Encounter::Grass), toInt(Encounter::GrassDark), toInt(Encounter::Surfing), toInt(Encounter::SuperRod) });
    ui->comboBoxSearcherEncounter->setup(
        { toInt(Encounter::Grass), toInt(Encounter::GrassDark), toInt(Encounter::Surfing), toInt(Encounter::SuperRod) });

    ui->filterGenerator->disableControls(Controls::Height | Controls::Weight);
    ui->filterSearcher->disableControls(Controls::DisableFilter | Controls::Height | Controls::Weight);

    ui->comboMenuGeneratorLead->addAction(tr("None"), toInt(Lead::None));
    ui->comboMenuGeneratorLead->addAction(tr("Compound Eyes"), toInt(Lead::CompoundEyes));
    ui->comboMenuGeneratorLead->addMenu(tr("Cute Charm"),
                                        { { tr("♂ Lead"), toInt(Lead::CuteCharmM) }, { tr("♀ Lead"), toInt(Lead::CuteCharmF) } });
    ui->comboMenuGeneratorLead->addMenu(
        tr("Encounter Modifier"), { { tr("Sticky Hold"), toInt(Lead::StickyHold) }, { tr("Suction Cups"), toInt(Lead::SuctionCups) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Level Modifier"),
                                        { { tr("Hustle"), toInt(Lead::Hustle) },
                                          { tr("Pressure"), toInt(Lead::Pressure) },
                                          { tr("Vital Spirit"), toInt(Lead::VitalSpirit) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Slot Modifier"),
                                        { { tr("Magnet Pull"), toInt(Lead::MagnetPull) }, { tr("Static"), toInt(Lead::Static) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Encounter Modifier"),
                                        { { tr("Arena Trap"), toInt(Lead::ArenaTrap) },
                                          { tr("Illuminate"), toInt(Lead::Illuminate) },
                                          { tr("No Guard"), toInt(Lead::NoGuard) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Synchronize"), Translator::getNatures());

    ui->comboMenuSearcherLead->addAction(tr("None"), toInt(Lead::None));
    ui->comboMenuSearcherLead->addAction(tr("Compound Eyes"), toInt(Lead::CompoundEyes));
    ui->comboMenuSearcherLead->addMenu(tr("Cute Charm"),
                                       { { tr("♂ Lead"), toInt(Lead::CuteCharmM) }, { tr("♀ Lead"), toInt(Lead::CuteCharmF) } });
    ui->comboMenuSearcherLead->addMenu(
        tr("Encounter Modifier"), { { tr("Sticky Hold"), toInt(Lead::StickyHold) }, { tr("Suction Cups"), toInt(Lead::SuctionCups) } });
    ui->comboMenuSearcherLead->addMenu(tr("Level Modifier"),
                                       { { tr("Hustle"), toInt(Lead::Hustle) },
                                         { tr("Pressure"), toInt(Lead::Pressure) },
                                         { tr("Vital Spirit"), toInt(Lead::VitalSpirit) } });
    ui->comboMenuSearcherLead->addMenu(tr("Slot Modifier"),
                                       { { tr("Magnet Pull"), toInt(Lead::MagnetPull) }, { tr("Static"), toInt(Lead::Static) } });
    ui->comboMenuSearcherLead->addMenu(tr("Encounter Modifier"),
                                       { { tr("Arena Trap"), toInt(Lead::ArenaTrap) },
                                         { tr("Illuminate"), toInt(Lead::Illuminate) },
                                         { tr("No Guard"), toInt(Lead::NoGuard) } });
    ui->comboMenuSearcherLead->addMenu(tr("Synchronize"), Translator::getNatures());
    ui->comboMenuSearcherLead->setMultiSelect(true);
    ui->comboMenuSearcherLead->setCheckedData({ toInt(Lead::None) });
    ui->comboMenuSearcherLead->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    ui->comboBoxGeneratorLocation->enableAutoComplete();
    ui->comboBoxSearcherLocation->enableAutoComplete();

    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("None"), PassPower5::None);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Lucky Power ↑"), PassPower5::Lucky1);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Lucky Power ↑↑"), PassPower5::Lucky2);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Lucky Power ↑↑↑/S"), PassPower5::Lucky3);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Encounter Power ↑"), PassPower5::Encounter1);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Encounter Power ↑↑"), PassPower5::Encounter2);
    addPassPowerOption(ui->comboBoxGeneratorLuckyPower, tr("Encounter Power ↑↑↑"), PassPower5::Encounter3);
    ui->comboBoxGeneratorLuckyPower->setMultiSelect(true);
    ui->comboBoxGeneratorLuckyPower->setCheckedData({ PassPower5::None });
    ui->comboBoxGeneratorLuckyPower->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    ui->comboBoxSearcherLuckyPower->setMultiSelect(true);
    ui->comboBoxSearcherLuckyPower->addAction(tr("None"), PassPower5::None);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Lucky Power ↑"), PassPower5::Lucky1);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Lucky Power ↑↑"), PassPower5::Lucky2);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Lucky Power ↑↑↑/S"), PassPower5::Lucky3);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Encounter Power ↑"), PassPower5::Encounter1);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Encounter Power ↑↑"), PassPower5::Encounter2);
    ui->comboBoxSearcherLuckyPower->addAction(tr("Encounter Power ↑↑↑"), PassPower5::Encounter3);
    ui->comboBoxSearcherLuckyPower->setCheckedData({ PassPower5::None });
    ui->comboBoxSearcherLuckyPower->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    auto *advanceFinder = ui->tableViewGenerator->addAction(tr("Advance Finder"));
    connect(advanceFinder, &QAction::triggered, this, &Wild5::openAdvanceFinder);

    auto *adjacentSeeds = ui->tableViewSearcher->addAction(tr("Adjacent Seeds"));
    connect(adjacentSeeds, &QAction::triggered, this, &Wild5::openAdjacentSeeds);

    connect(ui->profileDisplay, &ProfileDisplay5::profileChanged, this, &Wild5::profileChanged);
    connect(ui->profileDisplay, &ProfileDisplay5::profilesChanged, this, &Wild5::profilesChanged);
    connect(ui->tabRNGSelector, &TabWidget::transferFilters, this, &Wild5::transferFilters);
    connect(ui->tabRNGSelector, &TabWidget::transferSettings, this, &Wild5::transferSettings);
    connect(ui->pushButtonGenerate, &QPushButton::clicked, this, &Wild5::generate);
    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &Wild5::search);
    connect(ui->comboBoxGeneratorEncounter, &QComboBox::currentIndexChanged, this, &Wild5::generatorEncounterIndexChanged);
    connect(ui->comboBoxSearcherEncounter, &QComboBox::currentIndexChanged, this, &Wild5::searcherEncounterIndexChanged);
    connect(ui->comboBoxGeneratorLocation, &QComboBox::currentIndexChanged, this, &Wild5::generatorLocationIndexChanged);
    connect(ui->comboBoxSearcherLocation, &QComboBox::currentIndexChanged, this, &Wild5::searcherLocationIndexChanged);
    connect(ui->comboBoxGeneratorPokemon, &QComboBox::currentIndexChanged, this, &Wild5::generatorPokemonIndexChanged);
    connect(ui->comboBoxSearcherPokemon, &QComboBox::currentIndexChanged, this, &Wild5::searcherPokemonIndexChanged);
    connect(ui->comboBoxGeneratorSeason, &QComboBox::currentIndexChanged, this, &Wild5::generatorSeasonIndexChanged);
    connect(ui->comboBoxSearcherSeason, &QComboBox::currentIndexChanged, this, &Wild5::searcherSeasonIndexChanged);
    connect(ui->checkBoxGeneratorSwarm, &QCheckBox::checkStateChanged, this, [=] { generatorEncounterIndexChanged(0); });
    connect(ui->checkBoxSearcherSwarm, &QCheckBox::checkStateChanged, this, [=] { searcherEncounterIndexChanged(0); });
    connect(ui->filterGenerator, &Filter::showStatsChanged, generatorModel, &WildGeneratorModel5::setShowStats);
    connect(ui->filterSearcher, &Filter::showStatsChanged, searcherModel, &WildSearcherModel5::setShowStats);
    connect(ui->checkBoxGeneratorMovingTrigger, &QCheckBox::toggled, this, [this](bool checked) {
        generatorModel->setShowMovingTrigger(checked);
        if (generatorModel->rowCount() > 0)
        {
            generate();
        }
    });
    connect(ui->checkBoxSearcherMovingTrigger, &QCheckBox::toggled, searcherModel, &WildSearcherModel5::setShowMovingTrigger);
    connect(ui->comboBoxGeneratorLuckyPower, &ComboMenu::checkedDataChanged, this, [this] {
        QSignalBlocker blocker(ui->comboBoxGeneratorLuckyPower);
        bool bw1 = (currentProfile->getVersion() & Game::BW) != Game::None;
        updateGeneratorPassPowerActions(ui->comboBoxGeneratorLuckyPower, bw1);
    });
    connect(ui->filterSearcher, &Filter::ivsChanged, this, &Wild5::searcherFastSearchChanged);
    connect(ui->textBoxSearcherInitialIVAdvances, &TextBox::textChanged, this, &Wild5::searcherFastSearchChanged);
    connect(ui->textBoxSearcherMaxIVAdvances, &TextBox::textChanged, this, &Wild5::searcherFastSearchChanged);

    updateProfiles();
    if (hasProfiles())
    {
        generatorEncounterIndexChanged(0);
        searcherEncounterIndexChanged(0);
    }
    searcherFastSearchChanged();

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

Wild5::~Wild5()
{
    QSettings setting;
    setting.beginGroup(settingPrefix);
    setting.setValue("geometry", this->saveGeometry());
    setting.setValue("startDate", ui->dateEditSearcherStartDate->date());
    setting.setValue("endDate", ui->dateEditSearcherEndDate->date());
    setting.endGroup();

    delete ivCache;
    delete shaCache;
    delete ui;
}

bool Wild5::hasProfiles() const
{
    return ui->profileDisplay->hasProfiles();
}

void Wild5::updateProfiles()
{
    ui->profileDisplay->updateProfiles();
}

bool Wild5::fastSearchEnabled() const
{
    if (ivCache == nullptr)
    {
        return false;
    }

    u32 initialAdvances = ui->textBoxSearcherInitialIVAdvances->getUInt();
    u32 maxAdvances = ui->textBoxSearcherMaxIVAdvances->getUInt();

    if (initialAdvances < ivCache->getInitialAdvances()
        || (initialAdvances + maxAdvances) > (ivCache->getInitialAdvances() + ivCache->getMaxAdvances()))
    {
        return false;
    }

    auto min = ui->filterSearcher->getMinIVs();
    auto max = ui->filterSearcher->getMaxIVs();

    return min[0] >= 30 && min[2] >= 30 && min[4] >= 30 && (min[1] >= 30 || min[3] >= 30) && (min[5] >= 30 || max[5] <= 1);
}

void Wild5::generate()
{
    if (!ui->filterGenerator->isValid(ui->spinBoxGeneratorLevelMin->value(), ui->spinBoxGeneratorLevelMax->value()))
    {
        return;
    }

    generatorModel->clearModel();

    u64 seed = ui->textBoxGeneratorSeed->getULong();
    u32 ivAdvances = ui->textBoxGeneratorIVAdvances->getUInt();
    u32 initialAdvances = ui->textBoxGeneratorInitialAdvances->getUInt();
    u32 maxAdvances = ui->textBoxGeneratorMaxAdvances->getUInt();
    u32 offset = ui->textBoxGeneratorOffset->getUInt();
    auto lead = ui->comboMenuGeneratorLead->getEnum<Lead>();
    u8 passPower = (currentProfile->getVersion() & Game::BW2) != Game::None ? getGeneratorPassPower(ui->comboBoxGeneratorLuckyPower) : PassPower5::None;
    bool searchMovingTrigger = ui->checkBoxGeneratorMovingTrigger->isChecked() && (currentProfile->getVersion() & Game::Gen5) != Game::None;
    if (!searchMovingTrigger)
    {
        passPower = PassPower5::combine(PassPower5::getLuckyPower(passPower), PassPower5::None);
    }

    auto filter = ui->filterGenerator->getFilter<WildStateFilter, true>();
    WildGenerator5 generator(initialAdvances, maxAdvances, offset, Method::None, lead, passPower, searchMovingTrigger, false,
                             encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()], *currentProfile, filter);

    auto states = generator.generate(seed, ivAdvances, 0);
    if (ui->filterGenerator->hasActiveFilters(encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()].getCount()))
    {
        std::erase_if(states, [](const auto &state) { return !state.isValid(); });
    }
    generatorModel->addItems(states);
}

void Wild5::generatorEncounterIndexChanged(int index)
{
    if (index >= 0)
    {
        auto encounter = ui->comboBoxGeneratorEncounter->getEnum<Encounter>();
        u16 currentLocation = ui->comboBoxGeneratorLocation->getCurrentUShort();
        bool movingTrigger = supportsMovingTrigger(encounter, currentProfile);
        ui->checkBoxGeneratorMovingTrigger->setEnabled(movingTrigger);
        if (!movingTrigger)
        {
            ui->checkBoxGeneratorMovingTrigger->setChecked(false);
        }

        bool grass = encounter == Encounter::Grass;
        bool fish = encounter == Encounter::SuperRod;

        ui->comboMenuGeneratorLead->hideAction(toInt(Lead::SuctionCups), !fish);
        ui->checkBoxGeneratorSwarm->setVisible(grass);
        if (!grass && ui->checkBoxGeneratorSwarm->isChecked())
        {
            ui->checkBoxGeneratorSwarm->setChecked(false);
        }

        EncounterSettings5 settings = { };
        settings.swarm = ui->checkBoxGeneratorSwarm->isChecked();
        settings.season = ui->comboBoxGeneratorSeason->currentIndex();

        encounterGenerator = Encounters5::getEncounters(encounter, settings, currentProfile);

        std::vector<u16> locs;
        std::ranges::transform(encounterGenerator, std::back_inserter(locs), [](const EncounterArea5 &area) { return area.getLocation(); });

        ui->comboBoxGeneratorLocation->clear();
        ui->comboBoxGeneratorLocation->addItems(Translator::getLocations(locs, currentProfile->getVersion()), locs);
        ui->comboBoxGeneratorLocation->setCurrentIndexByData(currentLocation);
    }
}

void Wild5::generatorLocationIndexChanged(int index)
{
    if (index >= 0)
    {
        auto &area = encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()];
        auto species = area.getUniqueSpecies();
        auto names = area.getSpecieNames();

        ui->filterGenerator->setEncounterSlots(area.getCount());

        ui->comboBoxGeneratorPokemon->clear();
        ui->comboBoxGeneratorPokemon->addItem("-");
        for (size_t i = 0; i < species.size(); i++)
        {
            ui->comboBoxGeneratorPokemon->addItem(QString::fromStdString(names[i]), species[i]);
        }
    }
}

void Wild5::generatorPokemonIndexChanged(int index)
{
    if (index <= 0)
    {
        ui->filterGenerator->resetEncounterSlots();
        ui->spinBoxGeneratorLevelMin->setValue(0);
        ui->spinBoxGeneratorLevelMax->setValue(0);
        ui->filterGenerator->setLevelRange(1, 100);
    }
    else
    {
        u16 num = ui->comboBoxGeneratorPokemon->getCurrentUShort();
        auto flags = encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()].getSlots(num);
        ui->filterGenerator->toggleEncounterSlots(flags);

        auto range = encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()].getLevelRange(num);
        ui->spinBoxGeneratorLevelMin->setValue(range.first);
        ui->spinBoxGeneratorLevelMax->setValue(range.second);
        ui->filterGenerator->setLevelRange(range.first, range.second);
    }
}

void Wild5::generatorSeasonIndexChanged(int index)
{
    if (index >= 0)
    {
        generatorEncounterIndexChanged(0);
    }
}

void Wild5::openAdjacentSeeds()
{
    QModelIndex index = proxyModel->mapToSource(ui->tableViewSearcher->currentIndex());
    const auto &state = searcherModel->getItem(index.row());

    auto *window = new AdjacentSeeds(false, state.getButtons(), state.getDateTime(), *currentProfile);
    window->show();
}

void Wild5::openAdvanceFinder()
{
    auto *advanceFinder = new AdvanceFinder(generatorModel, ui->tableViewGenerator, currentProfile, this);
    advanceFinder->show();
}

void Wild5::profileChanged(const Profile5 &profile)
{
    currentProfile = &profile;

    if (ivCache)
    {
        delete ivCache;
        ivCache = nullptr;
    }

    if (shaCache)
    {
        delete shaCache;
        shaCache = nullptr;
    }

    auto ivCachePath = currentProfile->getIVCache();
    if (!ivCachePath.empty())
    {
        ivCache = new IVCache(ivCachePath);
    }

    auto shaCachePath = currentProfile->getSHACache();
    if (!shaCachePath.empty())
    {
        shaCache = new SHA1Cache(shaCachePath);
        ui->dateEditSearcherStartDate->setDateRange(shaCache->getStartDate(), shaCache->getEndDate());
        ui->dateEditSearcherEndDate->setDateRange(shaCache->getStartDate(), shaCache->getEndDate());
    }
    else
    {
        ui->dateEditSearcherStartDate->clearDateRange();
        ui->dateEditSearcherEndDate->clearDateRange();
    }

    bool flag = (currentProfile->getVersion() & Game::BW2) != Game::None;

    ui->labelGeneratorLuckyPower->setVisible(flag);
    ui->comboBoxGeneratorLuckyPower->setVisible(flag);

    ui->labelSearcherLuckyPower->setVisible(flag);
    ui->comboBoxSearcherLuckyPower->setVisible(flag);
    if (!flag)
    {
        ui->comboBoxGeneratorLuckyPower->setCheckedData({ PassPower5::None });
        ui->comboBoxSearcherLuckyPower->setCheckedData({ PassPower5::None });
    }
    updateGeneratorPassPowerActions(ui->comboBoxGeneratorLuckyPower, (currentProfile->getVersion() & Game::BW) != Game::None);

    generatorEncounterIndexChanged(0);
    searcherEncounterIndexChanged(0);

    searcherFastSearchChanged();
}

void Wild5::search()
{
    Date start = ui->dateEditSearcherStartDate->getDate();
    Date end = ui->dateEditSearcherEndDate->getDate();
    if (start > end)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid date range"), tr("Start date is after end date"));
        msg.exec();
        return;
    }

    if (!ui->filterSearcher->isValid(ui->spinBoxSearcherLevelMin->value(), ui->spinBoxSearcherLevelMax->value()))
    {
        return;
    }

    searcherModel->clearModel();

    ui->pushButtonSearch->setEnabled(false);
    ui->pushButtonCancel->setEnabled(true);

    u32 initialIVAdvances = ui->textBoxSearcherInitialIVAdvances->getUInt();
    u32 maxIVAdvances = ui->textBoxSearcherMaxIVAdvances->getUInt();
    u32 initialAdvances = ui->textBoxSearcherInitialAdvances->getUInt();
    u32 maxAdvances = ui->textBoxSearcherMaxAdvances->getUInt();
    auto leads = getSearcherLeads(ui->comboMenuSearcherLead);
    bool searchMovingTrigger
        = ui->checkBoxSearcherMovingTrigger->isChecked() && supportsMovingTrigger(ui->comboBoxSearcherEncounter->getEnum<Encounter>(), currentProfile);
    auto passPowers = (currentProfile->getVersion() & Game::BW2) != Game::None
        ? getPassPowers(getCheckedUChars(ui->comboBoxSearcherLuckyPower), searchMovingTrigger,
                        (currentProfile->getVersion() & Game::BW) != Game::None)
        : std::vector<u8> { PassPower5::None };
    bool showPassPower = hasPassPower(passPowers);
    searcherModel->setShowPassPower(showPassPower);

    auto filter = ui->filterSearcher->getFilter<WildStateFilter, true>();
    WildGenerator5 generator(initialAdvances, maxAdvances, 0, Method::Method5, leads, passPowers, searchMovingTrigger, searchMovingTrigger,
                             encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()], *currentProfile, filter, true);

    SearcherBase5<WildGenerator5, WildState5> *searcher;
    if (fastSearchEnabled())
    {
        auto ivMap = ivCache->getCache(initialIVAdvances, maxIVAdvances, currentProfile->getVersion(), CacheType::Normal, filter);
        if (shaCache && shaCache->isValid(*currentProfile))
        {
            auto shaMap = shaCache->getCache(initialIVAdvances, maxIVAdvances, start, end, ivMap, CacheType::Normal, *currentProfile);
            searcher = new WildSearcher5CacheFast(initialIVAdvances, maxIVAdvances, shaMap, ivMap, generator, *currentProfile);
        }
        else
        {
            searcher = new WildSearcher5Fast(initialIVAdvances, maxIVAdvances, ivMap, generator, *currentProfile);
        }
    }
    else
    {
        searcher = new WildSearcher5(initialIVAdvances, maxIVAdvances, generator, *currentProfile);
    }

    searcher->setMaxProgress(searcher->getMaxProgress(start, end));

    QSettings settings;
    int threads = settings.value("settings/threads").toInt();

    auto *timer = new QTimer(this);
    connect(ui->pushButtonCancel, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        ui->pushButtonCancel->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer, showPassPower] {
        searcherModel->addItems(searcher->getResults());
        if (showPassPower)
        {
            ui->tableViewSearcher->resizeColumnToContents(2);
        }
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

void Wild5::searcherEncounterIndexChanged(int index)
{
    if (index >= 0)
    {
        auto encounter = ui->comboBoxSearcherEncounter->getEnum<Encounter>();
        u16 currentLocation = ui->comboBoxSearcherLocation->getCurrentUShort();
        bool movingTrigger = supportsMovingTrigger(encounter, currentProfile);
        ui->checkBoxSearcherMovingTrigger->setEnabled(movingTrigger);
        if (!movingTrigger)
        {
            ui->checkBoxSearcherMovingTrigger->setChecked(false);
        }
        bool grass = encounter == Encounter::Grass;

        ui->checkBoxSearcherSwarm->setVisible(grass);
        if (!grass && ui->checkBoxSearcherSwarm->isChecked())
        {
            ui->checkBoxSearcherSwarm->setChecked(false);
        }

        EncounterSettings5 settings = { };
        settings.swarm = ui->checkBoxSearcherSwarm->isChecked();
        settings.season = ui->comboBoxSearcherSeason->currentIndex();

        bool fish = encounter == Encounter::SuperRod;

        ui->comboMenuSearcherLead->hideAction(toInt(Lead::SuctionCups), !fish);

        encounterSearcher = Encounters5::getEncounters(encounter, settings, currentProfile);

        std::vector<u16> locs;
        std::ranges::transform(encounterSearcher, std::back_inserter(locs), [](const EncounterArea5 &area) { return area.getLocation(); });

        ui->comboBoxSearcherLocation->clear();
        ui->comboBoxSearcherLocation->addItems(Translator::getLocations(locs, currentProfile->getVersion()), locs);
        ui->comboBoxSearcherLocation->setCurrentIndexByData(currentLocation);
    }
}

void Wild5::searcherFastSearchChanged()
{
    if (fastSearchEnabled())
    {
        if (shaCache && shaCache->isValid(*currentProfile))
        {
            ui->labelIVFastSearch->setText(tr("Settings are configured for fast IV/SHA searching"));
        }
        else
        {
            ui->labelIVFastSearch->setText(
                tr("Settings are configured for fast IV searching.\nProfile is missing or has an incompatible SHA cache."));
        }
    }
    else
    {
        if (ivCache == nullptr)
        {
            ui->labelIVFastSearch->setText(tr("Profile does not have a IV cache file configured"));
        }
        else
        {
            QStringList text
                = { tr("Settings are not configured for fast searching"),
                    tr("Keep initial/max advances below %1/%2").arg(ivCache->getInitialAdvances()).arg(ivCache->getMaxAdvances()),
                    tr("Ensure IV filters are set to common spreads") };
            ui->labelIVFastSearch->setText(text.join('\n'));
        }
    }
}

void Wild5::searcherLocationIndexChanged(int index)
{
    if (index >= 0)
    {
        auto &area = encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()];
        auto species = area.getUniqueSpecies();
        auto names = area.getSpecieNames();

        ui->filterSearcher->setEncounterSlots(area.getCount());

        ui->comboBoxSearcherPokemon->clear();
        ui->comboBoxSearcherPokemon->addItem("-");
        for (size_t i = 0; i < species.size(); i++)
        {
            ui->comboBoxSearcherPokemon->addItem(QString::fromStdString(names[i]), species[i]);
        }
    }
}

void Wild5::searcherPokemonIndexChanged(int index)
{
    if (index <= 0)
    {
        ui->filterSearcher->resetEncounterSlots();
        ui->spinBoxSearcherLevelMin->setValue(0);
        ui->spinBoxSearcherLevelMax->setValue(0);
        ui->filterSearcher->setLevelRange(1, 100);
    }
    else
    {
        u16 num = ui->comboBoxSearcherPokemon->getCurrentUShort();
        auto flags = encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()].getSlots(num);
        ui->filterSearcher->toggleEncounterSlots(flags);

        auto range = encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()].getLevelRange(num);
        ui->spinBoxSearcherLevelMin->setValue(range.first);
        ui->spinBoxSearcherLevelMax->setValue(range.second);
        ui->filterSearcher->setLevelRange(range.first, range.second);
    }
}

void Wild5::searcherSeasonIndexChanged(int index)
{
    if (index >= 0)
    {
        searcherEncounterIndexChanged(0);
    }
}

void Wild5::transferFilters(int index)
{
    if (index == 0)
    {
        ui->filterSearcher->copyFrom(ui->filterGenerator);
    }
    else
    {
        ui->filterGenerator->copyFrom(ui->filterSearcher);
    }
}

void Wild5::transferSettings(int index)
{
    if (index == 0)
    {
        ui->comboBoxSearcherEncounter->setCurrentIndex(ui->comboBoxGeneratorEncounter->currentIndex());
        ui->comboBoxSearcherLocation->setCurrentIndex(ui->comboBoxGeneratorLocation->currentIndex());
        ui->comboBoxSearcherPokemon->setCurrentIndex(ui->comboBoxGeneratorPokemon->currentIndex());
        ui->comboBoxSearcherSeason->setCurrentIndex(ui->comboBoxGeneratorSeason->currentIndex());
        ui->checkBoxSearcherMovingTrigger->setChecked(ui->checkBoxGeneratorMovingTrigger->isChecked());
        ui->checkBoxSearcherSwarm->setChecked(ui->checkBoxGeneratorSwarm->isChecked());
        ui->comboBoxSearcherLuckyPower->setCheckedData(getPassPowerMenuOptions(getGeneratorPassPower(ui->comboBoxGeneratorLuckyPower)));
    }
    else
    {
        ui->comboBoxGeneratorEncounter->setCurrentIndex(ui->comboBoxSearcherEncounter->currentIndex());
        ui->comboBoxGeneratorLocation->setCurrentIndex(ui->comboBoxSearcherLocation->currentIndex());
        ui->comboBoxGeneratorPokemon->setCurrentIndex(ui->comboBoxSearcherPokemon->currentIndex());
        ui->comboBoxGeneratorSeason->setCurrentIndex(ui->comboBoxSearcherSeason->currentIndex());
        ui->checkBoxGeneratorMovingTrigger->setChecked(ui->checkBoxSearcherMovingTrigger->isChecked());
        ui->checkBoxGeneratorSwarm->setChecked(ui->checkBoxSearcherSwarm->isChecked());
        auto passPowers = (currentProfile->getVersion() & Game::BW2) != Game::None
            ? getPassPowers(getCheckedUChars(ui->comboBoxSearcherLuckyPower), ui->checkBoxSearcherMovingTrigger->isChecked(),
                            (currentProfile->getVersion() & Game::BW) != Game::None)
            : std::vector<u8> { PassPower5::None };
        ui->comboBoxGeneratorLuckyPower->setCheckedData(getPassPowerMenuOptions(passPowers.empty() ? PassPower5::None : passPowers.front()));
        updateGeneratorPassPowerActions(ui->comboBoxGeneratorLuckyPower, (currentProfile->getVersion() & Game::BW) != Game::None);
    }
}
