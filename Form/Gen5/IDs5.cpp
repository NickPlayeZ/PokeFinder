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
#include <Core/Gen5/WhiteForestData.hpp>
#include <Core/Gen5/Searchers/IDSearcher5.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/RNG/SHA1.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Util/Utilities.hpp>
#include <Form/Gen5/Profile/ProfileManager5.hpp>
#include <Form/Controls/CheckList.hpp>
#include <Model/Gen5/IDModel5.hpp>
#include <QAction>
#include <QBrush>
#include <QDate>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QItemSelectionModel>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

static const QString settingPrefix = QStringLiteral("ids5");

IDs5::IDs5(QWidget *parent) :
    QWidget(parent), ui(new Ui::IDs5), currentProfile(nullptr), updatingResidents(false)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    ui->profileDisplay->setup(settingPrefix, Game::Gen5);
    ui->profileDisplay->setIDsEditable(false);

    updatingResidents = true;
    ui->comboMenuResidents->setFull(false);
    ui->comboMenuEntralinkResidents->setFull(false);
    for (u8 resident : WhiteForest5::residentNameOrder)
    {
        const QString name = QString::fromUtf8(WhiteForest5::residentNames[resident]);
        ui->comboMenuResidents->addItem(name, resident);
        ui->comboMenuEntralinkResidents->addItem(name, resident);
    }
    ui->comboMenuResidents->resetChecks();
    ui->comboMenuEntralinkResidents->resetChecks();
    updatingResidents = false;

    model = new IDModel5(ui->tableView);
    ui->tableView->setModel(model);
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { updateResidentInfoMarks(); });

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
    connect(ui->checkBoxResidents, &QCheckBox::toggled, this, [this](bool checked) {
        ui->comboMenuResidents->setEnabled(checked);
        ui->labelEntralinkResidents->setEnabled(checked);
        ui->comboMenuEntralinkResidents->setEnabled(checked);
        ui->pushButtonResidentInfo->setEnabled(checked);
        model->clearModel();
        model->setShowResidents(false);
        updateResidentInfoMarks();
    });
    connect(ui->comboMenuResidents, &CheckList::checksChanged, this, [this] { updateResidentSelection(false); });
    connect(ui->comboMenuEntralinkResidents, &CheckList::checksChanged, this, [this] { updateResidentSelection(true); });
    connect(ui->pushButtonResidentInfo, &QPushButton::clicked, this, &IDs5::showResidentInfo);

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
    const bool showResidents = ui->checkBoxSeedFinderResidents->isChecked()
                               && (currentProfile->getVersion() & Game::BW) != Game::None;
    model->setShowResidents(showResidents);

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
    const bool generateResidents = (currentProfile->getVersion() & Game::BW) != Game::None;
    IDGenerator5 generator(0, maxAdvance, 0, false, false, *currentProfile, filter, {}, {}, generateResidents);
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
    const bool useResidents = ui->checkBoxResidents->isChecked()
                              && (currentProfile->getVersion() & Game::BW) != Game::None;
    model->setShowResidents(useResidents);

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
    std::vector<u8> residents;
    std::vector<u8> entralinkResidents;
    if (useResidents)
    {
        for (u16 resident : ui->comboMenuResidents->getExplicitCheckedData()) residents.emplace_back(static_cast<u8>(resident));
        for (u16 resident : ui->comboMenuEntralinkResidents->getExplicitCheckedData())
            entralinkResidents.emplace_back(static_cast<u8>(resident));
    }

    IDGenerator5 generator(0, ui->textBoxMaxAdvances->getUInt(), pid, usePID, useXOR, *currentProfile, filter, residents,
                           entralinkResidents, useResidents);

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

void IDs5::updateResidentSelection(bool entralink)
{
    if (updatingResidents) return;

    updatingResidents = true;
    CheckList *changedMenu = entralink ? ui->comboMenuEntralinkResidents : ui->comboMenuResidents;
    std::vector<u16> &changedSelection = entralink ? entralinkResidentSelection : residentSelection;
    const size_t limit = entralink ? 3 : 10;
    std::vector<u16> selection;
    for (u16 resident : changedMenu->getExplicitCheckedData()) selection.emplace_back(resident);
    if (selection.size() > limit)
    {
        changedMenu->setCheckedData(changedSelection);
        updatingResidents = false;
        return;
    }

    changedSelection = selection;
    CheckList *otherMenu = entralink ? ui->comboMenuResidents : ui->comboMenuEntralinkResidents;
    std::vector<u16> &otherSelection = entralink ? residentSelection : entralinkResidentSelection;
    std::erase_if(otherSelection, [&](u16 resident) { return std::ranges::contains(selection, resident); });
    otherMenu->setCheckedData(otherSelection);
    updatingResidents = false;
    updateResidentInfoMarks();
}

void IDs5::showResidentInfo()
{
    if (currentProfile == nullptr) return;

    const bool white = currentProfile->getVersion() == Game::White;
    const int columns = white ? 7 : 4;
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Resident Info"));
    auto *layout = new QVBoxLayout(dialog);
    auto *table = new QTableWidget(static_cast<int>(WhiteForest5::residentNames.size()), columns, dialog);
    if (white)
    {
        table->setHorizontalHeaderLabels(
            { tr("Resident"), tr("Grass Pokémon"), tr("Weight"), tr("Surfing Pokémon"), tr("Weight"), tr("Item"), tr("Item Odds") });
    }
    else
    {
        table->setHorizontalHeaderLabels({ tr("Resident"), tr("Party Pokémon"), tr("Item"), tr("Price") });
    }

    for (u8 row = 0; row < WhiteForest5::residentNameOrder.size(); row++)
    {
        const u8 resident = WhiteForest5::residentNameOrder[row];
        QStringList values;
        values.emplace_back(QString::fromUtf8(WhiteForest5::residentNames[resident]));
        if (white)
        {
            const auto &data = WhiteForest5::residents[resident];
            values << QString::fromStdString(Translator::getSpecie(data.grassSpecies)) << QString::number(data.grassWeight)
                   << QString::fromStdString(Translator::getSpecie(data.waterSpecies)) << QString::number(data.waterWeight)
                   << QString::fromStdString(Translator::getItem(data.item)) << tr("%1%").arg(data.itemChance);
        }
        else
        {
            const auto &data = WhiteForest5::blackCityResidents[resident];
            QString party = QString::fromStdString(Translator::getSpecie(data.species1));
            if (data.species2 != 0)
            {
                party += QStringLiteral(", ") + QString::fromStdString(Translator::getSpecie(data.species2));
            }
            values << party << QString::fromStdString(Translator::getItem(data.item)) << QString::number(data.price);
        }

        for (int column = 0; column < values.size(); column++)
        {
            auto *item = new QTableWidgetItem(values[column]);
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
    dialog->resize(white ? 850 : 650, 350);
    residentInfoTables.emplace_back(table);
    updateResidentInfoMarks();
    dialog->show();
}

void IDs5::updateResidentInfoMarks()
{
    std::erase_if(residentInfoTables, [](const QPointer<QTableWidget> &table) { return table.isNull(); });
    if (residentInfoTables.empty()) return;

    std::array<bool, 30> selectedResidents {};
    const QModelIndex current = ui->tableView->currentIndex();
    const bool resultSelected = current.isValid() && ui->tableView->selectionModel()->isSelected(current);
    if (resultSelected)
    {
        const auto &state = model->getItem(current.row()).getState();
        for (u8 resident : state.getResidents())
            if (resident < selectedResidents.size()) selectedResidents[resident] = true;
        for (u8 resident : state.getEntralinkResidents())
            if (resident < selectedResidents.size()) selectedResidents[resident] = true;
    }
    else if (ui->checkBoxResidents->isChecked())
    {
        for (u16 resident : ui->comboMenuResidents->getExplicitCheckedData())
            if (resident < selectedResidents.size()) selectedResidents[resident] = true;
        for (u16 resident : ui->comboMenuEntralinkResidents->getExplicitCheckedData())
            if (resident < selectedResidents.size()) selectedResidents[resident] = true;
    }

    QSettings settings;
    const bool markEnabled = settings.value(QStringLiteral("settings/targetMarkEnabled"), true).toBool();
    const QColor mark = settings.value(QStringLiteral("settings/targetMarkColor"), QColor(Qt::red)).value<QColor>();
    const int alpha = settings.value(QStringLiteral("settings/targetMarkAlpha"), 128).toInt();

    for (QTableWidget *table : residentInfoTables)
    {
        const QColor base = table->palette().color(QPalette::Base);
        auto blend = [alpha](int foreground, int background) {
            return (foreground * alpha + background * (255 - alpha)) / 255;
        };
        const QColor markedBackground(blend(mark.red(), base.red()), blend(mark.green(), base.green()),
                                      blend(mark.blue(), base.blue()));
        for (int row = 0; row < table->rowCount(); row++)
        {
            const u8 resident = WhiteForest5::residentNameOrder[row];
            for (int column = 0; column < table->columnCount(); column++)
            {
                auto *item = table->item(row, column);
                item->setBackground(markEnabled && selectedResidents[resident] ? QBrush(markedBackground) : QBrush());
            }
        }
    }
}

void IDs5::profileChanged(const Profile5 &profile)
{
    currentProfile = &profile;
    model->clearModel();
    model->setGame(profile.getVersion());
    model->setShowResidents(false);
    ui->textBoxTimer0Min->setText(QString::number(profile.getTimer0Min(), 16));
    ui->textBoxTimer0Max->setText(QString::number(profile.getTimer0Max(), 16));
    const bool bw = profile.getVersion() == Game::Black || profile.getVersion() == Game::White;
    ui->checkBoxResidents->setVisible(bw);
    ui->comboMenuResidents->setVisible(bw);
    ui->labelEntralinkResidents->setVisible(bw);
    ui->comboMenuEntralinkResidents->setVisible(bw);
    ui->pushButtonResidentInfo->setVisible(bw);
    ui->checkBoxSeedFinderResidents->setVisible(bw);
    if (!bw)
    {
        ui->checkBoxResidents->setChecked(false);
        ui->checkBoxSeedFinderResidents->setChecked(false);
    }
}
