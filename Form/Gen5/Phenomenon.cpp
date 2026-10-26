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

#include "Phenomenon.hpp"
#include "ui_Phenomenon.h"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen5/EncounterArea5.hpp>
#include <Core/Gen5/Encounters5.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Gen5/IVCache.hpp>
#include <Core/Gen5/PhenomenonArea.hpp>
#include <Core/Gen5/PhenomenonMap.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/SHA1Cache.hpp>
#include <Core/Gen5/Searchers/IVSearcher5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Parents/Filters/StateFilter.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/CheckList.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/Controls.hpp>
#include <Form/Controls/ResponsiveGrid.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <Form/Gen5/Tools/PhenomenonItem.hpp>
#include <Form/Gen5/ResultToGenerator.hpp>
#include <Form/Gen5/Tools/AdjacentSeeds.hpp>
#include <Form/Util/AdvanceFinder.hpp>
#include <Model/Gen5/WildModel5.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace
{
    constexpr u16 noItem = 0xffff;

    bool supportsEncounterModifier(Encounter encounter)
    {
        return encounter == Encounter::DustCloud || encounter == Encounter::FlyingShadow;
    }

    PhenomenonTileType getPhenomenonEncounterType(Encounter encounter)
    {
        switch (encounter)
        {
        case Encounter::GrassRustling:
            return PhenomenonTileType::Grass;
        case Encounter::DustCloud:
            return PhenomenonTileType::Cave;
        case Encounter::SurfingRippling:
        case Encounter::SuperRodRippling:
            return PhenomenonTileType::Water;
        case Encounter::FlyingShadow:
            return PhenomenonTileType::Bridge;
        default:
            return PhenomenonTileType::None;
        }
    }

    QString getPhenomenonTileColor(PhenomenonTileType type, bool strong)
    {
        switch (type)
        {
        case PhenomenonTileType::Grass:
            return strong ? QStringLiteral("#1f5f2f") : QStringLiteral("#7fbd73");
        case PhenomenonTileType::Cave:
            return strong ? QStringLiteral("#4f4b3d") : QStringLiteral("#afa981");
        case PhenomenonTileType::Water:
            return strong ? QStringLiteral("#0022ab") : QStringLiteral("#2980ef");
        case PhenomenonTileType::Bridge:
            return strong ? QStringLiteral("#215bab") : QStringLiteral("#81b9ef");
        default:
            return QString();
        }
    }

    std::vector<u8> getCheckedUChars(const ComboMenu *comboMenu)
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

    std::vector<Lead> getSearcherLeads(const ComboMenu *comboMenu)
    {
        auto data = comboMenu->getCheckedData();
        std::vector<Lead> leads;
        leads.reserve(data.size());
        for (int value : data)
        {
            leads.emplace_back(static_cast<Lead>(value));
        }

        if (leads.empty())
        {
            leads.emplace_back(Lead::None);
        }

        std::ranges::sort(leads);
        leads.erase(std::ranges::unique(leads).begin(), leads.end());
        return leads;
    }

    bool hasPassPower(const std::vector<u8> &powers)
    {
        return std::ranges::find_if(powers, [](u8 power) { return power != PassPower5::None; }) != powers.end();
    }

    bool isLuckyPower(int power)
    {
        return power >= PassPower5::Lucky1 && power <= PassPower5::Lucky3;
    }

    bool isExploringPower(int power)
    {
        return power >= PassPower5::Exploring1 && power <= PassPower5::Exploring3 && PassPower5::getLuckyPower(power) == PassPower5::None;
    }

    std::vector<int> normalizeGeneratorPowers(const std::vector<int> &powers, const std::vector<int> &previous)
    {
        if (std::ranges::contains(powers, static_cast<int>(PassPower5::None))
            && !std::ranges::contains(previous, static_cast<int>(PassPower5::None)))
        {
            return { PassPower5::None };
        }

        auto selectPower = [&](auto predicate) {
            for (int power : previous)
            {
                if (predicate(power) && std::ranges::contains(powers, power))
                {
                    return power;
                }
            }
            auto power = std::ranges::find_if(powers, predicate);
            return power == powers.end() ? static_cast<int>(PassPower5::None) : *power;
        };

        int luckyPower = selectPower(isLuckyPower);
        int exploringPower = selectPower(isExploringPower);
        std::vector<int> normalized;
        if (luckyPower != PassPower5::None)
        {
            normalized.emplace_back(luckyPower);
        }
        if (exploringPower != PassPower5::None)
        {
            normalized.emplace_back(exploringPower);
        }
        return normalized.empty() ? std::vector<int> { PassPower5::None } : normalized;
    }

    std::vector<int> getGeneratorPowerProperty(const ComboMenu *comboMenu)
    {
        std::vector<int> powers;
        for (const auto &value : comboMenu->property("phenomenonGeneratorPowers").toList())
        {
            powers.emplace_back(value.toInt());
        }
        return powers;
    }

    void setGeneratorPowers(ComboMenu *comboMenu, const std::vector<int> &powers)
    {
        QVariantList values;
        for (int power : powers)
        {
            values.emplace_back(power);
        }
        comboMenu->setProperty("phenomenonGeneratorPowers", values);
        comboMenu->setCheckedData(powers);
    }

    u8 getGeneratorPassPower(ComboMenu *comboMenu)
    {
        auto powers = normalizeGeneratorPowers(comboMenu->getRawCheckedData(), getGeneratorPowerProperty(comboMenu));

        u8 luckyPower = PassPower5::None;
        u8 exploringPower = PassPower5::None;
        for (int power : powers)
        {
            if (isLuckyPower(power))
            {
                luckyPower = PassPower5::getLuckyPower(power);
            }
            else if (isExploringPower(power))
            {
                exploringPower = PassPower5::getExploringPower(power);
            }
        }
        return PassPower5::combineExploring(luckyPower, exploringPower);
    }

    std::vector<u8> getLuckyPowers(std::vector<u8> powers, bool bw)
    {
        if (bw)
        {
            return { PassPower5::None };
        }

        if (powers.empty())
        {
            powers.emplace_back(PassPower5::None);
        }

        std::ranges::sort(powers);
        powers.erase(std::ranges::unique(powers).begin(), powers.end());
        return powers;
    }

    WildStateFilter getUnfilteredWildStateFilter()
    {
        std::array<u8, 6> ivMin { };
        std::array<u8, 6> ivMax;
        ivMax.fill(31);

        std::array<bool, 25> natures;
        natures.fill(true);

        std::array<bool, 16> powers;
        powers.fill(true);

        StackVector<bool, 13> encounterSlots;
        encounterSlots.fill(true);

        return WildStateFilter(255, 255, 255, 1, 100, 0, 255, 0, 255, true, ivMin, ivMax, natures, powers, encounterSlots);
    }
}

Phenomenon::Phenomenon(QWidget *parent) : QWidget(parent), ui(new Ui::Phenomenon), ivCache(nullptr), shaCache(nullptr)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    itemTool = new PhenomenonItem(true, ui->tabRNGSelector);
    ui->tabRNGSelector->addTab(itemTool, tr("Items"));
    connect(itemTool, &PhenomenonItem::openGenerator, this,
            [this](const Profile5 &profile, Encounter encounter, u8 location, u64 seed, u8 exploringPower,
                   const std::vector<u32> &targetAdvances) {
                configureGenerator(profile, encounter, location, seed, exploringPower, targetAdvances);
                ui->tabRNGSelector->setCurrentIndex(0);
            });

    generatorModel = new WildGeneratorModel5(ui->tableViewGenerator);
    searcherModel = new WildSearcherModel5(ui->tableViewSearcher);
    proxyModel = new SortFilterProxyModel(ui->tableViewSearcher, searcherModel);
    generatorModel->setShowPhenomenon(true);

    ui->tableViewGenerator->setModel(generatorModel);
    ui->tableViewSearcher->setModel(proxyModel);
    setupPhenomenonField();

    ui->textBoxGeneratorSeed->setValues(InputType::Seed64Bit);
    ui->textBoxGeneratorIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorInitialAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorMaxAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxGeneratorMaxAdvances->setText(QStringLiteral("1000"));
    ui->textBoxGeneratorOffset->setValues(InputType::Advance32Bit);

    ui->textBoxSearcherInitialIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherMaxIVAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherInitialAdvances->setValues(InputType::Advance32Bit);
    ui->textBoxSearcherMaxAdvances->setValues(InputType::Advance32Bit);

    ui->comboBoxGeneratorEncounter->setItemText(0, tr("Rustling Grass"));
    ui->comboBoxGeneratorEncounter->setItemText(1, tr("Dust Cloud"));
    ui->comboBoxGeneratorEncounter->setItemText(2, tr("Rippling Surfing"));
    ui->comboBoxGeneratorEncounter->setItemText(3, tr("Rippling Fishing"));
    ui->comboBoxGeneratorEncounter->setItemText(4, tr("Flying Shadow"));
    ui->comboBoxGeneratorEncounter->setup({ toInt(Encounter::GrassRustling), toInt(Encounter::DustCloud), toInt(Encounter::SurfingRippling),
                                            toInt(Encounter::SuperRodRippling), toInt(Encounter::FlyingShadow) });

    ui->comboBoxSearcherEncounter->setItemText(0, tr("Rustling Grass"));
    ui->comboBoxSearcherEncounter->setItemText(1, tr("Dust Cloud"));
    ui->comboBoxSearcherEncounter->setItemText(2, tr("Rippling Surfing"));
    ui->comboBoxSearcherEncounter->setItemText(3, tr("Rippling Fishing"));
    ui->comboBoxSearcherEncounter->setItemText(4, tr("Flying Shadow"));
    ui->comboBoxSearcherEncounter->setup({ toInt(Encounter::GrassRustling), toInt(Encounter::DustCloud), toInt(Encounter::SurfingRippling),
                                           toInt(Encounter::SuperRodRippling), toInt(Encounter::FlyingShadow) });

    checkListGeneratorItem = new CheckList(ui->groupBoxGeneratorSettings);
    checkListGeneratorItem->setUncheckedText(tr("Any"));
    labelGeneratorItem = new QLabel(tr("Item"), ui->groupBoxGeneratorSettings);
    auto *generatorSettingsLayout = qobject_cast<QGridLayout *>(ui->groupBoxGeneratorSettings->layout());
    auto moveLayoutItem = [](QGridLayout *layout, int row, int column, int newRow, int newColumn, int rowSpan = 1, int columnSpan = 1) {
        if (QLayoutItem *item = layout->itemAtPosition(row, column))
        {
            layout->removeItem(item);
            layout->addItem(item, newRow, newColumn, rowSpan, columnSpan);
        }
    };
    moveLayoutItem(generatorSettingsLayout, 5, 0, 6, 0);
    moveLayoutItem(generatorSettingsLayout, 5, 1, 6, 1);
    moveLayoutItem(generatorSettingsLayout, 5, 2, 6, 2);
    moveLayoutItem(generatorSettingsLayout, 4, 0, 5, 0, 1, 3);
    generatorSettingsLayout->addWidget(labelGeneratorItem, 4, 0);
    generatorSettingsLayout->addWidget(checkListGeneratorItem, 4, 1, 1, 2);

    ui->filterGenerator->disableControls(Controls::Height | Controls::Weight);
    ui->filterSearcher->disableControls(Controls::DisableFilter | Controls::Height | Controls::Weight);

    ui->comboMenuGeneratorLead->addAction(tr("None"), toInt(Lead::None));
    ui->comboMenuGeneratorLead->addAction(tr("Compound Eyes"), toInt(Lead::CompoundEyes));
    ui->comboMenuGeneratorLead->addMenu(tr("Cute Charm"),
                                        { { tr("♂ Lead"), toInt(Lead::CuteCharmM) }, { tr("♀ Lead"), toInt(Lead::CuteCharmF) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Encounter Modifier"),
                                        { { tr("Arena Trap"), toInt(Lead::ArenaTrap) },
                                          { tr("Illuminate"), toInt(Lead::Illuminate) },
                                          { tr("No Guard"), toInt(Lead::NoGuard) },
                                          { tr("Quick Feet"), toInt(Lead::QuickFeet) },
                                          { tr("Stench"), toInt(Lead::Stench) },
                                          { tr("Sticky Hold"), toInt(Lead::StickyHold) },
                                          { tr("Suction Cups"), toInt(Lead::SuctionCups) },
                                          { tr("White Smoke"), toInt(Lead::WhiteSmoke) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Level Modifier"),
                                        { { tr("Hustle"), toInt(Lead::Hustle) },
                                          { tr("Pressure"), toInt(Lead::Pressure) },
                                          { tr("Vital Spirit"), toInt(Lead::VitalSpirit) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Slot Modifier"),
                                        { { tr("Magnet Pull"), toInt(Lead::MagnetPull) }, { tr("Static"), toInt(Lead::Static) } });
    ui->comboMenuGeneratorLead->addMenu(tr("Synchronize"), Translator::getNatures());

    ui->comboMenuSearcherLead->addAction(tr("None"), toInt(Lead::None));
    ui->comboMenuSearcherLead->addAction(tr("Compound Eyes"), toInt(Lead::CompoundEyes));
    ui->comboMenuSearcherLead->addMenu(tr("Cute Charm"),
                                       { { tr("♂ Lead"), toInt(Lead::CuteCharmM) }, { tr("♀ Lead"), toInt(Lead::CuteCharmF) } });
    ui->comboMenuSearcherLead->addMenu(tr("Encounter Modifier"),
                                       { { tr("Arena Trap"), toInt(Lead::ArenaTrap) },
                                         { tr("Illuminate"), toInt(Lead::Illuminate) },
                                         { tr("No Guard"), toInt(Lead::NoGuard) },
                                         { tr("Quick Feet"), toInt(Lead::QuickFeet) },
                                         { tr("Stench"), toInt(Lead::Stench) },
                                         { tr("Sticky Hold"), toInt(Lead::StickyHold) },
                                         { tr("Suction Cups"), toInt(Lead::SuctionCups) },
                                         { tr("White Smoke"), toInt(Lead::WhiteSmoke) } });
    ui->comboMenuSearcherLead->addMenu(tr("Level Modifier"),
                                       { { tr("Hustle"), toInt(Lead::Hustle) },
                                         { tr("Pressure"), toInt(Lead::Pressure) },
                                         { tr("Vital Spirit"), toInt(Lead::VitalSpirit) } });
    ui->comboMenuSearcherLead->addMenu(tr("Slot Modifier"),
                                       { { tr("Magnet Pull"), toInt(Lead::MagnetPull) }, { tr("Static"), toInt(Lead::Static) } });
    ui->comboMenuSearcherLead->addMenu(tr("Synchronize"), Translator::getNatures());
    ui->comboMenuSearcherLead->setMultiSelect(true);
    ui->comboMenuSearcherLead->setCheckedData({ toInt(Lead::None) });
    ui->comboMenuSearcherLead->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    ui->comboBoxGeneratorLocation->enableAutoComplete();
    ui->comboBoxSearcherLocation->enableAutoComplete();

    ui->comboBoxGeneratorLuckyPower->setMultiSelect(true);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("None"), PassPower5::None);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Lucky Power ↑"), PassPower5::Lucky1);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Lucky Power ↑↑"), PassPower5::Lucky2);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Lucky Power ↑↑↑ / S"), PassPower5::Lucky3);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Exploring Power ↑"), PassPower5::Exploring1);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Exploring Power ↑↑"), PassPower5::Exploring2);
    ui->comboBoxGeneratorLuckyPower->addAction(tr("Exploring Power ↑↑↑ / S"), PassPower5::Exploring3);
    ui->comboBoxGeneratorLuckyPower->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setGeneratorPowers(ui->comboBoxGeneratorLuckyPower, { PassPower5::None });
    ui->comboBoxSearcherLuckyPower->setMultiSelect(true);
    ui->comboBoxSearcherLuckyPower->addAction(tr("None"), PassPower5::None);
    ui->comboBoxSearcherLuckyPower->addAction(tr("↑"), PassPower5::Lucky1);
    ui->comboBoxSearcherLuckyPower->addAction(tr("↑↑"), PassPower5::Lucky2);
    ui->comboBoxSearcherLuckyPower->addAction(tr("↑↑↑ / S"), PassPower5::Lucky3);
    ui->comboBoxSearcherLuckyPower->setCheckedData({ PassPower5::None });
    ui->comboBoxSearcherLuckyPower->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    auto *advanceFinder = ui->tableViewGenerator->addAction(tr("Advance Finder"));
    ui->tableViewGenerator->setPrimaryAction(advanceFinder);
    connect(advanceFinder, &QAction::triggered, this, &Phenomenon::openAdvanceFinder);

    auto *adjacentSeeds = ui->tableViewSearcher->addAction(tr("Adjacent Seeds"));
    ui->tableViewSearcher->setPrimaryAction(adjacentSeeds);
    connect(adjacentSeeds, &QAction::triggered, this, &Phenomenon::openAdjacentSeeds);
    auto *goToGenerator = ui->tableViewSearcher->addAction(tr("Go to Generator"));
    ui->tableViewSearcher->setSecondaryAction(goToGenerator);
    connect(goToGenerator, &QAction::triggered, this, &Phenomenon::goToGenerator);
    auto *removeTargetMark = ui->tableViewGenerator->addAction(tr("Remove target Mark"));
    ui->tableViewGenerator->setRemoveTargetAction(removeTargetMark);
    connect(removeTargetMark, &QAction::triggered, ui->tableViewGenerator, &TableView::clearTargetAdvance);
    auto *markPhenomenon = ui->tableViewGenerator->addAction(tr("Mark Phenomenon"));
    connect(markPhenomenon, &QAction::triggered, this, &Phenomenon::markPhenomenon);

    connect(ui->comboBoxProfiles, &QComboBox::currentIndexChanged, this, &Phenomenon::profileIndexChanged);
    connect(ui->tabRNGSelector, &TabWidget::transferFilters, this, &Phenomenon::transferFilters);
    connect(ui->tabRNGSelector, &TabWidget::transferSettings, this, &Phenomenon::transferSettings);
    connect(ui->pushButtonGenerate, &QPushButton::clicked, this, &Phenomenon::generate);
    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &Phenomenon::search);
    connect(ui->comboBoxGeneratorEncounter, &QComboBox::currentIndexChanged, this, &Phenomenon::generatorEncounterIndexChanged);
    connect(ui->comboBoxGeneratorLuckyPower, &ComboMenu::checkedDataChanged, this, [this] {
        QSignalBlocker blocker(ui->comboBoxGeneratorLuckyPower);
        auto powers = normalizeGeneratorPowers(ui->comboBoxGeneratorLuckyPower->getRawCheckedData(),
                                               getGeneratorPowerProperty(ui->comboBoxGeneratorLuckyPower));
        setGeneratorPowers(ui->comboBoxGeneratorLuckyPower, powers);
    });
    connect(ui->comboBoxSearcherEncounter, &QComboBox::currentIndexChanged, this, &Phenomenon::searcherEncounterIndexChanged);
    connect(ui->comboBoxGeneratorLocation, &QComboBox::currentIndexChanged, this, &Phenomenon::generatorLocationIndexChanged);
    connect(ui->comboBoxSearcherLocation, &QComboBox::currentIndexChanged, this, &Phenomenon::searcherLocationIndexChanged);
    connect(ui->comboBoxGeneratorPokemon, &QComboBox::currentIndexChanged, this, &Phenomenon::generatorPokemonIndexChanged);
    connect(ui->comboBoxSearcherPokemon, &QComboBox::currentIndexChanged, this, &Phenomenon::searcherPokemonIndexChanged);
    connect(ui->comboBoxGeneratorSeason, &QComboBox::currentIndexChanged, this, &Phenomenon::generatorSeasonIndexChanged);
    connect(ui->comboBoxSearcherSeason, &QComboBox::currentIndexChanged, this, &Phenomenon::searcherSeasonIndexChanged);
    connect(ui->pushButtonProfileManager, &QPushButton::clicked, this, &Phenomenon::profileManager);
    connect(ui->filterGenerator, &Filter::showStatsChanged, generatorModel, &WildGeneratorModel5::setShowStats);
    connect(ui->filterSearcher, &Filter::showStatsChanged, searcherModel, &WildSearcherModel5::setShowStats);
    connect(ui->comboBoxProfiles, &QComboBox::currentIndexChanged, this, &Phenomenon::searcherFastSearchChanged);
    connect(ui->filterSearcher, &Filter::ivsChanged, this, &Phenomenon::searcherFastSearchChanged);
    connect(ui->textBoxSearcherInitialIVAdvances, &TextBox::textChanged, this, &Phenomenon::searcherFastSearchChanged);
    connect(ui->textBoxSearcherMaxIVAdvances, &TextBox::textChanged, this, &Phenomenon::searcherFastSearchChanged);
    updateProfiles();
    if (hasProfiles())
    {
        generatorEncounterIndexChanged(0);
        searcherEncounterIndexChanged(0);
    }
    searcherFastSearchChanged();

    QSettings setting;
    setting.beginGroup("phenomenon");
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

Phenomenon::~Phenomenon()
{
    QSettings setting;
    setting.beginGroup("phenomenon");
    setting.setValue("profile", ui->comboBoxProfiles->currentIndex());
    setting.setValue("geometry", this->saveGeometry());
    setting.setValue("startDate", ui->dateEditSearcherStartDate->date());
    setting.setValue("endDate", ui->dateEditSearcherEndDate->date());
    setting.endGroup();

    delete ivCache;
    delete shaCache;
    delete ui;
}

bool Phenomenon::hasProfiles() const
{
    return !profiles.empty();
}

void Phenomenon::configureGenerator(const Profile5 &profile, Encounter encounter, u8 location, u64 seed, u8 exploringPower,
                                    const std::vector<u32> &targetAdvances)
{
    auto profileIt = std::ranges::find(profiles, profile);
    if (profileIt != profiles.end())
    {
        ui->comboBoxProfiles->setCurrentIndex(static_cast<int>(std::distance(profiles.begin(), profileIt)));
    }

    ui->tabRNGSelector->setCurrentIndex(0);
    ui->comboBoxGeneratorEncounter->setCurrentIndex(ui->comboBoxGeneratorEncounter->findData(toInt(encounter)));
    ui->comboBoxGeneratorLocation->setCurrentIndexByData(location);
    ui->textBoxGeneratorSeed->setText(QString::number(seed, 16).toUpper());
    setGeneratorPowers(ui->comboBoxGeneratorLuckyPower,
                       { exploringPower == 0 ? PassPower5::None : exploringPower << PassPower5::ExploringShift });
    if (!targetAdvances.empty())
    {
        ui->textBoxGeneratorMaxAdvances->setText(
            QString::number(ResultToGenerator5::getMaxAdvances(seed, targetAdvances.back(), 50, profile)));
    }
    ui->tableViewGenerator->setTargetAdvances(targetAdvances);
    generate();
}

void Phenomenon::updateProfiles()
{
    profiles = ProfileLoader5::getProfiles(Game::Gen5);

    ui->comboBoxProfiles->clear();
    for (const auto &profile : profiles)
    {
        ui->comboBoxProfiles->addItem(QString::fromStdString(profile.getName()));
    }

    QSettings setting;
    int val = setting.value("phenomenon/profile", 0).toInt();
    if (val < ui->comboBoxProfiles->count())
    {
        ui->comboBoxProfiles->setCurrentIndex(val);
    }
}
bool Phenomenon::fastSearchEnabled() const
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

bool Phenomenon::removeByGeneratorFilters(const WildState5 &state) const
{
    if (state.getPhenomenonItem())
    {
        auto itemCheckState = checkListGeneratorItem->getCheckState();
        bool pokemonSelected = ui->comboBoxGeneratorPokemon->currentIndex() > 0;
        bool itemFilterAny = itemCheckState == Qt::Unchecked || itemCheckState == Qt::Checked;
        if (pokemonSelected && itemFilterAny)
        {
            return true;
        }

        if (itemFilterAny)
        {
            return false;
        }

        auto items = checkListGeneratorItem->getCheckedData();
        return std::find(items.begin(), items.end(), state.getItem()) == items.end();
    }

    return false;
}

bool Phenomenon::removeBySearcherFilters(const WildState5 &state) const
{
    return state.getPhenomenonItem();
}

void Phenomenon::updateItemFilter(QWidget *itemLabel, CheckList *itemFilter, EncounterArea5 &area)
{
    itemFilter->clear();

    PhenomenonType type;
    if (area.getEncounter() == Encounter::DustCloud)
    {
        type = PhenomenonType::Cave;
    }
    else if (area.getEncounter() == Encounter::FlyingShadow)
    {
        type = PhenomenonType::Bridge;
    }
    else
    {
        itemLabel->setVisible(false);
        itemFilter->setVisible(false);
        return;
    }

    PhenomenonArea phenomenonArea(area.getLocation(), type);
    auto items = phenomenonArea.getUniqueItems();
    auto names = phenomenonArea.getItemNames();

    std::vector<std::pair<QString, u16>> itemEntries;
    itemEntries.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        itemEntries.emplace_back(QString::fromStdString(names[i]), items[i]);
    }
    std::ranges::sort(itemEntries, { }, &std::pair<QString, u16>::first);

    itemFilter->addItem(tr("None"), noItem);
    for (const auto &[name, item] : itemEntries)
    {
        itemFilter->addItem(name, item);
    }
    itemFilter->resetChecks();

    itemLabel->setVisible(true);
    itemFilter->setVisible(true);
}

void Phenomenon::setupPhenomenonField()
{
    auto *tabLayout = qobject_cast<QGridLayout *>(ui->tabGenerator->layout());
    phenomenonPositionGroup = new QGroupBox(tr("Phenomenon Tiles"), ui->tabGenerator);
    auto *outerLayout = new QVBoxLayout(phenomenonPositionGroup);
    auto *tileWidget = new QWidget(phenomenonPositionGroup);
    auto *layout = new QGridLayout(tileWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setHorizontalSpacing(1);
    layout->setVerticalSpacing(1);

    for (int x = 0; x < 11; x++)
    {
        auto *label = new QLabel(QString(QChar('A' + x)), tileWidget);
        label->setFixedWidth(18);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 0, x + 1);
    }
    for (int z = 0; z < 11; z++)
    {
        auto *label = new QLabel(QString::number(z), tileWidget);
        label->setFixedWidth(20);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, z + 1, 0);
        for (int x = 0; x < 11; x++)
        {
            auto *tile = new QPushButton(tileWidget);
            tile->setFixedSize(18, 18);
            tile->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(tile, &QPushButton::customContextMenuRequested, this, [this, tile, x, z](const QPoint &position) {
                if (phenomenonChunks.empty())
                {
                    return;
                }

                int chunkX = phenomenonPlayerX + x - 5;
                int chunkZ = phenomenonPlayerZ + z - 5;
                if (chunkX < 0 || chunkX >= 32 || chunkZ < 0 || chunkZ >= 32)
                {
                    return;
                }
                if (!isPhenomenonPlayerPositionValid(phenomenonChunk, chunkX, chunkZ))
                {
                    return;
                }

                QMenu menu(tile);
                QString coordinate = QString("%1%2").arg(QChar('A' + x)).arg(z);
                QAction *action = menu.addAction(tr("Mark %1 as new Position").arg(coordinate));
                if (menu.exec(tile->mapToGlobal(position)) == action)
                {
                    phenomenonPlayerX = chunkX;
                    phenomenonPlayerZ = chunkZ;
                    clearPhenomenonMarking();
                    if (phenomenonGeneratorHasRun)
                    {
                        generate();
                    }
                }
            });
            layout->addWidget(tile, z + 1, x + 1);
            phenomenonTiles[z * 11 + x] = tile;
        }
    }
    tileWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    outerLayout->addWidget(tileWidget, 0, Qt::AlignHCenter | Qt::AlignTop);

    choosePhenomenonPosition = new QPushButton(tr("Choose Position"), phenomenonPositionGroup);
    auto *removeMarking = new QPushButton(tr("Remove marking"), phenomenonPositionGroup);
    auto *reset = new QPushButton(tr("Reset"), phenomenonPositionGroup);
    choosePhenomenonPosition->setEnabled(false);
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(choosePhenomenonPosition);
    buttonLayout->addWidget(removeMarking);
    outerLayout->addLayout(buttonLayout);
    outerLayout->addWidget(reset);
    outerLayout->addStretch(1);
    connect(choosePhenomenonPosition, &QPushButton::clicked, this, &Phenomenon::choosePhenomenonMapPosition);
    connect(removeMarking, &QPushButton::clicked, this, &Phenomenon::clearPhenomenonMarking);
    connect(reset, &QPushButton::clicked, this, &Phenomenon::resetPhenomenonPosition);

    tabLayout->removeWidget(ui->groupBoxGeneratorFilters);
    tabLayout->removeWidget(ui->tableViewGenerator);
    tabLayout->addWidget(phenomenonPositionGroup, 0, 2);
    tabLayout->addWidget(ui->groupBoxGeneratorFilters, 0, 3);
    tabLayout->addWidget(ui->tableViewGenerator, 1, 0, 1, 4);
    ui->groupBoxGeneratorRNGInfo->setMaximumWidth(260);
    tabLayout->setColumnStretch(0, 0);
    tabLayout->setColumnStretch(1, 1);
    tabLayout->setColumnStretch(2, 0);
    tabLayout->setColumnStretch(3, 1);
    phenomenonPositionGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    updatePhenomenonField();
}

void Phenomenon::updatePhenomenonMap()
{
    Encounter encounter = ui->comboBoxGeneratorEncounter->getEnum<Encounter>();
    u16 location = ui->comboBoxGeneratorLocation->getCurrentUShort();
    phenomenonChunks = currentProfile != nullptr ? getPhenomenonChunks(location, currentProfile->getVersion(), encounter)
                                                 : std::vector<PhenomenonChunk> { };
    phenomenonChunk = 0;
    generatedPhenomenonChunk = -1;
    markedPhenomenonChunk = -1;
    markedPhenomenonX = -1;
    markedPhenomenonZ = -1;
    markedPhenomenonBlocked = false;
    if (!phenomenonChunks.empty())
    {
        auto iterator
            = std::find_if(phenomenonChunks.begin(), phenomenonChunks.end(), [](const auto &chunk) { return chunk.defaultChunk; });
        phenomenonChunk = iterator == phenomenonChunks.end() ? 0 : static_cast<int>(std::distance(phenomenonChunks.begin(), iterator));
        phenomenonPlayerX = phenomenonChunks[phenomenonChunk].startX;
        phenomenonPlayerZ = phenomenonChunks[phenomenonChunk].startZ;
    }
    choosePhenomenonPosition->setEnabled(!phenomenonChunks.empty());
    updatePhenomenonField();
}

bool Phenomenon::isPhenomenonPlayerPositionValid(int chunk, int x, int z) const
{
    if (chunk < 0 || chunk >= static_cast<int>(phenomenonChunks.size()) || x < 0 || x >= 32 || z < 0 || z >= 32)
    {
        return false;
    }

    const PhenomenonChunk &map = phenomenonChunks[chunk];
    int index = z * 32 + x;
    if (map.npcTiles[index] && !map.movingNpcTiles[index])
    {
        return false;
    }

    return map.tiles[index] || map.playerTiles[index] || map.movingNpcTiles[index];
}

void Phenomenon::clearPhenomenonMarking()
{
    markedPhenomenonChunk = -1;
    markedPhenomenonX = -1;
    markedPhenomenonZ = -1;
    markedPhenomenonBlocked = false;
    updatePhenomenonField();
}

void Phenomenon::updatePhenomenonField()
{
    if (phenomenonChunks.empty())
    {
        Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
        bool lightMode = colorScheme == Qt::ColorScheme::Light
            || (colorScheme == Qt::ColorScheme::Unknown && palette().color(QPalette::Window).lightness() >= 128);
        QString emptyColor = lightMode ? QStringLiteral("#a6a6a6") : QStringLiteral("#303030");
        for (QPushButton *tile : phenomenonTiles)
        {
            tile->setEnabled(false);
            tile->setText(QString());
            tile->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; border: 1px solid black; }").arg(emptyColor));
        }
        return;
    }

    Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
    bool lightMode = colorScheme == Qt::ColorScheme::Light
        || (colorScheme == Qt::ColorScheme::Unknown && palette().color(QPalette::Window).lightness() >= 128);
    QString walkableColor = lightMode ? QStringLiteral("#f5f5f5") : QStringLiteral("#5a5a5a");
    QString invalidColor = lightMode ? QStringLiteral("#a6a6a6") : QStringLiteral("#303030");
    PhenomenonTileType selectedType = getPhenomenonEncounterType(ui->comboBoxGeneratorEncounter->getEnum<Encounter>());

    for (QPushButton *tile : phenomenonTiles)
    {
        tile->setEnabled(true);
    }

    for (int z = 0; z < 11; z++)
    {
        for (int x = 0; x < 11; x++)
        {
            int chunkX = phenomenonPlayerX + x - 5;
            int chunkZ = phenomenonPlayerZ + z - 5;
            bool inChunk = chunkX >= 0 && chunkX < 32 && chunkZ >= 0 && chunkZ < 32;
            bool valid = inChunk && phenomenonChunks[phenomenonChunk].tiles[chunkZ * 32 + chunkX];
            int index = chunkZ * 32 + chunkX;
            QString color = valid ? getPhenomenonTileColor(phenomenonChunks[phenomenonChunk].tileTypes[index], false)
                                  : (inChunk && phenomenonChunks[phenomenonChunk].playerTiles[index] ? walkableColor : invalidColor);
            if (inChunk && phenomenonChunks[phenomenonChunk].npcTiles[chunkZ * 32 + chunkX])
            {
                color = phenomenonChunks[phenomenonChunk].movingNpcTiles[index] ? QStringLiteral("#d9c63f") : QStringLiteral("#d98b3f");
            }
            if (chunkX == phenomenonPlayerX && chunkZ == phenomenonPlayerZ)
            {
                color = QStringLiteral("#c04040");
            }
            if (phenomenonChunk == markedPhenomenonChunk && chunkX == markedPhenomenonX && chunkZ == markedPhenomenonZ)
            {
                if (!markedPhenomenonBlocked)
                {
                    color = getPhenomenonTileColor(phenomenonChunks[phenomenonChunk].tileTypes[index], true);
                }
            }
            bool player = inChunk && chunkX == phenomenonPlayerX && chunkZ == phenomenonPlayerZ;
            bool actor = inChunk && (phenomenonChunks[phenomenonChunk].npcTiles[index] || player);
            bool marked = phenomenonChunk == markedPhenomenonChunk && chunkX == markedPhenomenonX && chunkZ == markedPhenomenonZ;
            bool matchingBlockedTile
                = valid && actor && phenomenonChunks[phenomenonChunk].tileTypes[index] == selectedType;
            bool playerBlockedTile = matchingBlockedTile && player;
            bool showPhenomenon = !matchingBlockedTile && marked;
            phenomenonTiles[z * 11 + x]->setText(matchingBlockedTile ? QStringLiteral("B")
                                                                      : (showPhenomenon ? QStringLiteral("P") : QString()));
            phenomenonTiles[z * 11 + x]->setStyleSheet(
                QStringLiteral("QPushButton { background-color: %1; border: 1px solid black;%2 }")
                    .arg(color, lightMode && (showPhenomenon || playerBlockedTile) ? QStringLiteral(" color: white;") : QString()));
        }
    }
}

void Phenomenon::markPhenomenon()
{
    QModelIndex index = ui->tableViewGenerator->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    const WildState5 &state = generatorModel->getItem(index.row());
    if (!state.hasPhenomenonPosition())
    {
        return;
    }

    if (generatedPhenomenonChunk < 0)
    {
        return;
    }

    markedPhenomenonChunk = generatedPhenomenonChunk;
    markedPhenomenonX = state.getPhenomenonX();
    markedPhenomenonZ = state.getPhenomenonZ();
    markedPhenomenonBlocked = state.isPhenomenonPositionBlocked();
    updatePhenomenonField();
}

void Phenomenon::resetPhenomenonPosition()
{
    if (phenomenonChunks.empty())
    {
        return;
    }

    phenomenonPlayerX = phenomenonChunks[phenomenonChunk].startX;
    phenomenonPlayerZ = phenomenonChunks[phenomenonChunk].startZ;
    clearPhenomenonMarking();
}

void Phenomenon::choosePhenomenonMapPosition()
{
    if (phenomenonChunks.empty())
    {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowTitle(tr("Choose Position"));
    auto *outer = new QVBoxLayout(&dialog);
    auto *content = new QHBoxLayout();
    outer->addLayout(content);

    auto *mapWidget = new ResponsiveGrid(32, 32, 20, 10, &dialog);
    std::array<QPushButton *, 1024> buttons { };
    for (int z = 0; z < 32; z++)
    {
        for (int x = 0; x < 32; x++)
        {
            auto *button = new QPushButton(mapWidget);
            button->setContextMenuPolicy(Qt::CustomContextMenu);
            buttons[z * 32 + x] = button;
            mapWidget->addWidget(button, z, x);
        }
    }
    content->addWidget(mapWidget);

    int selectedChunk = phenomenonChunk;
    int selectedX = phenomenonPlayerX;
    int selectedZ = phenomenonPlayerZ;
    std::vector<QPushButton *> previewButtons;
    previewButtons.reserve(phenomenonChunks.size());
    Qt::ColorScheme colorScheme = QGuiApplication::styleHints()->colorScheme();
    bool lightMode = colorScheme == Qt::ColorScheme::Light
        || (colorScheme == Qt::ColorScheme::Unknown && palette().color(QPalette::Window).lightness() >= 128);
    QString walkableColor = lightMode ? QStringLiteral("#f5f5f5") : QStringLiteral("#5a5a5a");
    QString invalidColor = lightMode ? QStringLiteral("#a6a6a6") : QStringLiteral("#303030");
    PhenomenonTileType selectedType = getPhenomenonEncounterType(ui->comboBoxGeneratorEncounter->getEnum<Encounter>());
    auto refreshMap = [&] {
        for (int z = 0; z < 32; z++)
        {
            for (int x = 0; x < 32; x++)
            {
                bool valid = phenomenonChunks[selectedChunk].tiles[z * 32 + x];
                int index = z * 32 + x;
                QString color = valid ? getPhenomenonTileColor(phenomenonChunks[selectedChunk].tileTypes[index], false)
                                      : (phenomenonChunks[selectedChunk].playerTiles[index] ? walkableColor : invalidColor);
                if (phenomenonChunks[selectedChunk].npcTiles[z * 32 + x])
                {
                    color = phenomenonChunks[selectedChunk].movingNpcTiles[index] ? QStringLiteral("#d9c63f")
                                                                                  : QStringLiteral("#d98b3f");
                }
                if (x == selectedX && z == selectedZ)
                {
                    color = QStringLiteral("#c04040");
                }
                if (selectedChunk == markedPhenomenonChunk && x == markedPhenomenonX && z == markedPhenomenonZ)
                {
                    if (!markedPhenomenonBlocked)
                    {
                        color = getPhenomenonTileColor(phenomenonChunks[selectedChunk].tileTypes[index], true);
                    }
                }
                bool player = x == selectedX && z == selectedZ;
                bool actor = phenomenonChunks[selectedChunk].npcTiles[index] || player;
                bool marked = selectedChunk == markedPhenomenonChunk && x == markedPhenomenonX && z == markedPhenomenonZ;
                bool matchingBlockedTile = valid && actor && phenomenonChunks[selectedChunk].tileTypes[index] == selectedType;
                bool playerBlockedTile = matchingBlockedTile && player;
                bool showPhenomenon = !matchingBlockedTile && marked;
                buttons[z * 32 + x]->setText(matchingBlockedTile ? QStringLiteral("B")
                                                                  : (showPhenomenon ? QStringLiteral("P") : QString()));
                buttons[z * 32 + x]->setStyleSheet(
                    QStringLiteral("QPushButton { background-color: %1; border: 1px solid black;%2 }")
                        .arg(color, lightMode && (showPhenomenon || playerBlockedTile) ? QStringLiteral(" color: white;") : QString()));
            }
        }
        for (size_t i = 0; i < previewButtons.size(); i++)
        {
            previewButtons[i]->setStyleSheet(
                i == static_cast<size_t>(selectedChunk) ? QStringLiteral("QPushButton { border: 3px solid #3f8fd9; }") : QString());
        }
    };
    for (int z = 0; z < 32; z++)
    {
        for (int x = 0; x < 32; x++)
        {
            QPushButton *button = buttons[z * 32 + x];
            connect(button, &QPushButton::clicked, &dialog, [&, x, z] {
                if (isPhenomenonPlayerPositionValid(selectedChunk, x, z))
                {
                    selectedX = x;
                    selectedZ = z;
                    refreshMap();
                }
            });
            connect(button, &QPushButton::customContextMenuRequested, &dialog, [&, button, x, z](const QPoint &position) {
                if (!isPhenomenonPlayerPositionValid(selectedChunk, x, z))
                {
                    return;
                }
                QMenu menu(button);
                QAction *action = menu.addAction(tr("Mark (%1, %2) as new Position").arg(x).arg(z));
                if (menu.exec(button->mapToGlobal(position)) == action)
                {
                    selectedX = x;
                    selectedZ = z;
                    refreshMap();
                }
            });
        }
    }

    auto *scroll = new QScrollArea(&dialog);
    auto *previews = new QWidget(scroll);
    auto *previewLayout = new QGridLayout(previews);
    previewLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    constexpr int maxPreviewRows = 7;
    int previewColumns = phenomenonChunks.size() > maxPreviewRows ? 2 : 1;
    int previewRows = previewColumns == 2 ? static_cast<int>((phenomenonChunks.size() + 1) / 2)
                                          : static_cast<int>(phenomenonChunks.size());
    for (size_t i = 0; i < phenomenonChunks.size(); i++)
    {
        QPixmap image(64, 64);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        for (int z = 0; z < 32; z++)
        {
            for (int x = 0; x < 32; x++)
            {
                int index = z * 32 + x;
                QColor color = phenomenonChunks[i].tiles[index]
                    ? QColor(getPhenomenonTileColor(phenomenonChunks[i].tileTypes[index], false))
                    : (phenomenonChunks[i].playerTiles[index] ? QColor(walkableColor) : QColor(invalidColor));
                if (phenomenonChunks[i].npcTiles[z * 32 + x])
                {
                    color = phenomenonChunks[i].movingNpcTiles[index] ? QColor("#d9c63f") : QColor("#d98b3f");
                }
                painter.fillRect(x * 2, z * 2, 2, 2, color);
            }
        }
        auto *button = new QPushButton(previews);
        button->setIcon(QIcon(image));
        button->setIconSize(QSize(64, 64));
        button->setFixedSize(80, 80);
        int previewRow = static_cast<int>(i) % previewRows;
        int previewColumn = static_cast<int>(i) / previewRows;
        previewLayout->addWidget(button, previewRow, previewColumn, Qt::AlignHCenter);
        previewButtons.emplace_back(button);
        connect(button, &QPushButton::clicked, &dialog, [&, i] {
            selectedChunk = static_cast<int>(i);
            selectedX = phenomenonChunks[i].startX;
            selectedZ = phenomenonChunks[i].startZ;
            refreshMap();
        });
    }
    scroll->setWidget(previews);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(previewColumns == 2 ? 195 : 105);
    scroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    content->addWidget(scroll);

    refreshMap();
    auto *dialogButtons = new QHBoxLayout;
    auto *resetWindowSize = new QPushButton(tr("Reset Window Size"), &dialog);
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    dialogButtons->addWidget(resetWindowSize);
    dialogButtons->addStretch();
    dialogButtons->addWidget(buttonBox);
    outer->addLayout(dialogButtons);
    connect(resetWindowSize, &QPushButton::clicked, &dialog, [mapWidget] { mapWidget->resetSize(); });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.adjustSize();
    if (dialog.exec() == QDialog::Accepted)
    {
        phenomenonChunk = selectedChunk;
        phenomenonPlayerX = selectedX;
        phenomenonPlayerZ = selectedZ;
        clearPhenomenonMarking();
        if (phenomenonGeneratorHasRun)
        {
            generate();
        }
    }
}

void Phenomenon::generate()
{
    if (!ui->filterGenerator->isValid(ui->spinBoxGeneratorLevelMin->value(), ui->spinBoxGeneratorLevelMax->value()))
    {
        return;
    }

    phenomenonGeneratorHasRun = true;
    generatorModel->clearModel();
    clearPhenomenonMarking();
    generatedPhenomenonChunk = phenomenonChunks.empty() ? -1 : phenomenonChunk;

    u64 seed = ui->textBoxGeneratorSeed->getULong();
    u32 ivAdvances = ui->textBoxGeneratorIVAdvances->getUInt();
    u32 initialAdvances = ui->textBoxGeneratorInitialAdvances->getUInt();
    u32 maxAdvances = ui->textBoxGeneratorMaxAdvances->getUInt();
    u32 offset = ui->textBoxGeneratorOffset->getUInt();
    auto lead = ui->comboMenuGeneratorLead->getEnum<Lead>();
    u8 passPower = (currentProfile->getVersion() & Game::BW2) != Game::None ? getGeneratorPassPower(ui->comboBoxGeneratorLuckyPower)
                                                                            : PassPower5::None;
    u8 luckyPower = PassPower5::getLuckyPower(passPower);
    u8 exploringPower = PassPower5::getExploringPower(passPower);

    auto filter = ui->filterGenerator->getFilter<WildStateFilter, true>();
    WildGenerator5 generator(initialAdvances, maxAdvances, offset, Method::None, lead, luckyPower, exploringPower,
                             encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()], *currentProfile, filter);
    if (!phenomenonChunks.empty())
    {
        generator.setPhenomenonPosition(phenomenonChunks[phenomenonChunk].tiles, phenomenonChunks[phenomenonChunk].npcTiles,
                                        phenomenonChunks[phenomenonChunk].tileTypes, phenomenonPlayerX, phenomenonPlayerZ);
    }

    bool itemFilterVisible = checkListGeneratorItem->isVisible();
    auto itemCheckState = itemFilterVisible ? checkListGeneratorItem->getCheckState() : Qt::Unchecked;
    auto selectedItems = checkListGeneratorItem->getCheckedData();
    bool pokemonSelected = ui->comboBoxGeneratorPokemon->currentIndex() > 0;
    bool itemFilterAny = itemCheckState == Qt::Unchecked || itemCheckState == Qt::Checked;
    bool noneSelected = std::find(selectedItems.begin(), selectedItems.end(), noItem) != selectedItems.end();
    bool itemSelected
        = std::find_if(selectedItems.begin(), selectedItems.end(), [](u16 item) { return item != noItem; }) != selectedItems.end();
    bool generateItems = itemFilterVisible && !(pokemonSelected && itemFilterAny) && (itemFilterAny || itemSelected);
    bool generatePokemon = !itemFilterVisible || pokemonSelected || itemFilterAny || noneSelected;

    auto states = generator.generate(seed, ivAdvances, 0);
    std::erase_if(states, [=](const WildState5 &state) { return state.getPhenomenonItem() || !generatePokemon; });
    if (generateItems)
    {
        WildGenerator5 itemGenerator(initialAdvances, maxAdvances, offset, Method::None, lead, luckyPower, exploringPower,
                                     encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()], *currentProfile,
                                     getUnfilteredWildStateFilter());
        if (!phenomenonChunks.empty())
        {
            itemGenerator.setPhenomenonPosition(phenomenonChunks[phenomenonChunk].tiles, phenomenonChunks[phenomenonChunk].npcTiles,
                                                phenomenonChunks[phenomenonChunk].tileTypes, phenomenonPlayerX, phenomenonPlayerZ);
        }
        auto itemStates = itemGenerator.generate(seed, ivAdvances, 0);
        std::erase_if(itemStates,
                      [=, this](const WildState5 &state) { return !state.getPhenomenonItem() || removeByGeneratorFilters(state); });

        states.insert(states.end(), itemStates.begin(), itemStates.end());
        std::stable_sort(states.begin(), states.end(),
                         [](const WildState5 &left, const WildState5 &right) { return left.getAdvances() < right.getAdvances(); });
    }
    generatorModel->addItems(states);
}

void Phenomenon::goToGenerator()
{
    if (!ui->tableViewSearcher->currentIndex().isValid())
        return;
    QModelIndex index = proxyModel->mapToSource(ui->tableViewSearcher->currentIndex());
    const auto &result = searcherModel->getItem(index.row());
    const auto &state = result.getState();

    ui->comboBoxGeneratorEncounter->setCurrentIndex(ui->comboBoxSearcherEncounter->currentIndex());
    ui->comboBoxGeneratorSeason->setCurrentIndex(ui->comboBoxSearcherSeason->currentIndex());
    ui->comboBoxGeneratorLocation->setCurrentIndex(ui->comboBoxSearcherLocation->currentIndex());
    ui->tabRNGSelector->setCurrentIndex(0);
    ui->textBoxGeneratorSeed->setText(QString::number(result.getInitialSeed(), 16).toUpper());
    ui->textBoxGeneratorIVAdvances->setText(QString::number(state.getIVAdvances()));
    ui->textBoxGeneratorMaxAdvances->setText(
        QString::number(ResultToGenerator5::getMaxAdvances(result.getInitialSeed(), state.getAdvances(), 50, *currentProfile)));
    ResultToGenerator5::setPreferredLead(ui->comboMenuGeneratorLead, state.getLeadMask(), state.getNature());
    setGeneratorPowers(ui->comboBoxGeneratorLuckyPower, { state.getPassPower() });
    ui->tableViewGenerator->setTargetAdvance(state.getAdvances());
    generate();
}

void Phenomenon::generatorEncounterIndexChanged(int index)
{
    if (index >= 0)
    {
        auto encounter = ui->comboBoxGeneratorEncounter->getEnum<Encounter>();
        u16 currentLocation = ui->comboBoxGeneratorLocation->getCurrentUShort();
        bool encounterModifier = supportsEncounterModifier(encounter);
        ui->comboMenuGeneratorLead->hideAction(toInt(Lead::ArenaTrap), !encounterModifier);
        ui->comboMenuGeneratorLead->hideAction(toInt(Lead::SuctionCups), !encounterModifier);

        EncounterSettings5 settings { false, static_cast<u8>(ui->comboBoxGeneratorSeason->currentIndex()) };
        encounterGenerator = Encounters5::getEncounters(encounter, settings, currentProfile);

        std::vector<u16> locs;
        std::ranges::transform(encounterGenerator, std::back_inserter(locs), [](const EncounterArea5 &area) { return area.getLocation(); });

        {
            QSignalBlocker blocker(ui->comboBoxGeneratorLocation);
            ui->comboBoxGeneratorLocation->clear();
            ui->comboBoxGeneratorLocation->addItems(Translator::getLocations(locs, currentProfile->getVersion()), locs);
            ui->comboBoxGeneratorLocation->setCurrentIndexByData(currentLocation);
        }
        generatorLocationIndexChanged(ui->comboBoxGeneratorLocation->currentIndex());
    }
}

void Phenomenon::generatorLocationIndexChanged(int index)
{
    if (index >= 0)
    {
        auto &area = encounterGenerator[ui->comboBoxGeneratorLocation->currentIndex()];
        auto species = area.getUniqueSpecies();
        auto names = area.getSpecieNames();

        ui->filterGenerator->setEncounterSlots(area.getCount());

        {
            QSignalBlocker blocker(ui->comboBoxGeneratorPokemon);
            ui->comboBoxGeneratorPokemon->clear();
            ui->comboBoxGeneratorPokemon->addItem("-");
            for (size_t i = 0; i < species.size(); i++)
            {
                ui->comboBoxGeneratorPokemon->addItem(QString::fromStdString(names[i]), species[i]);
            }
        }
        generatorPokemonIndexChanged(ui->comboBoxGeneratorPokemon->currentIndex());

        updateItemFilter(labelGeneratorItem, checkListGeneratorItem, area);
        updatePhenomenonMap();
    }
}

void Phenomenon::generatorPokemonIndexChanged(int index)
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

void Phenomenon::generatorSeasonIndexChanged(int index)
{
    if (index >= 0)
    {
        generatorEncounterIndexChanged(0);
    }
}

void Phenomenon::openAdjacentSeeds()
{
    QModelIndex index = proxyModel->mapToSource(ui->tableViewSearcher->currentIndex());
    const auto &state = searcherModel->getItem(index.row());

    auto *window = new AdjacentSeeds(false, state.getButtons(), state.getDateTime(), *currentProfile);
    window->show();
}

void Phenomenon::openAdvanceFinder()
{
    auto *advanceFinder = new AdvanceFinder(generatorModel, ui->tableViewGenerator, currentProfile, this);
    advanceFinder->show();
}

void Phenomenon::profileIndexChanged(int index)
{
    if (index >= 0)
    {
        currentProfile = &profiles[index];
        itemTool->setProfile(*currentProfile);

        ui->labelProfileTIDValue->setText(QString::number(currentProfile->getTID()));
        ui->labelProfileSIDValue->setText(QString::number(currentProfile->getSID()));
        ui->labelProfileMACAddressValue->setText(QString::number(currentProfile->getMac(), 16));
        ui->labelProfileDSTypeValue->setText(QString::fromStdString(currentProfile->getDSTypeString()));
        ui->labelProfileVCountValue->setText(QString::number(currentProfile->getVCount(), 16));
        ui->labelProfileTimer0Value->setText(QString::number(currentProfile->getTimer0Min(), 16) + "-"
                                             + QString::number(currentProfile->getTimer0Max(), 16));
        ui->labelProfileGxStatValue->setText(QString::number(currentProfile->getGxStat()));
        ui->labelProfileVFrameValue->setText(QString::number(currentProfile->getVFrame()));
        ui->labelProfileKeypressesValue->setText(QString::fromStdString(currentProfile->getKeypressesString()));
        ui->labelProfileGameValue->setText(QString::fromStdString(Translator::getGame(currentProfile->getVersion())));

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
            setGeneratorPowers(ui->comboBoxGeneratorLuckyPower, { PassPower5::None });
            ui->comboBoxSearcherLuckyPower->setCheckedData({ PassPower5::None });
        }

        generatorEncounterIndexChanged(0);
        searcherEncounterIndexChanged(0);
    }
}

void Phenomenon::profileManager()
{
    auto *manager = new ProfileManager5();
    connect(manager, &ProfileManager5::profilesChanged, this, [=](int num) { emit profilesChanged(num); });
    manager->show();
}

void Phenomenon::search()
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

    u32 initialIVAdvances = ui->textBoxSearcherInitialIVAdvances->getUInt();
    u32 maxIVAdvances = ui->textBoxSearcherMaxIVAdvances->getUInt();
    u32 initialAdvances = ui->textBoxSearcherInitialAdvances->getUInt();
    u32 maxAdvances = ui->textBoxSearcherMaxAdvances->getUInt();
    auto leads = getSearcherLeads(ui->comboMenuSearcherLead);
    auto luckyPowers = (currentProfile->getVersion() & Game::BW2) != Game::None
        ? getLuckyPowers(getCheckedUChars(ui->comboBoxSearcherLuckyPower), (currentProfile->getVersion() & Game::BW) != Game::None)
        : std::vector<u8> { PassPower5::None };
    bool showPassPower = hasPassPower(luckyPowers);
    if (showPassPower && static_cast<u64>(initialIVAdvances) + maxIVAdvances < 2)
    {
        QMessageBox msg(QMessageBox::Warning, tr("Invalid IV Advance range"),
                        tr("An IV Advance of 2 or higher must be searched for to use Pass Powers"));
        msg.exec();
        return;
    }

    searcherModel->clearModel();
    ui->pushButtonSearch->setEnabled(false);
    ui->pushButtonCancel->setEnabled(true);
    searcherModel->setShowPassPower(showPassPower);

    auto filter = ui->filterSearcher->getFilter<WildStateFilter, true>();
    WildGenerator5 generator(initialAdvances, maxAdvances, 0, Method::Method5, leads, luckyPowers, false, false,
                             encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()], *currentProfile, filter, true);

    SearcherBase5<WildGenerator5, WildState5> *searcher;
    if (fastSearchEnabled())
    {
        auto ivMap = ivCache->getCache(initialIVAdvances, maxIVAdvances, currentProfile->getVersion(), CacheType::Normal, filter);
        if (shaCache && shaCache->isValid(*currentProfile))
        {
            auto shaMap = shaCache->getCache(initialIVAdvances, maxIVAdvances, start, end, ivMap, CacheType::Normal, *currentProfile);
            searcher = new IVSearcher5CacheFast<WildGenerator5, WildState5>(initialIVAdvances, maxIVAdvances, shaMap, ivMap, generator,
                                                                            *currentProfile);
        }
        else
        {
            searcher = new IVSearcher5Fast<WildGenerator5, WildState5>(initialIVAdvances, maxIVAdvances, ivMap, generator, *currentProfile);
        }
    }
    else
    {
        searcher = new IVSearcher5<WildGenerator5, WildState5>(initialIVAdvances, maxIVAdvances, generator, *currentProfile);
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
        auto results = searcher->getResults();
        std::erase_if(results, [=, this](const SearcherState5<WildState5> &result) {
            const auto &state = result.getState();
            return removeBySearcherFilters(state);
        });
        searcherModel->addItems(results);
        if (showPassPower)
        {
            ui->tableViewSearcher->resizeColumnToContents(2);
        }
        ui->progressBar->setValue(searcher->getProgress());

        if (!searcher->isSearching())
        {
            timer->stop();

            auto finalResults = searcher->getResults();
            std::erase_if(finalResults, [=, this](const SearcherState5<WildState5> &result) {
                const auto &state = result.getState();
                return removeBySearcherFilters(state);
            });
            searcherModel->addItems(finalResults);
            if (showPassPower)
            {
                ui->tableViewSearcher->resizeColumnToContents(2);
            }
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

void Phenomenon::searcherEncounterIndexChanged(int index)
{
    if (index >= 0)
    {
        auto encounter = ui->comboBoxSearcherEncounter->getEnum<Encounter>();
        u16 currentLocation = ui->comboBoxSearcherLocation->getCurrentUShort();
        bool encounterModifier = supportsEncounterModifier(encounter);
        ui->comboMenuSearcherLead->hideAction(toInt(Lead::ArenaTrap), !encounterModifier);
        ui->comboMenuSearcherLead->hideAction(toInt(Lead::SuctionCups), !encounterModifier);

        EncounterSettings5 settings { false, static_cast<u8>(ui->comboBoxSearcherSeason->currentIndex()) };
        encounterSearcher = Encounters5::getEncounters(encounter, settings, currentProfile);

        std::vector<u16> locs;
        std::ranges::transform(encounterSearcher, std::back_inserter(locs), [](const EncounterArea5 &area) { return area.getLocation(); });

        {
            QSignalBlocker blocker(ui->comboBoxSearcherLocation);
            ui->comboBoxSearcherLocation->clear();
            ui->comboBoxSearcherLocation->addItems(Translator::getLocations(locs, currentProfile->getVersion()), locs);
            ui->comboBoxSearcherLocation->setCurrentIndexByData(currentLocation);
        }
        searcherLocationIndexChanged(ui->comboBoxSearcherLocation->currentIndex());
    }
}

void Phenomenon::searcherFastSearchChanged()
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

void Phenomenon::searcherLocationIndexChanged(int index)
{
    if (index >= 0)
    {
        auto &area = encounterSearcher[ui->comboBoxSearcherLocation->currentIndex()];
        auto species = area.getUniqueSpecies();
        auto names = area.getSpecieNames();

        ui->filterSearcher->setEncounterSlots(area.getCount());

        {
            QSignalBlocker blocker(ui->comboBoxSearcherPokemon);
            ui->comboBoxSearcherPokemon->clear();
            ui->comboBoxSearcherPokemon->addItem("-");
            for (size_t i = 0; i < species.size(); i++)
            {
                ui->comboBoxSearcherPokemon->addItem(QString::fromStdString(names[i]), species[i]);
            }
        }
        searcherPokemonIndexChanged(ui->comboBoxSearcherPokemon->currentIndex());
    }
}

void Phenomenon::searcherPokemonIndexChanged(int index)
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

void Phenomenon::searcherSeasonIndexChanged(int index)
{
    if (index >= 0)
    {
        searcherEncounterIndexChanged(0);
    }
}

void Phenomenon::transferFilters(int index)
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

void Phenomenon::transferSettings(int index)
{
    if (index == 0)
    {
        ui->comboBoxSearcherEncounter->setCurrentIndex(ui->comboBoxGeneratorEncounter->currentIndex());
        ui->comboBoxSearcherLocation->setCurrentIndex(ui->comboBoxGeneratorLocation->currentIndex());
        ui->comboBoxSearcherPokemon->setCurrentIndex(ui->comboBoxGeneratorPokemon->currentIndex());
        ui->comboBoxSearcherSeason->setCurrentIndex(ui->comboBoxGeneratorSeason->currentIndex());
        ui->comboBoxSearcherLuckyPower->setCheckedData(
            { PassPower5::getLuckyPower(getGeneratorPassPower(ui->comboBoxGeneratorLuckyPower)) });
    }
    else
    {
        ui->comboBoxGeneratorEncounter->setCurrentIndex(ui->comboBoxSearcherEncounter->currentIndex());
        ui->comboBoxGeneratorLocation->setCurrentIndex(ui->comboBoxSearcherLocation->currentIndex());
        ui->comboBoxGeneratorPokemon->setCurrentIndex(ui->comboBoxSearcherPokemon->currentIndex());
        ui->comboBoxGeneratorSeason->setCurrentIndex(ui->comboBoxSearcherSeason->currentIndex());
        auto luckyPowers = (currentProfile->getVersion() & Game::BW2) != Game::None
            ? getLuckyPowers(getCheckedUChars(ui->comboBoxSearcherLuckyPower), (currentProfile->getVersion() & Game::BW) != Game::None)
            : std::vector<u8> { PassPower5::None };
        setGeneratorPowers(ui->comboBoxGeneratorLuckyPower, { luckyPowers.empty() ? PassPower5::None : luckyPowers.front() });
    }
}
