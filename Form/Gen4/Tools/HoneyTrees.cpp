#include "HoneyTrees.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Gen4/Tools/HoneyTreeGenerator.hpp>
#include <Core/Parents/Searchers/SearcherBase.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Util/Utilities.hpp>
#include <Form/Controls/CheckList.hpp>
#include <Form/Controls/ComboBox.hpp>
#include <Form/Controls/TableView.hpp>
#include <Form/Controls/TextBox.hpp>
#include <Form/Gen4/Profile/ProfileDisplay4.hpp>
#include <Form/Util/AdvanceFinder.hpp>
#include <Form/Gen4/Tools/SeedToTime4.hpp>
#include <Model/Gen4/IRNGProvider4.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <Model/TableModel.hpp>
#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <iterator>
#include <set>
#include <thread>

namespace
{
    const QString settingPrefix = QStringLiteral("honeyTrees");
    constexpr u16 noSpecies = 0xffff;

    struct LocationEntry
    {
        const char *name;
        u8 index;
    };

    constexpr std::array<LocationEntry, 21> locations = { { { "Route 205 (South)", 0 },
                                                            { "Route 205 (North)", 1 },
                                                            { "Route 206", 2 },
                                                            { "Route 207", 3 },
                                                            { "Route 208", 4 },
                                                            { "Route 209", 5 },
                                                            { "Route 210 (South)", 6 },
                                                            { "Route 210 (North)", 7 },
                                                            { "Route 211 (East)", 8 },
                                                            { "Route 212 (North)", 9 },
                                                            { "Route 212 (South)", 10 },
                                                            { "Route 213", 11 },
                                                            { "Route 214", 12 },
                                                            { "Route 215", 13 },
                                                            { "Route 218", 14 },
                                                            { "Route 221", 15 },
                                                            { "Route 222", 16 },
                                                            { "Valley Windworks", 17 },
                                                            { "Eterna Forest exterior", 18 },
                                                            { "Fuego Ironworks", 19 },
                                                            { "Floaroma Meadow", 20 } } };

    QString groupName(u8 group)
    {
        if (group == 0)
            return HoneyTrees::tr("None");
        return HoneyTrees::tr("Group %1").arg(QChar('A' + group - 1));
    }

    QString npcNoiseName(u8 location)
    {
        switch (location)
        {
        case 0:
        case 2:
        case 7:
        case 9:
        case 10:
        case 11:
        case 18:
            return HoneyTrees::tr("Noisy");
        case 1:
        case 17:
        case 19:
        case 20:
            return HoneyTrees::tr("Quiet");
        default:
            return HoneyTrees::tr("Quiet via Vs. Seeker");
        }
    }
}

class HoneyTreeModel final : public TableModel<HoneyTreeState>, public IRNGProvider4
{
public:
    explicit HoneyTreeModel(QObject *parent) : TableModel(parent)
    {
    }
    int columnCount(const QModelIndex & = { }) const override
    {
        return 6;
    }
    u8 getCall(int) const override
    {
        return 0;
    }
    u8 getChatot(int row) const override
    {
        return model[row].chatot;
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        const auto &state = model[index.row()];
        if (state.species == 0)
        {
            if (role == Qt::FontRole)
            {
                QFont font;
                font.setItalic(true);
                return font;
            }
            if (role == Qt::ForegroundRole)
                return QColor(128, 128, 128);
        }
        if (role != Qt::DisplayRole)
            return { };
        if (state.species == 0 && index.column() > 1)
            return QStringLiteral("-");
        switch (index.column())
        {
        case 0:
            return state.advances;
        case 1:
            return QString::fromStdString(Utilities4::getChatot(state.chatot));
        case 2:
            return state.group == 0 && state.species != 0 ? tr("None (Group A table)") : groupName(state.group);
        case 3:
            return state.slot == 0xff ? QStringLiteral("-") : QString::number(state.slot);
        case 4:
            return state.species == 0 ? QStringLiteral("-")
                                      : QString::fromStdString(Translator::getSpecies(std::vector<u16> { state.species })[0]);
        case 5:
            return static_cast<int>(state.shakes);
        default:
            return { };
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return { };
        static const QStringList headers = { tr("Advances"), tr("Chatot"), tr("Group"), tr("Slot"), tr("Pok\303\251mon"), tr("Shakes") };
        return headers[section];
    }
};

struct HoneyTreeSearchResult
{
    u32 seed;
    u32 delay;
    u32 advance;
    u8 group;
    u8 slot;
    u16 species;
    u8 shakes;
    u32 highestAdvance;
    std::vector<u32> delays;
};

class HoneyTreeSearchModel final : public TableModel<HoneyTreeSearchResult>
{
public:
    explicit HoneyTreeSearchModel(QObject *parent) : TableModel(parent)
    {
    }
    int columnCount(const QModelIndex & = { }) const override
    {
        return 8;
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        const auto &result = model[index.row()];
        if (result.species == 0)
        {
            if (role == Qt::FontRole)
            {
                QFont font;
                font.setItalic(true);
                return font;
            }
            if (role == Qt::ForegroundRole)
                return QColor(128, 128, 128);
        }
        if (role != Qt::DisplayRole)
            return { };
        switch (index.column())
        {
        case 0:
            return QString::number(result.seed, 16).toUpper().rightJustified(8, '0');
        case 1:
            return result.delay;
        case 2:
        {
            QStringList values;
            for (u32 delay : result.delays)
                values.emplace_back(QString::number(delay));
            return values.join(QStringLiteral(", "));
        }
        case 3:
            return result.advance;
        case 4:
            return result.species == 0 ? QStringLiteral("-")
                                       : result.group == 0 ? tr("None (Group A table)") : groupName(result.group);
        case 5:
            return result.slot == 0xff ? QStringLiteral("-") : QString::number(result.slot);
        case 6:
            return result.species == 0 ? QStringLiteral("-")
                                       : QString::fromStdString(Translator::getSpecies(std::vector<u16> { result.species })[0]);
        case 7:
            return result.species == 0 ? QStringLiteral("-") : QString::number(result.shakes);
        default:
            return { };
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
            return { };
        static const QStringList headers = { tr("Seed"),  tr("Target Delay"), tr("Delay Cluster"), tr("Advance"),
                                             tr("Group"), tr("Slot"),         tr("Pok\303\251mon"),       tr("Shakes") };
        return headers[section];
    }
};

class HoneyTreeSearchControls
{
public:
    ComboBox *location;
    QLineEdit *npcNoise;
    ComboBox *species;
    QCheckBox *sameTree;
    QComboBox *previousGroup;
    std::array<CheckList *, 3> slotChecks;
    CheckList *shakes;

    HoneyTreeSearchControls(QGroupBox *settingsGroup, QGroupBox *filterGroup)
    {
        auto *settings = new QFormLayout(settingsGroup);
        settings->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        location = new ComboBox(settingsGroup);
        location->enableAutoComplete();
        location->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        location->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        npcNoise = new QLineEdit(settingsGroup);
        npcNoise->setReadOnly(true);
        npcNoise->setEnabled(false);
        species = new ComboBox(settingsGroup);
        species->enableAutoComplete();
        sameTree = new QCheckBox(HoneyTrees::tr("Slathered same Tree as before"), settingsGroup);
        previousGroup = new QComboBox(settingsGroup);
        previousGroup->addItems(
            { HoneyTrees::tr("Group A"), HoneyTrees::tr("Group B"), HoneyTrees::tr("Group C"), HoneyTrees::tr("No encounter") });
        previousGroup->setEnabled(false);
        settings->addRow(HoneyTrees::tr("Location"), location);
        settings->addRow(HoneyTrees::tr("NPC Noise"), npcNoise);
        settings->addRow(HoneyTrees::tr("Pok\303\251mon"), species);
        settings->addRow(sameTree);
        settings->addRow(HoneyTrees::tr("Previous Group"), previousGroup);

        auto *filters = new QFormLayout(filterGroup);
        const std::vector<std::string> slotNames = { "0", "1", "2", "3", "4", "5" };
        for (u8 group = 0; group < 3; group++)
        {
            slotChecks[group] = new CheckList(filterGroup);
            slotChecks[group]->addItems(slotNames);
            slotChecks[group]->setToolTip(HoneyTrees::tr("Click holding ctrl to reset"));
            filters->addRow(HoneyTrees::tr("Group %1").arg(QChar('A' + group)), slotChecks[group]);
        }
        shakes = new CheckList(filterGroup);
        shakes->addItems({ "0", "1", "2", "3" });
        shakes->setToolTip(HoneyTrees::tr("Click holding ctrl to reset"));
        filters->addRow(HoneyTrees::tr("Shakes"), shakes);

        QObject::connect(sameTree, &QCheckBox::toggled, previousGroup, &QComboBox::setEnabled);
        QObject::connect(location, &QComboBox::currentIndexChanged, location, [this] {
            updateNPCNoise();
            updateSpeciesItems();
        });
        QObject::connect(species, &QComboBox::currentIndexChanged, species, [this] { updateSpeciesFilters(); });
    }

    void update(const Profile4 &profile)
    {
        this->profile = &profile;
        const int selectedLocation = location->currentData().toInt();
        std::vector<std::pair<QString, u8>> locationEntries;
        for (const auto &entry : locations)
        {
            QString name = HoneyTrees::tr(entry.name);
            if (HoneyTreeGenerator::isMunchlaxTree(profile.getTID(), profile.getSID(), entry.index))
                name += HoneyTrees::tr(" (Munchlax)");
            locationEntries.emplace_back(name, entry.index);
        }
        std::ranges::sort(locationEntries, [](const auto &a, const auto &b) { return QString::localeAwareCompare(a.first, b.first) < 0; });
        location->clear();
        for (const auto &[name, index] : locationEntries)
            location->addItem(name, index);
        location->setMinimumWidth(0);
        location->setMinimumWidth(location->sizeHint().width());
        const int restore = location->findData(selectedLocation);
        location->setCurrentIndex(restore < 0 ? 0 : restore);
        updateNPCNoise();

        updateSpeciesItems();
    }

private:
    const Profile4 *profile = nullptr;

    void updateNPCNoise()
    {
        if (location->currentIndex() < 0)
            npcNoise->clear();
        else
            npcNoise->setText(npcNoiseName(location->getCurrentUChar()));
    }

    void updateSpeciesItems()
    {
        if (profile == nullptr || location->currentIndex() < 0)
            return;
        const u16 selectedSpecies = species->currentData().toUInt();
        const bool munchlaxTree
            = HoneyTreeGenerator::isMunchlaxTree(profile->getTID(), profile->getSID(), location->getCurrentUChar());
        const u16 munchlax = HoneyTreeGenerator::getSpecies(profile->getVersion(), 3, 0);
        std::set<u16> unique;
        for (u8 group = 1; group <= 3; group++)
        {
            for (u8 slot = 0; slot < 6; slot++)
            {
                const u16 value = HoneyTreeGenerator::getSpecies(profile->getVersion(), group, slot);
                if (munchlaxTree || value != munchlax)
                    unique.insert(value);
            }
        }
        std::vector<std::pair<QString, u16>> entries;
        for (u16 value : unique)
            entries.emplace_back(QString::fromStdString(Translator::getSpecies(std::vector<u16> { value })[0]), value);
        std::ranges::sort(entries, [](const auto &a, const auto &b) { return QString::localeAwareCompare(a.first, b.first) < 0; });
        species->blockSignals(true);
        species->clear();
        species->addItem(HoneyTrees::tr("Any"), 0);
        species->addItem(HoneyTrees::tr("None"), noSpecies);
        for (const auto &[name, value] : entries)
            species->addItem(name, value);
        const int restore = species->findData(selectedSpecies);
        species->setCurrentIndex(restore < 0 ? 0 : restore);
        species->blockSignals(false);
        updateSpeciesFilters();
    }

    void updateSpeciesFilters()
    {
        if (profile == nullptr)
            return;
        const u16 selected = species->getCurrentUShort();
        for (u8 group = 1; group <= 3; group++)
        {
            if (selected == 0 || selected == noSpecies)
            {
                slotChecks[group - 1]->resetChecks();
                slotChecks[group - 1]->setEnabled(selected == 0);
                continue;
            }
            std::vector<bool> matches(6, false);
            bool groupMatch = false;
            for (u8 slot = 0; slot < 6; slot++)
            {
                matches[slot] = HoneyTreeGenerator::getSpecies(profile->getVersion(), group, slot) == selected;
                groupMatch |= matches[slot];
            }
            slotChecks[group - 1]->setEnabled(groupMatch);
            if (groupMatch)
                slotChecks[group - 1]->setChecks(matches);
            else
                slotChecks[group - 1]->resetChecks();
        }
    }
};

struct HoneyTreeSearchParameters
{
    Game version;
    u16 tid;
    u16 sid;
    u8 location;
    u16 species;
    bool sameTree;
    u8 previousGroup;
    std::array<std::array<bool, 6>, 3> slotChecks;
    std::array<bool, 4> shakes;
};

class HoneyTreeSearcher final : public SearcherBase<HoneyTreeSearchResult>
{
public:
    HoneyTreeSearcher(u32 minDelay, u32 maxDelay, u32 minAdvance, u32 maxAdvance, u16 consecutive, bool parity,
                      HoneyTreeSearchParameters parameters) :
        minDelay(minDelay),
        maxDelay(maxDelay),
        minAdvance(minAdvance),
        maxAdvance(maxAdvance),
        consecutive(consecutive),
        parity(parity),
        parameters(parameters)
    {
    }

    void start(int threads)
    {
        setMaxProgress(256 * 24);
        activeThreads.store(threads);
        for (int i = 0; i < threads; i++)
            threadContainer.emplace_back([this] {
                run();
                activeThreads.fetch_sub(1);
            });
    }

private:
    struct Match
    {
        u32 delay;
        HoneyTreeState state;
    };

    bool matches(const HoneyTreeState &state) const
    {
        if (!parameters.shakes[state.shakes])
            return false;
        if (parameters.species == noSpecies)
            return state.species == 0;
        if (state.species == 0)
            return parameters.species == 0;
        if (parameters.species != 0 && state.species != parameters.species)
            return false;
        const u8 group = state.group == 0 ? 0 : state.group - 1;
        return state.slot != 0xff && parameters.slotChecks[group][state.slot];
    }

    void addRun(u32 prefix, const std::vector<Match> &run, std::vector<HoneyTreeSearchResult> &found) const
    {
        if (run.size() < consecutive)
            return;
        const Match &target = run[(run.size() - 1) / 2];
        const u32 highestAdvance
            = std::ranges::max_element(run, { }, [](const Match &match) { return match.state.advances; })->state.advances;
        HoneyTreeSearchResult result { prefix + target.delay, target.delay, target.state.advances, target.state.group, target.state.slot,
                                        target.state.species, target.state.shakes, highestAdvance, { } };
        for (const Match &match : run)
            result.delays.emplace_back(match.delay);
        found.emplace_back(std::move(result));
    }

    void scan(u32 prefix, u32 first, u8 step, std::vector<HoneyTreeSearchResult> &found) const
    {
        std::vector<Match> run;
        const bool munchlax = HoneyTreeGenerator::isMunchlaxTree(parameters.tid, parameters.sid, parameters.location);
        for (u32 delay = first; delay <= maxDelay; delay += step)
        {
            if (cancelled.load(std::memory_order_relaxed))
                return;
            HoneyTreeGenerator generator(minAdvance, maxAdvance - minAdvance, parameters.sameTree, parameters.previousGroup, munchlax,
                                         parameters.version);
            const auto states = generator.generate(prefix + delay);
            const auto match = std::ranges::find_if(states, [this](const HoneyTreeState &state) { return matches(state); });
            if (match != states.end())
                run.push_back({ delay, *match });
            else
            {
                addRun(prefix, run, found);
                run.clear();
            }
            if (maxDelay - delay < step)
                break;
        }
        addRun(prefix, run, found);
    }

    void run()
    {
        while (true)
        {
            const u64 value = index.fetch_add(1);
            if (value >= 256 * 24 || cancelled.load(std::memory_order_relaxed))
                return;
            const u32 prefix = (static_cast<u32>(value / 24) << 24) | (static_cast<u32>(value % 24) << 16);
            std::vector<HoneyTreeSearchResult> found;
            if (parity)
            {
                const u64 even = (minDelay & 1) == 0 ? minDelay : static_cast<u64>(minDelay) + 1;
                const u64 odd = (minDelay & 1) != 0 ? minDelay : static_cast<u64>(minDelay) + 1;
                if (even <= maxDelay)
                    scan(prefix, static_cast<u32>(even), 2, found);
                if (odd <= maxDelay)
                    scan(prefix, static_cast<u32>(odd), 2, found);
            }
            else
            {
                scan(prefix, minDelay, 1, found);
            }
            if (!found.empty())
            {
                std::lock_guard<std::mutex> guard(mutex);
                results.insert(results.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
            }
            progress.fetch_add(1, std::memory_order_relaxed);
        }
    }

    u32 minDelay;
    u32 maxDelay;
    u32 minAdvance;
    u32 maxAdvance;
    u16 consecutive;
    bool parity;
    HoneyTreeSearchParameters parameters;
};

HoneyTrees::HoneyTrees(QWidget *parent) : QWidget(parent), currentProfile(nullptr), targetSpeciesActive(false), targetSpecies(0)
{
    setWindowTitle(tr("Honey Trees"));
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *root = new QVBoxLayout(this);
    profileDisplay = new ProfileDisplay4(this);
    profileDisplay->setup(settingPrefix, Game::DPPt, Game::Diamond, true);
    profileDisplay->setTitle(QString());
    root->addWidget(profileDisplay);

    auto *tabs = new QTabWidget(this);
    auto *generator = new QWidget(tabs);
    auto *generatorLayout = new QVBoxLayout(generator);
    auto *controls = new QHBoxLayout;

    auto *rngGroup = new QGroupBox(tr("RNG Info"), generator);
    auto *rng = new QFormLayout(rngGroup);
    seed = new TextBox(rngGroup);
    seed->setValues(InputType::Seed32Bit);
    initialAdvances = new TextBox(rngGroup);
    initialAdvances->setValues(InputType::Advance32Bit);
    initialAdvances->setText(QStringLiteral("0"));
    maxAdvances = new TextBox(rngGroup);
    maxAdvances->setValues(InputType::Advance32Bit);
    maxAdvances->setText(QStringLiteral("100"));
    rng->addRow(tr("Seed"), seed);
    rng->addRow(tr("Initial Advances"), initialAdvances);
    rng->addRow(tr("Max Advances"), maxAdvances);
    auto *generateButton = new QPushButton(tr("Generate"), rngGroup);
    rng->addRow(generateButton);
    controls->addWidget(rngGroup);

    auto *settingsGroup = new QGroupBox(tr("Settings"), generator);
    auto *settings = new QFormLayout(settingsGroup);
    settings->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    location = new ComboBox(settingsGroup);
    location->enableAutoComplete();
    location->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    location->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    species = new ComboBox(settingsGroup);
    species->enableAutoComplete();
    sameTree = new QCheckBox(tr("Slathered same Tree as before"), settingsGroup);
    previousGroup = new QComboBox(settingsGroup);
    previousGroup->addItems({ tr("Group A"), tr("Group B"), tr("Group C"), tr("No encounter") });
    previousGroup->setEnabled(false);
    settings->addRow(tr("Location"), location);
    npcNoise = new QLineEdit(settingsGroup);
    npcNoise->setReadOnly(true);
    npcNoise->setEnabled(false);
    settings->addRow(tr("NPC Noise"), npcNoise);
    settings->addRow(tr("Pok\303\251mon"), species);
    settings->addRow(sameTree);
    settings->addRow(tr("Previous Group"), previousGroup);
    controls->addWidget(settingsGroup, 1);

    auto *filterGroup = new QGroupBox(tr("Filters"), generator);
    auto *filters = new QFormLayout(filterGroup);
    const std::vector<std::string> slotNames = { "0", "1", "2", "3", "4", "5" };
    for (u8 group = 0; group < 3; group++)
    {
        slotFilters[group] = new CheckList(filterGroup);
        slotFilters[group]->addItems(slotNames);
        slotFilters[group]->setToolTip(tr("Click holding ctrl to reset"));
        filters->addRow(tr("Group %1").arg(QChar('A' + group)), slotFilters[group]);
    }
    shakeFilter = new CheckList(filterGroup);
    shakeFilter->addItems({ "0", "1", "2", "3" });
    shakeFilter->setToolTip(tr("Click holding ctrl to reset"));
    filters->addRow(tr("Shakes"), shakeFilter);
    controls->addWidget(filterGroup, 2);
    generatorLayout->addLayout(controls);

    table = new TableView(generator);
    model = new HoneyTreeModel(table);
    table->setModel(model);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(false);
    auto *advanceFinder = table->addAction(tr("Advance Finder"));
    table->setPrimaryAction(advanceFinder);
    auto *removeTargetMark = table->addAction(tr("Remove target Mark"));
    table->setRemoveTargetAction(removeTargetMark);
    generatorLayout->addWidget(table, 1);
    tabs->addTab(generator, tr("Generator"));

    auto *searcher = new QWidget(tabs);
    auto *searcherLayout = new QVBoxLayout(searcher);
    auto *searcherControlsLayout = new QHBoxLayout;
    auto *searchRNGGroup = new QGroupBox(tr("RNG Info"), searcher);
    auto *searchRNG = new QFormLayout(searchRNGGroup);
    searchMinDelay = new TextBox(searchRNGGroup);
    searchMinDelay->setValues(InputType::Delay);
    searchMinDelay->setText(QStringLiteral("600"));
    searchMaxDelay = new TextBox(searchRNGGroup);
    searchMaxDelay->setValues(InputType::Delay);
    searchMaxDelay->setText(QStringLiteral("1000"));
    searchMinAdvance = new TextBox(searchRNGGroup);
    searchMinAdvance->setValues(InputType::Advance32Bit);
    searchMinAdvance->setText(QStringLiteral("0"));
    searchMaxAdvance = new TextBox(searchRNGGroup);
    searchMaxAdvance->setValues(InputType::Advance32Bit);
    searchMaxAdvance->setText(QStringLiteral("10"));
    searchConsecutive = new QSpinBox(searchRNGGroup);
    searchConsecutive->setRange(1, 999);
    searchConsecutive->setValue(1);
    searchDelayParity = new QCheckBox(tr("Account for Delay parity"), searchRNGGroup);
    searchDelayParity->setChecked(true);
    searchRNG->addRow(tr("Min Delay"), searchMinDelay);
    searchRNG->addRow(tr("Max Delay"), searchMaxDelay);
    searchRNG->addRow(tr("Min Advance"), searchMinAdvance);
    searchRNG->addRow(tr("Max Advance"), searchMaxAdvance);
    searchRNG->addRow(tr("Delay Cluster"), searchConsecutive);
    searchRNG->addRow(searchDelayParity);
    auto *searchButtons = new QHBoxLayout;
    searchButton = new QPushButton(tr("Search"), searchRNGGroup);
    searchCancelButton = new QPushButton(tr("Cancel"), searchRNGGroup);
    searchCancelButton->setEnabled(false);
    searchButtons->addWidget(searchButton);
    searchButtons->addWidget(searchCancelButton);
    searchRNG->addRow(searchButtons);
    searcherControlsLayout->addWidget(searchRNGGroup);

    auto *searchSettingsGroup = new QGroupBox(tr("Settings"), searcher);
    auto *searchFilterGroup = new QGroupBox(tr("Filters"), searcher);
    searchControls = new HoneyTreeSearchControls(searchSettingsGroup, searchFilterGroup);
    searcherControlsLayout->addWidget(searchSettingsGroup, 1);
    searcherControlsLayout->addWidget(searchFilterGroup, 1);
    searcherLayout->addLayout(searcherControlsLayout);
    searchProgress = new QProgressBar(searcher);
    searcherLayout->addWidget(searchProgress);
    auto *searchTable = new TableView(searcher);
    searchModel = new HoneyTreeSearchModel(searchTable);
    auto *searchProxy = new SortFilterProxyModel(searchTable, searchModel);
    searchTable->setModel(searchProxy);
    searchTable->setSortingEnabled(true);
    searchTable->sortByColumn(0, Qt::AscendingOrder);
    searchTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    searchTable->horizontalHeader()->setStretchLastSection(false);
    auto *seedToTime = new QAction(tr("Generate times for seed"), searchTable);
    searchTable->setPrimaryAction(seedToTime);
    searchTable->addAction(seedToTime);
    connect(seedToTime, &QAction::triggered, this, [this, searchTable, searchProxy] {
        if (!searchTable->currentIndex().isValid() || !currentProfile)
            return;
        const QModelIndex index = searchProxy->mapToSource(searchTable->currentIndex());
        const auto &result = searchModel->getItem(index.row());
        auto *time = new SeedToTime4(result.seed, currentProfile->getVersion());
        time->show();
    });
    auto *goToGenerator = searchTable->addAction(tr("Go to Generator"));
    searchTable->setSecondaryAction(goToGenerator);
    connect(goToGenerator, &QAction::triggered, this, [this, searchTable, searchProxy, tabs] {
        if (!searchTable->currentIndex().isValid())
            return;
        const QModelIndex index = searchProxy->mapToSource(searchTable->currentIndex());
        const auto &result = searchModel->getItem(index.row());

        location->setCurrentIndex(location->findData(searchControls->location->currentData()));
        species->setCurrentIndex(0);
        sameTree->setChecked(searchControls->sameTree->isChecked());
        previousGroup->setCurrentIndex(searchControls->previousGroup->currentIndex());
        for (u8 group = 0; group < 3; group++)
            slotFilters[group]->resetChecks();
        shakeFilter->resetChecks();

        seed->setText(QString::number(result.seed, 16).toUpper());
        maxAdvances->setText(QString::number(static_cast<u64>(result.highestAdvance) + 20));
        targetSpecies = result.species;
        targetSpeciesActive = true;
        table->clearTargetAdvance();
        tabs->setCurrentIndex(0);
    });
    searcherLayout->addWidget(searchTable, 1);
    tabs->addTab(searcher, tr("Searcher"));
    root->addWidget(tabs, 1);

    connect(profileDisplay, &ProfileDisplay4::profileChanged, this, &HoneyTrees::profileChanged);
    connect(profileDisplay, &ProfileDisplay4::profilesChanged, this, &HoneyTrees::profilesChanged);
    connect(location, &QComboBox::currentIndexChanged, this, [this] {
        updateNPCNoise();
        updateSlotLabels();
    });
    connect(species, &QComboBox::currentIndexChanged, this, &HoneyTrees::speciesChanged);
    connect(sameTree, &QCheckBox::toggled, previousGroup, &QComboBox::setEnabled);
    connect(generateButton, &QPushButton::clicked, this, &HoneyTrees::generate);
    connect(searchButton, &QPushButton::clicked, this, &HoneyTrees::search);
    connect(advanceFinder, &QAction::triggered, this, [this] {
        auto *finder = new AdvanceFinder(model, table, currentProfile, this);
        finder->show();
    });
    connect(removeTargetMark, &QAction::triggered, this, [this] {
        targetSpeciesActive = false;
        table->clearTargetAdvance();
    });
    updateProfiles();

    QSettings store;
    store.beginGroup(settingPrefix);
    searchMinDelay->setText(store.value(QStringLiteral("searchMinDelay"), searchMinDelay->text()).toString());
    searchMaxDelay->setText(store.value(QStringLiteral("searchMaxDelay"), searchMaxDelay->text()).toString());
    searchMinAdvance->setText(store.value(QStringLiteral("searchMinAdvance"), searchMinAdvance->text()).toString());
    searchMaxAdvance->setText(store.value(QStringLiteral("searchMaxAdvance"), searchMaxAdvance->text()).toString());
    searchConsecutive->setValue(store.value(QStringLiteral("searchConsecutive"), searchConsecutive->value()).toInt());
    searchDelayParity->setChecked(store.value(QStringLiteral("searchDelayParity"), true).toBool());
    if (store.contains(QStringLiteral("geometry")))
        restoreGeometry(store.value(QStringLiteral("geometry")).toByteArray());
    else
        resize(1000, 600);
    store.endGroup();
}

HoneyTrees::~HoneyTrees()
{
    QSettings store;
    store.beginGroup(settingPrefix);
    store.setValue(QStringLiteral("searchMinDelay"), searchMinDelay->text());
    store.setValue(QStringLiteral("searchMaxDelay"), searchMaxDelay->text());
    store.setValue(QStringLiteral("searchMinAdvance"), searchMinAdvance->text());
    store.setValue(QStringLiteral("searchMaxAdvance"), searchMaxAdvance->text());
    store.setValue(QStringLiteral("searchConsecutive"), searchConsecutive->value());
    store.setValue(QStringLiteral("searchDelayParity"), searchDelayParity->isChecked());
    store.setValue(QStringLiteral("geometry"), saveGeometry());
    store.endGroup();
}

bool HoneyTrees::hasProfiles() const
{
    return currentProfile != nullptr;
}
void HoneyTrees::updateProfiles()
{
    profileDisplay->updateProfiles();
}

void HoneyTrees::profileChanged(const Profile4 &profile)
{
    currentProfile = &profile;
    updateLocations();
    updateSlotLabels();
    searchControls->update(profile);
}

void HoneyTrees::updateLocations()
{
    const int selected = location->currentData().toInt();
    std::vector<std::pair<QString, u8>> entries;
    for (const auto &entry : locations)
    {
        QString name = tr(entry.name);
        if (HoneyTreeGenerator::isMunchlaxTree(currentProfile->getTID(), currentProfile->getSID(), entry.index))
            name += tr(" (Munchlax)");
        entries.emplace_back(name, entry.index);
    }
    std::ranges::sort(entries, [](const auto &a, const auto &b) { return QString::localeAwareCompare(a.first, b.first) < 0; });
    location->clear();
    for (const auto &[name, index] : entries)
        location->addItem(name, index);
    location->setMinimumWidth(0);
    location->setMinimumWidth(location->sizeHint().width());
    const int restore = location->findData(selected);
    location->setCurrentIndex(restore < 0 ? 0 : restore);
    updateNPCNoise();
}

void HoneyTrees::updateNPCNoise()
{
    if (location->currentIndex() < 0)
    {
        npcNoise->clear();
        return;
    }
    npcNoise->setText(npcNoiseName(location->getCurrentUChar()));
}

void HoneyTrees::updateSlotLabels()
{
    if (!currentProfile || location->currentIndex() < 0)
        return;
    const u16 selectedSpecies = species->currentData().toUInt();
    const bool munchlaxTree
        = HoneyTreeGenerator::isMunchlaxTree(currentProfile->getTID(), currentProfile->getSID(), location->getCurrentUChar());
    const u16 munchlax = HoneyTreeGenerator::getSpecies(currentProfile->getVersion(), 3, 0);
    std::set<u16> unique;
    for (u8 group = 1; group <= 3; group++)
    {
        for (u8 slot = 0; slot < 6; slot++)
        {
            const u16 value = HoneyTreeGenerator::getSpecies(currentProfile->getVersion(), group, slot);
            if (munchlaxTree || value != munchlax)
                unique.insert(value);
        }
    }
    std::vector<std::pair<QString, u16>> entries;
    for (u16 value : unique)
        entries.emplace_back(QString::fromStdString(Translator::getSpecies(std::vector<u16> { value })[0]), value);
    std::ranges::sort(entries, [](const auto &a, const auto &b) { return QString::localeAwareCompare(a.first, b.first) < 0; });
    species->blockSignals(true);
    species->clear();
    species->addItem(tr("Any"), 0);
    species->addItem(tr("None"), noSpecies);
    for (const auto &[name, value] : entries)
        species->addItem(name, value);
    const int restore = species->findData(selectedSpecies);
    species->setCurrentIndex(restore < 0 ? 0 : restore);
    species->blockSignals(false);
    speciesChanged(0);
}

void HoneyTrees::speciesChanged(int)
{
    if (!currentProfile)
        return;
    const u16 selected = species->getCurrentUShort();
    for (u8 group = 1; group <= 3; group++)
    {
        if (selected == noSpecies)
        {
            slotFilters[group - 1]->resetChecks();
            slotFilters[group - 1]->setEnabled(false);
            continue;
        }

        if (selected == 0)
        {
            slotFilters[group - 1]->setEnabled(true);
            slotFilters[group - 1]->resetChecks();
            continue;
        }

        std::vector<bool> matches(6, false);
        bool groupMatch = false;
        for (u8 slot = 0; slot < 6; slot++)
        {
            matches[slot] = HoneyTreeGenerator::getSpecies(currentProfile->getVersion(), group, slot) == selected;
            groupMatch |= matches[slot];
        }
        slotFilters[group - 1]->setEnabled(groupMatch);
        if (groupMatch)
            slotFilters[group - 1]->setChecks(matches);
        else
            slotFilters[group - 1]->resetChecks();
    }
}

void HoneyTrees::generate()
{
    if (!currentProfile || location->currentIndex() < 0)
        return;
    const u32 initial = initialAdvances->getUInt();
    const bool munchlax
        = HoneyTreeGenerator::isMunchlaxTree(currentProfile->getTID(), currentProfile->getSID(), location->getCurrentUChar());
    HoneyTreeGenerator generator(initial, maxAdvances->getUInt(), sameTree->isChecked(), static_cast<u8>(previousGroup->currentIndex() + 1),
                                 munchlax, currentProfile->getVersion());
    if (previousGroup->currentIndex() == 3)
        generator = HoneyTreeGenerator(initial, maxAdvances->getUInt(), sameTree->isChecked(), 0, munchlax, currentProfile->getVersion());

    std::vector<HoneyTreeState> filtered;
    const u16 selectedSpecies = species->getCurrentUShort();
    const auto checkedShakes = shakeFilter->getChecked();
    for (const auto &state : generator.generate(seed->getUInt()))
    {
        if (!checkedShakes[state.shakes])
            continue;
        if (state.group == 0 && state.species == 0)
        {
            if (selectedSpecies == 0 || selectedSpecies == noSpecies)
                filtered.emplace_back(state);
            continue;
        }
        const u8 filterGroup = state.group == 0 ? 0 : state.group - 1;
        if (!slotFilters[filterGroup]->isEnabled())
            continue;
        const auto checkedSlots = slotFilters[filterGroup]->getChecked();
        if (state.slot == 0xff || !checkedSlots[state.slot])
            continue;
        if (selectedSpecies != 0 && state.species != selectedSpecies)
            continue;
        filtered.emplace_back(state);
    }
    model->clearModel();
    model->addItems(filtered);
    if (targetSpeciesActive)
    {
        std::vector<u32> targetAdvances;
        for (const auto &state : filtered)
        {
            if (state.species == targetSpecies)
                targetAdvances.emplace_back(state.advances);
        }
        table->setTargetAdvances(targetAdvances);
    }
    else
    {
        table->clearTargetAdvance();
    }
}

void HoneyTrees::search()
{
    if (currentProfile == nullptr || searchControls->location->currentIndex() < 0)
        return;
    const u32 minDelay = searchMinDelay->getUInt();
    const u32 maxDelay = searchMaxDelay->getUInt();
    const u32 minAdvance = searchMinAdvance->getUInt();
    const u32 maxAdvance = searchMaxAdvance->getUInt();
    if (minDelay > maxDelay || minAdvance > maxAdvance)
    {
        QMessageBox::warning(this, tr("Invalid range"), tr("The minimum value must not exceed the maximum value."));
        return;
    }
    HoneyTreeSearchParameters parameters;
    parameters.version = currentProfile->getVersion();
    parameters.tid = currentProfile->getTID();
    parameters.sid = currentProfile->getSID();
    parameters.location = searchControls->location->getCurrentUChar();
    parameters.species = searchControls->species->getCurrentUShort();
    parameters.sameTree = searchControls->sameTree->isChecked();
    parameters.previousGroup
        = searchControls->previousGroup->currentIndex() == 3 ? 0 : static_cast<u8>(searchControls->previousGroup->currentIndex() + 1);
    for (u8 group = 0; group < 3; group++)
    {
        const auto checked = searchControls->slotChecks[group]->getChecked();
        std::ranges::copy_n(checked.begin(), 6, parameters.slotChecks[group].begin());
    }
    const auto checkedShakes = searchControls->shakes->getChecked();
    std::ranges::copy_n(checkedShakes.begin(), 4, parameters.shakes.begin());

    searchModel->clearModel();
    auto *searcher = new HoneyTreeSearcher(minDelay, maxDelay, minAdvance, maxAdvance, static_cast<u16>(searchConsecutive->value()),
                                           searchDelayParity->isChecked(), parameters);
    QSettings settings;
    const int threads = std::max(1, settings.value(QStringLiteral("settings/threads"), 1).toInt());
    searchButton->setEnabled(false);
    searchCancelButton->setEnabled(true);
    searchProgress->setValue(0);
    auto *timer = new QTimer(this);
    connect(searchCancelButton, &QPushButton::clicked, timer, [=] {
        searcher->cancelSearch();
        searchCancelButton->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [=] {
        searchModel->addItems(searcher->getResults());
        searchProgress->setValue(searcher->getProgress());
        if (!searcher->isSearching())
        {
            timer->stop();
            searchModel->addItems(searcher->getResults());
            searchButton->setEnabled(true);
            searchCancelButton->setEnabled(false);
            delete searcher;
            timer->deleteLater();
        }
    });
    searcher->start(threads);
    timer->start(500);
}
