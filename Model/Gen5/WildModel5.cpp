/*
 * This file is part of PokeFinder
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

#include "WildModel5.hpp"
#include <Core/Enum/Lead.hpp>
#include <Core/Gen5/Generators/WildGenerator5.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Util/Utilities.hpp>
#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QStringList>

namespace
{
constexpr int generatorStepsColumn = 3;
constexpr int generatorPhenomenonColumn = 4;
constexpr int searcherPassPowerColumn = 1;
constexpr int searcherDataTriggerColumn = 2;
constexpr int searcherDataPhenomenonColumn = 4;
constexpr int searcherHeaderTriggerColumn = 3;
constexpr int searcherHeaderPhenomenonColumn = 5;

QString getLeadName(Lead lead, u8 flags)
{
    if (lead == Lead::None)
    {
        return QCoreApplication::translate("WildSearcherModel5", "None");
    }

    if (flags != 0)
    {
        QStringList leads;
        if ((flags & (1 << 0)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Synchronize"));
        }

        bool cuteCharmM = (flags & (1 << 1)) != 0;
        bool cuteCharmF = (flags & (1 << 2)) != 0;
        if (cuteCharmM && cuteCharmF)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Cute Charm: ♂ or ♀ Lead"));
        }
        else if (cuteCharmM)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Cute Charm: ♂ Lead"));
        }
        else if (cuteCharmF)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Cute Charm: ♀ Lead"));
        }

        if ((flags & (1 << 3)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Magnet Pull"));
        }
        if ((flags & (1 << 4)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Static"));
        }
        if ((flags & (1 << 5)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Hustle / Pressure / Vital Spirit"));
        }
        if ((flags & (1 << 6)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Compound Eyes"));
        }
        if ((flags & (1 << 7)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel5", "Encounter / Step Modifier"));
        }

        return leads.join(" / ");
    }

    if (lead <= Lead::SynchronizeEnd)
    {
        return QCoreApplication::translate("WildSearcherModel5", "Synchronize");
    }

    switch (lead)
    {
    case Lead::CompoundEyes:
        return QCoreApplication::translate("WildSearcherModel5", "Compound Eyes");
    case Lead::CuteCharmM:
        return QCoreApplication::translate("WildSearcherModel5", "Cute Charm: ♂ Lead");
    case Lead::CuteCharmF:
        return QCoreApplication::translate("WildSearcherModel5", "Cute Charm: ♀ Lead");
    case Lead::MagnetPull:
        return QCoreApplication::translate("WildSearcherModel5", "Magnet Pull");
    case Lead::Static:
        return QCoreApplication::translate("WildSearcherModel5", "Static");
    case Lead::Pressure:
        return QCoreApplication::translate("WildSearcherModel5", "Hustle / Pressure / Vital Spirit");
    case Lead::SuctionCups:
        return QCoreApplication::translate("WildSearcherModel5", "Sticky Hold / Suction Cups");
    case Lead::ArenaTrap:
        return QCoreApplication::translate("WildSearcherModel5", "Arena Trap / Illuminate / No Guard");
    default:
        return QString();
    }
}

QString getLuckyPowerText(u8 power)
{
    QStringList powers;

    switch (PassPower5::getLuckyPower(power))
    {
    case PassPower5::Lucky1:
        powers.append(WildSearcherModel5::tr("Lucky Power ↑"));
        break;
    case PassPower5::Lucky2:
        powers.append(WildSearcherModel5::tr("Lucky Power ↑↑"));
        break;
    case PassPower5::Lucky3:
        powers.append(WildSearcherModel5::tr("Lucky Power ↑↑↑/S"));
        break;
    default:
        break;
    }

    switch (PassPower5::getEncounterPower(power))
    {
    case 1:
        powers.append(WildSearcherModel5::tr("Encounter Power ↑"));
        break;
    case 2:
        powers.append(WildSearcherModel5::tr("Encounter Power ↑↑"));
        break;
    case 3:
        powers.append(WildSearcherModel5::tr("Encounter Power ↑↑↑"));
        break;
    default:
        break;
    }

    if (powers.empty())
    {
        return WildSearcherModel5::tr("None");
    }

    return powers.join(QStringLiteral(" & "));
}

int mapGeneratorColumn(int column, bool showMovingTrigger, bool showPhenomenon)
{
    if (!showMovingTrigger && column >= generatorStepsColumn)
    {
        column++;
    }

    if (!showPhenomenon && column >= generatorPhenomenonColumn)
    {
        column++;
    }

    return column;
}

int mapSearcherColumn(int column, bool showPassPower, bool showMovingTrigger, bool showPhenomenon)
{
    if (showPassPower && column > searcherPassPowerColumn)
    {
        column--;
    }

    if (showMovingTrigger && column >= searcherDataTriggerColumn)
    {
        column++;
    }
    else if (!showMovingTrigger && column >= searcherDataTriggerColumn)
    {
        column += 2;
    }

    if (!showPhenomenon && column >= searcherDataPhenomenonColumn)
    {
        column++;
    }

    return column;
}

int mapSearcherHeaderColumn(int column, bool showPassPower, bool showMovingTrigger, bool showPhenomenon)
{
    if (!showPassPower && column >= searcherPassPowerColumn)
    {
        column++;
    }

    if (showMovingTrigger && column >= searcherHeaderTriggerColumn)
    {
        column++;
    }
    else if (!showMovingTrigger && column >= searcherHeaderTriggerColumn)
    {
        column += 2;
    }

    if (!showPhenomenon && column >= searcherHeaderPhenomenonColumn)
    {
        column++;
    }

    return column;
}
}

WildGeneratorModel5::WildGeneratorModel5(QObject *parent) : TableModel(parent), showStats(false), showMovingTrigger(false), showPhenomenon(false)
{
}

int WildGeneratorModel5::columnCount(const QModelIndex &parent) const
{
    return header.size() - (showMovingTrigger ? 0 : 1) - (showPhenomenon ? 0 : 1);
}

QVariant WildGeneratorModel5::data(const QModelIndex &index, int role) const
{
    const auto &state = model[index.row()];
    if (!state.isValid())
    {
        if (role == Qt::FontRole && index.column() != 2)
        {
            QFont font;
            font.setItalic(true);
            return font;
        }
        else if (role == Qt::ForegroundRole)
        {
            return QColor(128, 128, 128);
        }
    }

    if (role == Qt::DisplayRole)
    {
        int column = mapGeneratorColumn(index.column(), showMovingTrigger, showPhenomenon);
        bool item = state.getPhenomenonItem();

        if (!state.isValid() && column > 2)
        {
            return "-";
        }

        switch (column)
        {
        case 0:
            return state.getAdvances();
        case 1:
            return QString::fromStdString(Utilities5::getChatot(state.getChatot()));
        case 2:
            return QString::fromStdString(Translator::getNeedle(state.getNeedle()));
        case 3:
            if (state.getMovingSteps() == 255)
            {
                return "-";
            }
            return state.getMovingSteps();
        case 4:
            return state.getPhenomenon() ? tr("Yes") : tr("No");
        case 5:
            return QString::fromStdString(Translator::getItem(state.getItem()));
        case 6:
            if (item)
            {
                return "-";
            }
            return QString("%1: %2")
                .arg(state.getEncounterSlot() == 12 ? tr("S") : QString::number(state.getEncounterSlot()))
                .arg(QString::fromStdString(Translator::getSpecie(state.getSpecie(), state.getForm())));
        case 7:
            if (item)
            {
                return "-";
            }
            return state.getLevel();
        case 8:
            if (item)
            {
                return "-";
            }
            return QString::number(state.getPID(), 16).toUpper().rightJustified(8, '0');
        case 9:
        {
            if (item)
            {
                return "-";
            }
            u8 shiny = state.getShiny();
            return shiny == 2 ? tr("Square") : shiny == 1 ? tr("Star") : tr("No");
        }
        case 10:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getNature(state.getNature()));
        case 11:
            if (item)
            {
                return "-";
            }
            if (state.getAbility() == 0 || state.getAbility() == 1)
            {
                return QString("%1: %2")
                    .arg(state.getAbility())
                    .arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
            }
            else
            {
                return QString("H (%2)").arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
            }
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
            if (item)
            {
                return "-";
            }
            return showStats ? state.getStat(column - 12) : state.getIV(column - 12);
        case 18:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getHiddenPower(state.getHiddenPower()));
        case 19:
            if (item)
            {
                return "-";
            }
            return state.getHiddenPowerStrength();
        case 20:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getGender(state.getGender()));
        case 21:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getCharacteristic(state.getCharacteristic(), CharacteristicGeneration::Gen5));
        }
    }

    return QVariant();
}

QVariant WildGeneratorModel5::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        return header[mapGeneratorColumn(section, showMovingTrigger, showPhenomenon)];
    }
    return QVariant();
}

void WildGeneratorModel5::setShowStats(bool flag)
{
    showStats = flag;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), { Qt::DisplayRole });
}

void WildGeneratorModel5::setShowMovingTrigger(bool flag)
{
    if (showMovingTrigger != flag)
    {
        beginResetModel();
        showMovingTrigger = flag;
        endResetModel();
    }
}

WildSearcherModel5::WildSearcherModel5(QObject *parent) :
    TableModel(parent), showStats(false), showMovingTrigger(false), showPhenomenon(false), showPassPower(false)
{
}

int WildSearcherModel5::columnCount(const QModelIndex &parent) const
{
    return header.size() - (showPassPower ? 0 : 1) - (showMovingTrigger ? 1 : 2) - (showPhenomenon ? 0 : 1) + 1;
}

QVariant WildSearcherModel5::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        const auto &display = model[index.row()];
        const auto &state = display.getState();
        if (index.column() == 1)
        {
            return getLeadName(state.getLead(), state.getLeadFlags());
        }

        int column = index.column() > 1 ? index.column() - 1 : index.column();
        if (showPassPower && column == searcherPassPowerColumn)
        {
            return getLuckyPowerText(state.getPassPower());
        }

        column = mapSearcherColumn(column, showPassPower, showMovingTrigger, showPhenomenon);
        bool item = state.getPhenomenonItem();

        switch (column)
        {
        case 0:
            return QString::number(display.getInitialSeed(), 16).toUpper().rightJustified(16, '0');
        case 1:
            return state.getAdvances();
        case 2:
            return state.getMovingTrigger();
        case 3:
            if (state.getMovingSteps() == 255)
            {
                return "-";
            }
            return state.getMovingSteps();
        case 4:
            return state.getPhenomenon() ? tr("Yes") : tr("No");
        case 5:
            return state.getIVAdvances();
        case 6:
            return QString::fromStdString(Translator::getItem(state.getItem()));
        case 7:
            if (item)
            {
                return "-";
            }
            return QString("%1: %2")
                .arg(state.getEncounterSlot() == 12 ? tr("S") : QString::number(state.getEncounterSlot()))
                .arg(QString::fromStdString(Translator::getSpecie(state.getSpecie(), state.getForm())));
        case 8:
            if (item)
            {
                return "-";
            }
            return state.getLevel();
        case 9:
            if (item)
            {
                return "-";
            }
            return QString::number(state.getPID(), 16).toUpper().rightJustified(8, '0');
        case 10:
        {
            if (item)
            {
                return "-";
            }
            u8 shiny = state.getShiny();
            return shiny == 2 ? tr("Square") : shiny == 1 ? tr("Star") : tr("No");
        }
        case 11:
            if (item)
            {
                return "-";
            }
            if (state.getVariableNature())
            {
                return tr("Sync");
            }
            return QString::fromStdString(Translator::getNature(state.getNature()));
        case 12:
            if (item)
            {
                return "-";
            }
            if (state.getAbility() == 0 || state.getAbility() == 1)
            {
                return QString("%1: %2")
                    .arg(state.getAbility())
                    .arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
            }
            else
            {
                return QString("H (%2)").arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
            }
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
            if (item)
            {
                return "-";
            }
            return showStats ? state.getStat(column - 13) : state.getIV(column - 13);
        case 19:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getHiddenPower(state.getHiddenPower()));
        case 20:
            if (item)
            {
                return "-";
            }
            return state.getHiddenPowerStrength();
        case 21:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getGender(state.getGender()));
        case 22:
            if (item)
            {
                return "-";
            }
            return QString::fromStdString(Translator::getCharacteristic(state.getCharacteristic(), CharacteristicGeneration::Gen5));
        case 23:
            return QString::fromStdString(display.getDateTime().toString());
        case 24:
            return QString::number(display.getTimer0(), 16).toUpper();
        case 25:
            return QString::fromStdString(Translator::getKeypresses(display.getButtons()));
        }
    }

    return QVariant();
}

QVariant WildSearcherModel5::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        if (section == 1)
        {
            return tr("Lead");
        }
        return header[mapSearcherHeaderColumn(section > 1 ? section - 1 : section, showPassPower, showMovingTrigger, showPhenomenon)];
    }
    return QVariant();
}

void WildSearcherModel5::setShowStats(bool flag)
{
    showStats = flag;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), { Qt::DisplayRole });
}

void WildSearcherModel5::setShowMovingTrigger(bool flag)
{
    if (showMovingTrigger != flag)
    {
        beginResetModel();
        showMovingTrigger = flag;
        endResetModel();
    }
}

void WildSearcherModel5::setShowPassPower(bool flag)
{
    if (showPassPower != flag)
    {
        beginResetModel();
        showPassPower = flag;
        endResetModel();
    }
}
