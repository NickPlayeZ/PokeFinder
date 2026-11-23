#include "WhiteForest.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/Gen5/Generators/WhiteForestGenerator.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/Searchers/Searcher5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Gen5/WhiteForestData.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Util/Translator.hpp>
#include <Form/Controls/TableView.hpp>
#include <Form/Controls/DateEdit.hpp>
#include <Form/Gen5/Profile/ProfileDisplay5.hpp>
#include <Form/Gen5/ResultToGenerator.hpp>
#include <QAction>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

WhiteForest::WhiteForest(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("White Forest Items"));
    resize(1000, 650);

    auto *layout = new QVBoxLayout(this);
    profileDisplay = new ProfileDisplay5(this);
    profileDisplay->setup(QStringLiteral("whiteForest"), Game::White);
    residentButton = new QPushButton(tr("Residents"), profileDisplay);
    residentInfoButton = new QPushButton(tr("Resident Info"), profileDisplay);
    profileDisplay->addControls(residentButton, residentInfoButton);
    layout->addWidget(profileDisplay);

    tabs = new QTabWidget(this);
    auto *generatorTab = new QWidget(tabs);
    auto *itemsLayout = new QGridLayout(generatorTab);
    auto *itemRNGInfo = new QGroupBox(tr("RNG Info"), generatorTab);
    auto *itemRNGLayout = new QGridLayout(itemRNGInfo);
    auto *itemFilters = new QGroupBox(tr("Filters"), generatorTab);
    auto *itemFiltersLayout = new QGridLayout(itemFilters);
    itemSeed = new QLineEdit(generatorTab);
    itemInitial = new QSpinBox(generatorTab);
    itemMax = new QSpinBox(generatorTab);
    itemInitial->setMaximum(1000000000);
    itemMax->setMaximum(1000000000);
    itemMax->setValue(100);
    int row = 0;
    auto addItemInput = [&](const QString &name, QWidget *widget) {
        itemRNGLayout->addWidget(new QLabel(name, itemRNGInfo), row, 0);
        itemRNGLayout->addWidget(widget, row++, 1);
    };
    addItemInput(tr("Seed"), itemSeed);
    addItemInput(tr("Initial Advances"), itemInitial);
    addItemInput(tr("Max Advances"), itemMax);
    auto *itemButton = new QPushButton(tr("Generate"), generatorTab);
    itemRNGLayout->addWidget(itemButton, row, 0, 1, 2);
    for (u8 filterSlot = 0; filterSlot < generatorFilters.size(); filterSlot++)
    {
        generatorFilters[filterSlot] = new QComboBox(itemFilters);
        itemFiltersLayout->addWidget(generatorFilters[filterSlot], filterSlot % 5, filterSlot / 5);
        connect(generatorFilters[filterSlot], &QComboBox::currentIndexChanged, this, &WhiteForest::updateItemFilters);
    }
    itemFiltersLayout->setAlignment(Qt::AlignTop);
    itemsLayout->addWidget(itemRNGInfo, 0, 0);
    itemsLayout->addWidget(itemFilters, 0, 1);
    itemsLayout->setColumnStretch(0, 1);
    itemsLayout->setColumnStretch(1, 4);
    generatorTable = new TableView(generatorTab);
    itemModel = new QStandardItemModel(generatorTable);
    QStringList headers { tr("Advances") };
    for (u8 i = 1; i <= 10; i++) headers << tr("Item %1").arg(i);
    itemModel->setHorizontalHeaderLabels(headers);
    generatorTable->setModel(itemModel);
    generatorTable->setSortingEnabled(true);
    itemsLayout->addWidget(generatorTable, 1, 0, 1, 2);
    tabs->addTab(generatorTab, tr("Generator"));

    auto *searcherTab = new QWidget(tabs);
    auto *searcherLayout = new QGridLayout(searcherTab);
    auto *searcherRNGInfo = new QGroupBox(tr("RNG Info"), searcherTab);
    auto *searcherRNGLayout = new QGridLayout(searcherRNGInfo);
    auto *searcherFilterGroup = new QGroupBox(tr("Filters"), searcherTab);
    auto *searcherFilterLayout = new QGridLayout(searcherFilterGroup);
    searcherInitial = new QSpinBox(searcherTab);
    searcherMax = new QSpinBox(searcherTab);
    searcherStartDate = new DateEdit(searcherTab);
    searcherEndDate = new DateEdit(searcherTab);
    searcherStartDate->setDate(QDate::currentDate());
    searcherEndDate->setDate(QDate::currentDate());
    searcherInitial->setMaximum(1000000000);
    searcherMax->setMaximum(1000000000);
    searcherMax->setValue(100);
    int searcherRow = 0;
    auto addSearcherInput = [&](const QString &name, QWidget *widget) {
        searcherRNGLayout->addWidget(new QLabel(name, searcherRNGInfo), searcherRow, 0);
        searcherRNGLayout->addWidget(widget, searcherRow++, 1);
    };
    addSearcherInput(tr("Initial Advances"), searcherInitial);
    addSearcherInput(tr("Max Advances"), searcherMax);
    addSearcherInput(tr("Start Date"), searcherStartDate);
    addSearcherInput(tr("End Date"), searcherEndDate);
    auto *searchButtons = new QHBoxLayout;
    searchButton = new QPushButton(tr("Search"), searcherTab);
    cancelButton = new QPushButton(tr("Cancel"), searcherTab);
    cancelButton->setEnabled(false);
    searchButtons->addWidget(searchButton);
    searchButtons->addWidget(cancelButton);
    searcherRNGLayout->addLayout(searchButtons, searcherRow, 0, 1, 2);
    for (u8 filterSlot = 0; filterSlot < searcherFilters.size(); filterSlot++)
    {
        searcherFilters[filterSlot] = new QComboBox(searcherFilterGroup);
        searcherFilterLayout->addWidget(searcherFilters[filterSlot], filterSlot % 5, filterSlot / 5);
        connect(searcherFilters[filterSlot], &QComboBox::currentIndexChanged, this, &WhiteForest::updateItemFilters);
    }
    searcherFilterLayout->setAlignment(Qt::AlignTop);
    searcherLayout->addWidget(searcherRNGInfo, 0, 0);
    searcherLayout->addWidget(searcherFilterGroup, 0, 1);
    searcherLayout->setColumnStretch(0, 1);
    searcherLayout->setColumnStretch(1, 4);
    progressBar = new QProgressBar(searcherTab);
    progressBar->setValue(0);
    searcherLayout->addWidget(progressBar, 1, 0, 1, 2);
    searcherTable = new TableView(searcherTab);
    searcherModel = new QStandardItemModel(searcherTable);
    QStringList searcherHeaders { tr("Seed") };
    for (u8 i = 1; i <= 10; i++) searcherHeaders << tr("Item %1").arg(i);
    searcherHeaders << tr("Date/Time") << tr("Timer0") << tr("Buttons") << tr("Advances");
    searcherModel->setHorizontalHeaderLabels(searcherHeaders);
    searcherTable->setModel(searcherModel);
    searcherTable->setSortingEnabled(true);
    searcherLayout->addWidget(searcherTable, 2, 0, 1, 2);
    tabs->addTab(searcherTab, tr("Searcher"));
    layout->addWidget(tabs, 1);

    auto *goToGenerator = searcherTable->addAction(tr("Go to Generator"));
    searcherTable->setPrimaryAction(goToGenerator);
    connect(goToGenerator, &QAction::triggered, this, &WhiteForest::goToGenerator);

    connect(profileDisplay, &ProfileDisplay5::profileChanged, this, &WhiteForest::profileChanged);
    connect(profileDisplay, &ProfileDisplay5::profilesChanged, this, &WhiteForest::profilesChanged);
    connect(residentButton, &QPushButton::clicked, this, &WhiteForest::editResidents);
    connect(residentInfoButton, &QPushButton::clicked, this, &WhiteForest::showResidentInfo);
    connect(itemButton, &QPushButton::clicked, this, &WhiteForest::generateItems);
    connect(searchButton, &QPushButton::clicked, this, &WhiteForest::searchItems);
    updateProfiles();
}

bool WhiteForest::hasProfiles() const { return profileDisplay->hasProfiles(); }
void WhiteForest::updateProfiles() { profileDisplay->updateProfiles(); }

bool WhiteForest::validateResidents(bool requireResident)
{
    std::array<bool, 30> used {};
    bool any = false;
    for (u8 resident : residents)
    {
        if (resident >= used.size()) continue;
        if (used[resident])
        {
            QMessageBox::warning(this, tr("Duplicate resident"), tr("Each White Forest resident can only occupy one slot."));
            return false;
        }
        used[resident] = true;
        any = true;
    }
    if (requireResident && !any)
    {
        QMessageBox::warning(this, tr("No residents"), tr("Select at least one White Forest resident."));
        return false;
    }
    return true;
}

void WhiteForest::editResidents()
{
    if (!currentProfile) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("White Forest Residents"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *residentLayout = new QGridLayout;
    std::array<QComboBox *, 10> inputs;
    std::array<u8, 10> displayedResidents;
    displayedResidents.fill(255);
    u8 displayedSlot = 0;
    for (u8 resident : residents)
        if (resident < WhiteForest5::residentNames.size()) displayedResidents[displayedSlot++] = resident;

    for (u8 residentSlot = 0; residentSlot < inputs.size(); residentSlot++)
    {
        residentLayout->addWidget(new QLabel(tr("Slot %1").arg(residentSlot + 1), &dialog), residentSlot / 5,
                                  (residentSlot % 5) * 2);
        inputs[residentSlot] = new QComboBox(&dialog);
        inputs[residentSlot]->addItem(tr("None"), 255);
        for (u8 resident : WhiteForest5::residentNameOrder)
            inputs[residentSlot]->addItem(QString::fromUtf8(WhiteForest5::residentNames[resident]), resident);
        inputs[residentSlot]->setCurrentIndex(inputs[residentSlot]->findData(displayedResidents[residentSlot]));
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

    std::array<u8, 10> selected;
    std::array<bool, 30> used {};
    for (u8 residentSlot = 0; residentSlot < selected.size(); residentSlot++)
    {
        selected[residentSlot] = inputs[residentSlot]->currentData().toUInt();
        const u8 resident = selected[residentSlot];
        if (resident >= used.size()) continue;
        if (used[resident])
        {
            QMessageBox::warning(this, tr("Duplicate resident"), tr("Each White Forest resident can only occupy one slot."));
            return;
        }
        used[resident] = true;
    }

    const Profile5 original = *currentProfile;
    const Profile5 update(original.getName(), original.getVersion(), original.getTID(), original.getSID(), original.getIVCache(),
                          original.getSHACache(), original.getMac(), original.getKeypresses(), original.getVCount(), original.getGxStat(),
                          original.getVFrame(), original.getSkipLR(), original.getTimer0Min(), original.getTimer0Max(),
                          original.getMemoryLink(), original.getShinyCharm(), original.getDSType(), original.getLanguage(),
                          original.getNsPokemonReleased(), original.getOvalCharm(), selected);
    ProfileLoader5::updateProfile(update, original);
    profileDisplay->updateProfiles();
    emit profilesChanged(5);
}

void WhiteForest::showResidentInfo()
{
    if (!currentProfile) return;

    std::array<bool, 30> selectedResidents {};
    for (u8 resident : residents)
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

void WhiteForest::updateResidentText()
{
    QStringList selected;
    for (u8 resident : residents)
        if (resident < WhiteForest5::residentNames.size()) selected.append(QString::fromUtf8(WhiteForest5::residentNames[resident]));
    residentButton->setToolTip(selected.isEmpty() ? tr("No residents selected") : selected.join(QStringLiteral(", ")));
}

void WhiteForest::updateItemFilters()
{
    std::vector<u8> possibleResidents;
    for (u8 resident : residents)
    {
        if (resident >= WhiteForest5::residents.size()) continue;
        possibleResidents.push_back(resident);
    }
    std::ranges::sort(possibleResidents, [](u8 left, u8 right) {
        const auto &leftData = WhiteForest5::residents[left];
        const auto &rightData = WhiteForest5::residents[right];
        const auto leftName = Translator::getItem(leftData.item);
        const auto rightName = Translator::getItem(rightData.item);
        return leftName == rightName ? leftData.itemChance < rightData.itemChance : leftName < rightName;
    });

    const size_t residentCount = possibleResidents.size();
    const auto updateFilters = [&](const std::array<QComboBox *, 10> &filters) {
        std::array<u8, 10> currentResidents;
        for (u8 filterSlot = 0; filterSlot < filters.size(); filterSlot++)
        {
            const uint value = filters[filterSlot]->currentData().toUInt();
            currentResidents[filterSlot] = value == 0 ? 255 : static_cast<u8>(value - 1);
        }

        for (u8 filterSlot = 0; filterSlot < filters.size(); filterSlot++)
        {
            auto *filter = filters[filterSlot];
            const QSignalBlocker blocker(filter);
            const u8 currentResident = currentResidents[filterSlot];
            filter->clear();
            filter->addItem(tr("Any"), 0);
            for (u8 resident : possibleResidents)
            {
                bool selectedElsewhere = false;
                for (u8 otherSlot = 0; otherSlot < currentResidents.size(); otherSlot++)
                {
                    if (otherSlot != filterSlot && currentResidents[otherSlot] == resident)
                    {
                        selectedElsewhere = true;
                        break;
                    }
                }
                if (selectedElsewhere) continue;
                const auto &data = WhiteForest5::residents[resident];
                filter->addItem(tr("%1 (%2%)").arg(QString::fromStdString(Translator::getItem(data.item))).arg(data.itemChance),
                                resident + 1);
            }
            const int index = currentResident == 255 ? 0 : filter->findData(currentResident + 1);
            filter->setCurrentIndex(index >= 0 ? index : 0);
            filter->setEnabled(filterSlot < residentCount);
            if (!filter->isEnabled()) filter->setCurrentIndex(0);
        }
    };
    updateFilters(generatorFilters);
    updateFilters(searcherFilters);
}

void WhiteForest::profileChanged(const Profile5 &profile)
{
    currentProfile = &profile;
    residents = profile.getWhiteForestResidents();
    residentButton->setVisible(profile.getVersion() == Game::White);
    updateResidentText();
    updateItemFilters();
}

void WhiteForest::generateItems()
{
    if (!currentProfile) return;
    if (!validateResidents(true)) return;
    bool okay = false;
    u64 seed = itemSeed->text().toULongLong(&okay, 16);
    if (!okay)
    {
        QMessageBox::warning(this, tr("Invalid seed"), tr("Enter a hexadecimal 64-bit seed."));
        return;
    }
    WhiteForestGenerator generator(itemInitial->value(), itemMax->value(), residents, *currentProfile);
    const auto states = generator.generateItems(seed);
    itemModel->removeRows(0, itemModel->rowCount());
    for (const auto &state : states)
    {
        std::array<u8, 256> required {};
        std::array<u8, 256> generated {};
        for (const QComboBox *filter : generatorFilters)
        {
            if (!filter->isEnabled()) continue;
            const uint value = filter->currentData().toUInt();
            if (value == 0) continue;
            const u8 resident = static_cast<u8>(value - 1);
            required[WhiteForest5::residents[resident].item]++;
        }
        for (u16 item : state.items)
            if (item < generated.size()) generated[item]++;
        bool matches = true;
        for (size_t item = 0; item < required.size(); item++)
        {
            if (required[item] > generated[item])
            {
                matches = false;
                break;
            }
        }
        if (!matches) continue;
        QList<QStandardItem *> row { new QStandardItem(QString::number(state.advances)) };
        for (u16 item : state.items)
            row << new QStandardItem(item == 0 ? QStringLiteral("-") : QString::fromStdString(Translator::getItem(item)));
        itemModel->appendRow(row);
    }
}

void WhiteForest::searchItems()
{
    if (!currentProfile) return;
    if (!validateResidents(true)) return;

    const Date start = searcherStartDate->getDate();
    const Date end = searcherEndDate->getDate();
    if (start > end)
    {
        QMessageBox::warning(this, tr("Invalid date range"), tr("Start date is after end date"));
        return;
    }

    std::array<u8, 256> requiredItems {};
    bool hasFilter = false;
    for (u8 filterSlot = 0; filterSlot < searcherFilters.size(); filterSlot++)
    {
        const auto *filter = searcherFilters[filterSlot];
        if (!filter->isEnabled()) continue;
        const uint value = filter->currentData().toUInt();
        if (value == 0) continue;
        const u8 resident = static_cast<u8>(value - 1);
        requiredItems[WhiteForest5::residents[resident].item]++;
        hasFilter = true;
    }
    if (!hasFilter)
    {
        QMessageBox::warning(this, tr("Missing filters"), tr("Select at least one item to search for."));
        return;
    }

    searcherModel->removeRows(0, searcherModel->rowCount());
    searchButton->setEnabled(false);
    cancelButton->setEnabled(true);
    progressBar->setValue(0);

    WhiteForestGenerator generator(searcherInitial->value(), searcherMax->value(), residents, *currentProfile, requiredItems);
    auto *searcher = new Searcher5<WhiteForestGenerator, WhiteForestItemState>(generator, *currentProfile);
    searcher->setMaxProgress(searcher->getMaxProgress(start, end));
    progressBar->setMaximum(100);

    QSettings settings;
    const int threads = std::max(1, settings.value(QStringLiteral("settings/threads"), 1).toInt());
    auto *timer = new QTimer(this);
    connect(cancelButton, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        cancelButton->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer] {
        const auto results = searcher->getResults();
        for (const auto &result : results)
        {
            const auto &state = result.getState();
            auto *seedItem = new QStandardItem(QString::number(result.getInitialSeed(), 16).toUpper());
            seedItem->setData(QVariant::fromValue<qulonglong>(result.getInitialSeed()), Qt::UserRole);
            seedItem->setData(state.advances, Qt::UserRole + 1);

            QList<QStandardItem *> row { seedItem };
            for (u16 item : state.items)
                row << new QStandardItem(item == 0 ? QStringLiteral("-") : QString::fromStdString(Translator::getItem(item)));
            row << new QStandardItem(QString::fromStdString(result.getDateTime().toString()))
                << new QStandardItem(QString::number(result.getTimer0(), 16).toUpper())
                << new QStandardItem(QString::fromStdString(Translator::getKeypresses(result.getButtons())))
                << new QStandardItem(QString::number(state.advances));
            searcherModel->appendRow(row);
        }
        progressBar->setValue(static_cast<int>(searcher->getProgress()));

        if (!searcher->isSearching())
        {
            timer->stop();
            searchButton->setEnabled(true);
            cancelButton->setEnabled(false);
            delete searcher;
            timer->deleteLater();
        }
    });

    searcher->startSearch(threads, start, end);
    timer->start(1000);
}

void WhiteForest::goToGenerator()
{
    const QModelIndex index = searcherTable->currentIndex();
    if (!index.isValid()) return;

    const QStandardItem *seedItem = searcherModel->item(index.row(), 0);
    if (!seedItem) return;
    const u64 seed = seedItem->data(Qt::UserRole).toULongLong();
    const u32 advances = seedItem->data(Qt::UserRole + 1).toUInt();
    for (auto *filter : generatorFilters)
    {
        const QSignalBlocker blocker(filter);
        filter->setCurrentIndex(0);
    }
    updateItemFilters();
    itemSeed->setText(QString::number(seed, 16).toUpper());
    itemInitial->setValue(0);
    itemMax->setValue(ResultToGenerator5::getMaxAdvances(seed, advances, 10, *currentProfile));
    tabs->setCurrentIndex(0);
    generateItems();
    generatorTable->setTargetAdvance(advances);
}
