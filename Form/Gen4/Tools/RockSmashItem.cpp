#include "RockSmashItem.hpp"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen4/EncounterArea4.hpp>
#include <Core/Gen4/Encounters4.hpp>
#include <Core/Gen4/Generators/WildGenerator4.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Gen4/States/WildState4.hpp>
#include <Core/Parents/Filters/StateFilter.hpp>
#include <Core/Parents/Searchers/SearcherBase.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/ComboBox.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/TableView.hpp>
#include <Form/Controls/TextBox.hpp>
#include <Form/Gen4/Profile/ProfileDisplay4.hpp>
#include <Form/Gen4/Tools/SeedToTime4.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <Model/TableModel.hpp>
#include <Model/Util/LeadDisplay.hpp>
#include <QCheckBox>
#include <QAction>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <iterator>

namespace
{
struct Result
{
    u32 seed;
    std::vector<u32> advances;
    u16 item;
    std::vector<Lead> leads;
};

QString getLeadName(Lead lead)
{
    if (lead == Lead::RockSmashMagnetPull || lead == Lead::RockSmashSuctionCups)
    {
        return QCoreApplication::translate("RockSmashItem", "Magnet Pull / Suction Cups");
    }
    if (lead == Lead::SereneGrace || lead == Lead::RockSmashSuperLuck)
    {
        return QCoreApplication::translate("RockSmashItem", "Serene Grace / Super Luck");
    }
    return LeadDisplay::getLeadName(getLeadFlag(lead), false);
}

class ResultModel final : public TableModel<Result>
{
public:
    explicit ResultModel(QObject *parent) : TableModel(parent) {}
    int columnCount(const QModelIndex & = {}) const override { return 5; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role != Qt::DisplayRole) return {};
        const auto &state = model[index.row()];
        switch (index.column())
        {
        case 0: return QString::number(state.seed, 16).toUpper().rightJustified(8, '0');
        case 1: return state.seed & 0xffff;
        case 2:
        {
            QStringList names;
            for (Lead lead : state.leads) names.emplace_back(getLeadName(lead));
            return names.join(" / ");
        }
        case 3:
        {
            QStringList values;
            for (u32 advance : state.advances) values.emplace_back(QString::number(advance));
            return values.join(", ");
        }
        case 4: return QString::fromStdString(Translator::getItem(state.item));
        default: return {};
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
        static const QStringList headers = { tr("Seed"), tr("Delay"), tr("Lead"), tr("Target Advances"), tr("Item") };
        return headers[section];
    }
};

WildStateFilter unfiltered()
{
    std::array<u8, 6> min = {};
    std::array<u8, 6> max;
    max.fill(31);
    std::array<bool, 25> natures;
    natures.fill(true);
    std::array<bool, 16> powers;
    powers.fill(true);
    StackVector<bool, 13> encounterSlots;
    encounterSlots.fill(true);
    return WildStateFilter(255, 255, 255, 1, 100, 0, 255, 0, 255, true, min, max, natures, powers, encounterSlots);
}

class ItemSearcher final : public SearcherBase<Result>
{
public:
    ItemSearcher(u32 minDelay, u32 maxDelay, u16 target, u8 amount,
                 const std::vector<std::pair<Lead, WildGenerator4>> &generators) :
        minDelay(minDelay), maxDelay(maxDelay), target(target), amount(amount), generators(generators) {}

    void start(int threads)
    {
        setMaxProgress(static_cast<u64>(maxDelay - minDelay + 1) * 256 * 24);
        activeThreads.store(threads);
        for (int i = 0; i < threads; i++)
        {
            threadContainer.emplace_back([this] { run(); activeThreads.fetch_sub(1); });
        }
    }

private:
    void run()
    {
        const u64 total = static_cast<u64>(maxDelay - minDelay + 1) * 256 * 24;
        while (true)
        {
            u64 value = index.fetch_add(1);
            if (value >= total || cancelled.load(std::memory_order_relaxed)) return;
            u32 delay = minDelay + static_cast<u32>(value / (256 * 24));
            u32 rem = static_cast<u32>(value % (256 * 24));
            u32 seed = ((rem / 24) << 24) | ((rem % 24) << 16) | delay;
            std::vector<Result> seedResults;
            for (const auto &[activeLead, generator] : generators)
            {
                std::vector<u32> matches;
                for (const auto &state : generator.generate(seed, 0))
                {
                    if (state.getSpecie() == 0 && state.getItem() == target
                        && (matches.empty() || state.getAdvances() - matches.back() >= 3))
                    {
                        matches.emplace_back(state.getAdvances());
                    }
                }
                if (matches.size() < amount) continue;
                auto existing = std::ranges::find_if(seedResults, [&](const Result &result) { return result.advances == matches; });
                if (existing == seedResults.end())
                {
                    seedResults.push_back({ seed, std::move(matches), target, { activeLead } });
                }
                else if (activeLead == Lead::None)
                {
                    existing->leads = { Lead::None };
                }
                else if (!std::ranges::contains(existing->leads, Lead::None))
                {
                    existing->leads.emplace_back(activeLead);
                }
            }
            if (!seedResults.empty())
            {
                std::lock_guard<std::mutex> guard(mutex);
                results.insert(results.end(), std::make_move_iterator(seedResults.begin()), std::make_move_iterator(seedResults.end()));
            }
            progress.fetch_add(1, std::memory_order_relaxed);
        }
    }
    u32 minDelay;
    u32 maxDelay;
    u16 target;
    u8 amount;
    std::vector<std::pair<Lead, WildGenerator4>> generators;
};

const QString settingPrefix = QStringLiteral("rockSmashItem");
}

RockSmashItem::RockSmashItem(QWidget *parent) : QWidget(parent), currentProfile(nullptr)
{
    setWindowTitle(tr("Rock Smash Items"));
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *root = new QVBoxLayout(this);
    profileDisplay = new ProfileDisplay4(this);
    profileDisplay->setup(settingPrefix, Game::HGSS, Game::HeartGold, true);
    root->addWidget(profileDisplay);

    auto *top = new QHBoxLayout;
    auto *rngGroup = new QGroupBox(tr("RNG Info"), this);
    auto *rng = new QFormLayout(rngGroup);
    minDelay = new TextBox(rngGroup); minDelay->setText(QStringLiteral("1000")); minDelay->setValues(InputType::Delay);
    maxDelay = new TextBox(rngGroup); maxDelay->setText(QStringLiteral("1020")); maxDelay->setValues(InputType::Delay);
    minAdvance = new TextBox(rngGroup); minAdvance->setText(QStringLiteral("5")); minAdvance->setValues(InputType::Advance32Bit);
    maxAdvance = new TextBox(rngGroup); maxAdvance->setText(QStringLiteral("35")); maxAdvance->setValues(InputType::Advance32Bit);
    rng->addRow(tr("Min Delay"), minDelay); rng->addRow(tr("Max Delay"), maxDelay);
    rng->addRow(tr("Min Advance"), minAdvance); rng->addRow(tr("Max Advance"), maxAdvance);
    auto *buttons = new QHBoxLayout;
    searchButton = new QPushButton(tr("Search"), rngGroup);
    cancelButton = new QPushButton(tr("Cancel"), rngGroup);
    cancelButton->setEnabled(false);
    buttons->addWidget(searchButton);
    buttons->addWidget(cancelButton);
    rng->addRow(buttons);
    top->addWidget(rngGroup, 1);

    auto *settingsGroup = new QGroupBox(tr("Settings"), this);
    auto *settings = new QGridLayout(settingsGroup);
    location = new ComboBox(settingsGroup); location->enableAutoComplete();
    lead = new ComboMenu(rngGroup);
    lead->addAction(tr("None"), toInt(Lead::None));
    lead->addAction(tr("Keen Eye"), toInt(Lead::KeenEye));
    lead->addAction(tr("Intimidate"), toInt(Lead::Intimidate));
    lead->addAction(tr("Magnet Pull / Suction Cups"), toInt(Lead::RockSmashMagnetPull));
    lead->addAction(tr("Serene Grace / Super Luck"), toInt(Lead::SereneGrace));
    lead->setMultiSelect(true);
    lead->setCheckedData({ toInt(Lead::None) });
    lead->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    rng->insertRow(0, tr("Lead"), lead);
    rockSmashPokemon = new QCheckBox(tr("Walking Pokémon uses Rock Smash"), settingsGroup);
    settings->addWidget(new QLabel(tr("Location"), settingsGroup), 0, 0);
    settings->addWidget(location, 0, 1);
    settings->addWidget(rockSmashPokemon, 1, 0, 1, 2);
    settings->setColumnStretch(1, 1);
    settings->setRowStretch(0, 1);
    settings->setRowStretch(1, 1);
    top->addWidget(settingsGroup, 2);

    auto *filterGroup = new QGroupBox(tr("Filters"), this);
    auto *filters = new QGridLayout(filterGroup);
    item = new ComboBox(filterGroup);
    amount = new QSpinBox(filterGroup); amount->setRange(1, 99); amount->setValue(1);
    filters->addWidget(new QLabel(tr("Item"), filterGroup), 0, 0);
    filters->addWidget(item, 0, 1);
    filters->addWidget(new QLabel(tr("Amount"), filterGroup), 1, 0);
    filters->addWidget(amount, 1, 1);
    filters->setColumnStretch(1, 1);
    filters->setRowStretch(0, 1);
    filters->setRowStretch(1, 1);
    top->addWidget(filterGroup, 1);
    root->addLayout(top);

    progressBar = new QProgressBar(this); root->addWidget(progressBar);
    table = new TableView(this);
    auto *model = new ResultModel(table); auto *proxy = new SortFilterProxyModel(table, model); table->setModel(proxy);
    table->setSortingEnabled(true);
    auto *generateTimes = table->addAction(tr("Generate times for seed"));
    table->setPrimaryAction(generateTimes);
    auto *openGenerator = table->addAction(tr("Open in Generator"));
    table->setSecondaryAction(openGenerator);
    table->horizontalHeader()->setStretchLastSection(true); root->addWidget(table, 1);

    connect(profileDisplay, &ProfileDisplay4::profileChanged, this, &RockSmashItem::profileChanged);
    connect(profileDisplay, &ProfileDisplay4::profilesChanged, this, &RockSmashItem::profilesChanged);
    connect(location, &QComboBox::currentIndexChanged, this, &RockSmashItem::locationChanged);
    connect(searchButton, &QPushButton::clicked, this, &RockSmashItem::search);
    connect(generateTimes, &QAction::triggered, this, &RockSmashItem::seedToTime);
    connect(openGenerator, &QAction::triggered, this, &RockSmashItem::openInGenerator);
    updateProfiles();

    QSettings settingsStore;
    settingsStore.beginGroup(settingPrefix);
    minDelay->setText(settingsStore.value(QStringLiteral("minDelay"), minDelay->text()).toString());
    maxDelay->setText(settingsStore.value(QStringLiteral("maxDelay"), maxDelay->text()).toString());
    minAdvance->setText(settingsStore.value(QStringLiteral("minAdvance"), minAdvance->text()).toString());
    maxAdvance->setText(settingsStore.value(QStringLiteral("maxAdvance"), maxAdvance->text()).toString());
    if (settingsStore.contains(QStringLiteral("geometry")))
    {
        restoreGeometry(settingsStore.value(QStringLiteral("geometry")).toByteArray());
    }
    else
    {
        resize(900, 550);
    }
    settingsStore.endGroup();
}

RockSmashItem::~RockSmashItem()
{
    QSettings settingsStore;
    settingsStore.beginGroup(settingPrefix);
    settingsStore.setValue(QStringLiteral("minDelay"), minDelay->text());
    settingsStore.setValue(QStringLiteral("maxDelay"), maxDelay->text());
    settingsStore.setValue(QStringLiteral("minAdvance"), minAdvance->text());
    settingsStore.setValue(QStringLiteral("maxAdvance"), maxAdvance->text());
    settingsStore.setValue(QStringLiteral("geometry"), saveGeometry());
    settingsStore.endGroup();
}
bool RockSmashItem::hasProfiles() const { return currentProfile != nullptr; }
void RockSmashItem::updateProfiles() { profileDisplay->updateProfiles(); }

void RockSmashItem::profileChanged(const Profile4 &profile)
{
    currentProfile = &profile;
    EncounterSettings4 settings = {};
    settings.hgss = { 0, {} };
    auto areas = Encounters4::getEncounters(Encounter::RockSmash, settings, currentProfile);
    std::vector<u16> locations;
    for (const auto &area : areas)
    {
        if (!WildGenerator4::getRockSmashItems(area.getLocation(), profile.getVersion()).empty()) locations.push_back(area.getLocation());
    }
    auto names = Translator::getLocations(locations, profile.getVersion());
    std::vector<std::pair<QString, u16>> entries;
    for (size_t i = 0; i < locations.size(); i++) entries.emplace_back(QString::fromStdString(names[i]), locations[i]);
    std::ranges::sort(entries, [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.first, right.first) < 0;
    });
    location->clear();
    for (const auto &[name, value] : entries)
    {
        location->addItem(name, value);
    }
    if (location->count() > 0)
    {
        location->setCurrentIndex(0);
        locationChanged(0);
    }
}

void RockSmashItem::locationChanged(int)
{
    if (!currentProfile || location->currentIndex() < 0) return;
    auto items = WildGenerator4::getRockSmashItems(location->getCurrentUChar(), currentProfile->getVersion());
    std::ranges::sort(items, [](u16 a, u16 b) { return Translator::getItem(a) < Translator::getItem(b); });
    item->clear();
    for (u16 value : items) item->addItem(QString::fromStdString(Translator::getItem(value)), value);
}

void RockSmashItem::search()
{
    if (!currentProfile || location->currentIndex() < 0 || item->currentIndex() < 0) return;
    if (minDelay->getUInt() > maxDelay->getUInt() || minAdvance->getUInt() > maxAdvance->getUInt())
    {
        QMessageBox::warning(this, tr("Invalid range"), tr("The minimum value must not exceed the maximum value."));
        return;
    }
    EncounterSettings4 settings = {}; settings.hgss = { 0, {} };
    auto areas = Encounters4::getEncounters(Encounter::RockSmash, settings, currentProfile);
    auto found = std::ranges::find_if(areas, [this](const auto &area) { return area.getLocation() == location->getCurrentUChar(); });
    if (found == areas.end()) return;
    auto *proxy = static_cast<SortFilterProxyModel *>(table->model());
    auto *model = static_cast<ResultModel *>(proxy->sourceModel()); model->clearModel();
    u32 initial = minAdvance->getUInt();
    std::vector<Lead> selectedLeads;
    for (int value : lead->getCheckedData())
    {
        Lead selected = static_cast<Lead>(value);
        if (!std::ranges::contains(selectedLeads, selected)) selectedLeads.emplace_back(selected);
    }
    if (selectedLeads.empty()) selectedLeads.emplace_back(Lead::None);
    std::vector<std::pair<Lead, WildGenerator4>> generators;
    for (Lead selected : selectedLeads)
    {
        generators.emplace_back(selected,
                                WildGenerator4(initial, maxAdvance->getUInt() - initial, 0, Method::MethodK, selected, false, false, false, 0,
                                               false, false, false, 0, 0, 0, rockSmashPokemon->isChecked(), 0, *found, *currentProfile,
                                               unfiltered()));
    }
    auto *searcher = new ItemSearcher(minDelay->getUInt(), maxDelay->getUInt(), item->getCurrentUShort(), amount->value(), generators);
    QSettings settingsStore; int threads = std::max(1, settingsStore.value("settings/threads", 1).toInt());
    searchButton->setEnabled(false); cancelButton->setEnabled(true);
    auto *timer = new QTimer(this);
    connect(cancelButton, &QPushButton::clicked, timer, [=] { searcher->cancelSearch(); cancelButton->setEnabled(false); });
    connect(timer, &QTimer::timeout, this, [=] {
        model->addItems(searcher->getResults()); progressBar->setValue(searcher->getProgress());
        if (!searcher->isSearching())
        {
            timer->stop();
            model->addItems(searcher->getResults());
            searchButton->setEnabled(true);
            cancelButton->setEnabled(false);
            delete searcher;
            timer->deleteLater();
        }
    });
    searcher->start(threads); timer->start(500);
}

void RockSmashItem::openInGenerator()
{
    if (currentProfile == nullptr || location->currentIndex() < 0 || !table->currentIndex().isValid())
    {
        return;
    }

    auto *proxy = static_cast<SortFilterProxyModel *>(table->model());
    QModelIndex index = proxy->mapToSource(table->currentIndex());
    auto *model = static_cast<ResultModel *>(proxy->sourceModel());
    const auto &result = model->getItem(index.row());
    if (result.leads.empty())
    {
        return;
    }

    emit openGenerator(*currentProfile, location->getCurrentUChar(), result.seed, toInt(result.leads.front()),
                       rockSmashPokemon->isChecked(), result.advances);
}

void RockSmashItem::seedToTime()
{
    if (currentProfile == nullptr || !table->currentIndex().isValid())
    {
        return;
    }

    auto *proxy = static_cast<SortFilterProxyModel *>(table->model());
    QModelIndex index = proxy->mapToSource(table->currentIndex());
    auto *model = static_cast<ResultModel *>(proxy->sourceModel());
    auto *window = new SeedToTime4(model->getItem(index.row()).seed, currentProfile->getVersion());
    window->show();
}
