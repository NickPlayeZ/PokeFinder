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

#include "WildModel4.hpp"
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Util/Translator.hpp>
#include <Core/Util/Utilities.hpp>
#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QStringList>

static QString getSynchronizeLeadName4(Lead lead, u32 flags, u8 targetNature)
{
    constexpr u32 allNatures = (1 << 25) - 1;
    u32 nonTargetNatures = allNatures & ~(1 << targetNature);
    if ((flags & allNatures) == allNatures || (flags & nonTargetNatures) != 0)
    {
        return QString("%1: %2").arg(QCoreApplication::translate("WildSearcherModel4", "Synchronize"),
                                     QCoreApplication::translate("WildSearcherModel4", "Any Nature"));
    }

    if (lead <= Lead::SynchronizeEnd)
    {
        return QString("%1: %2")
            .arg(QCoreApplication::translate("WildSearcherModel4", "Synchronize"),
                 QString::fromStdString(Translator::getNature(toInt(lead))));
    }

    return QCoreApplication::translate("WildSearcherModel4", "Synchronize");
}

static QString getLeadName4(Lead lead, Lead synchronizeLead, u32 synchronizeFlags, u8 flags, u8 targetNature)
{
    if (lead == Lead::None)
    {
        return QCoreApplication::translate("WildSearcherModel4", "None");
    }

    if (flags != 0)
    {
        QStringList leads;
        if ((flags & (1 << 0)) != 0)
        {
            leads.emplace_back(getSynchronizeLeadName4(synchronizeLead, synchronizeFlags, targetNature));
        }
        bool cuteCharmM = (flags & (1 << 1)) != 0;
        bool cuteCharmF = (flags & (1 << 2)) != 0;
        if (cuteCharmM && cuteCharmF)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Cute Charm: ♂ or ♀ Lead"));
        }
        else if (cuteCharmM)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Cute Charm: ♂ Lead"));
        }
        else if (cuteCharmF)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Cute Charm: ♀ Lead"));
        }
        if ((flags & (1 << 3)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Magnet Pull"));
        }
        if ((flags & (1 << 4)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Static"));
        }
        if ((flags & (1 << 5)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Hustle / Pressure / Vital Spirit"));
        }
        if ((flags & (1 << 6)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Compound Eyes"));
        }
        if ((flags & (1 << 7)) != 0)
        {
            leads.emplace_back(QCoreApplication::translate("WildSearcherModel4", "Arena Trap / Illuminate / No Guard"));
        }

        return leads.join(" / ");
    }

    if (lead <= Lead::SynchronizeEnd)
    {
        return getSynchronizeLeadName4(lead, synchronizeFlags, targetNature);
    }

    switch (lead)
    {
    case Lead::CompoundEyes:
        return QCoreApplication::translate("WildSearcherModel4", "Compound Eyes");
    case Lead::CuteCharmM:
        return QCoreApplication::translate("WildSearcherModel4", "Cute Charm: ♂ Lead");
    case Lead::CuteCharmF:
        return QCoreApplication::translate("WildSearcherModel4", "Cute Charm: ♀ Lead");
    case Lead::MagnetPull:
        return QCoreApplication::translate("WildSearcherModel4", "Magnet Pull");
    case Lead::Static:
        return QCoreApplication::translate("WildSearcherModel4", "Static");
    case Lead::Pressure:
        return QCoreApplication::translate("WildSearcherModel4", "Hustle / Pressure / Vital Spirit");
    case Lead::ArenaTrap:
        return QCoreApplication::translate("WildSearcherModel4", "Arena Trap / Illuminate / No Guard");
    case Lead::SuctionCups:
        return QCoreApplication::translate("WildSearcherModel4", "Sticky Hold / Suction Cups");
    default:
        return QString();
    }
}

WildGeneratorModel4::WildGeneratorModel4(QObject *parent, Method method) : TableModel(parent), showStats(false), showStepEncounter(false), method(method)
{
}

int WildGeneratorModel4::columnCount(const QModelIndex &parent) const
{
    int extra = showStepEncounter ? 1 : 0;
    switch (method)
    {
    case Method::MethodJ:
    case Method::HoneyTree:
    case Method::PokeRadar:
        return 20 + extra;
    case Method::MethodK:
        return 21 + extra;
    default:
        return 0;
    }
}

QVariant WildGeneratorModel4::data(const QModelIndex &index, int role) const
{
    const auto &state = model[index.row()];
    if (!state.isValid())
    {
        if (role == Qt::FontRole)
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
        int column = getColumn(index.column());
        if (!state.isValid() && column > (showStepEncounter ? 4 : 3))
        {
            return "-";
        }

        switch (column)
        {
        case 0:
            return state.getAdvances();
        case 1:
            return state.getBattleAdvances();
        case 2:
            return state.getCall() == 0 ? "E" : state.getCall() == 1 ? "K" : "P";
        case 3:
            return QString::fromStdString(Utilities4::getChatot(state.getChatot()));
        case 4:
            return state.getStepEncounter() ? QVariant(static_cast<int>(state.getMovements())) : QVariant("-");
        case 5:
            return QString::fromStdString(Translator::getItem(state.getItem()));
        case 6:
            return QString("%1: %2")
                .arg(state.getEncounterSlot())
                .arg(QString::fromStdString(Translator::getSpecie(state.getSpecie(), state.getForm())));
        case 7:
            return state.getLevel();
        case 8:
            return QString::number(state.getPID(), 16).toUpper().rightJustified(8, '0');
        case 9:
        {
            u8 shiny = state.getShiny();
            return shiny == 2 ? tr("Square") : shiny == 1 ? tr("Star") : tr("No");
        }
        case 10:
            return QString::fromStdString(Translator::getNature(state.getNature()));
        case 11:
            return QString("%1: %2").arg(state.getAbility()).arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
            return showStats ? state.getStat(column - 12) : state.getIV(column - 12);
        case 18:
            return QString::fromStdString(Translator::getHiddenPower(state.getHiddenPower()));
        case 19:
            return state.getHiddenPowerStrength();
        case 20:
            return QString::fromStdString(Translator::getGender(state.getGender()));
        case 21:
            return QString::fromStdString(Translator::getCharacteristic(state.getCharacteristic(), CharacteristicGeneration::Gen4));
        }
    }
    return QVariant();
}

QVariant WildGeneratorModel4::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        section = getColumn(section);

        return header[section];
    }
    return QVariant();
}

void WildGeneratorModel4::setMethod(Method method)
{
    beginResetModel();
    this->method = method;
    endResetModel();
}

void WildGeneratorModel4::setShowStepEncounter(bool flag)
{
    beginResetModel();
    showStepEncounter = flag;
    endResetModel();
}

void WildGeneratorModel4::setShowStats(bool flag)
{
    showStats = flag;
    int hp = showStepEncounter ? (method == Method::MethodK ? 12 : 11) : 11;
    emit dataChanged(index(0, hp), index(rowCount() - 1, hp + 5), { Qt::DisplayRole });
}

int WildGeneratorModel4::getColumn(int column) const
{
    switch (method)
    {
    case Method::MethodJ:
    case Method::HoneyTree:
    case Method::PokeRadar:
        column = column > 1 ? column + 1 : column;
        break;
    case Method::MethodK:
    default:
        break;
    }

    return !showStepEncounter && column >= 4 ? column + 1 : column;
}

WildSearcherModel4::WildSearcherModel4(QObject *parent) :
    TableModel(parent),
    showStats(false),
    showStepEncounter(false),
    showStepMovement(true),
    method(Method::MethodJ)
{
}

int WildSearcherModel4::columnCount(const QModelIndex &parent) const
{
    return showStepEncounter ? showStepMovement ? 25 : 24 : 22;
}

QVariant WildSearcherModel4::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        const auto &state = model[index.row()];
        int column = index.column();
        if (!showStepEncounter && column >= 5)
        {
            column += 3;
        }
        else if (showStepEncounter && !showStepMovement && column >= 6)
        {
            column++;
        }
        switch (column)
        {
        case 0:
            return QString::number(state.getSeed(), 16).toUpper().rightJustified(8, '0');
        case 1:
            return state.getSeed() & 0xffff;
        case 2:
            return (state.getSeed() >> 16) & 0xff;
        case 3:
            return getLeadName4(state.getLead(), state.getSynchronizeLead(), state.getSynchronizeLeadFlags(), state.getLeadFlags(),
                                state.getNature());
        case 4:
            return state.getAdvances();
        case 5:
            return static_cast<int>(state.getMovements());
        case 6:
            switch (state.getMovement())
            {
            case 1:
                return tr("Running");
            case 2:
                return tr("Biking");
            case 3:
                return method == Method::MethodK ? tr("Walking in Long Grass") : tr("Walking / Running in Long Grass");
            case 4:
                return tr("Running in Long Grass");
            case 5:
                return tr("Surfing");
            case 0:
            default:
                return method == Method::MethodK ? tr("Walking") : tr("Walking / Running");
            }
        case 7:
            switch (state.getStepModifier())
            {
            case 1:
                return tr("White Flute");
            case 2:
                return tr("Pokemon March");
            case 3:
                return tr("White Flute") + QString(" & ") + tr("Pokemon March");
            case 4:
                return tr("Pokemon Lullaby");
            case 5:
                return tr("White Flute") + QString(" & ") + tr("Pokemon Lullaby");
            case 6:
                return tr("Pokemon March") + QString(" / ") + tr("Lullaby");
            case 7:
                return tr("White Flute") + QString(" & ") + tr("Pokemon March") + QString(" / ") + tr("Lullaby");
            case 0:
            default:
                return QString("-");
            }
        case 8:
            return QString::fromStdString(Translator::getItem(state.getItem()));
        case 9:
            return QString("%1: %2")
                .arg(state.getEncounterSlot())
                .arg(QString::fromStdString(Translator::getSpecie(state.getSpecie(), state.getForm())));
        case 10:
            return state.getLevel();
        case 11:
            return QString::number(state.getPID(), 16).toUpper().rightJustified(8, '0');
        case 12:
        {
            u8 shiny = state.getShiny();
            return shiny == 2 ? tr("Square") : shiny == 1 ? tr("Star") : tr("No");
        }
        case 13:
            return QString::fromStdString(Translator::getNature(state.getNature()));
        case 14:
            return QString("%1: %2").arg(state.getAbility()).arg(QString::fromStdString(Translator::getAbility(state.getAbilityIndex())));
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            return showStats ? state.getStat(column - 15) : state.getIV(column - 15);
        case 21:
            return QString::fromStdString(Translator::getHiddenPower(state.getHiddenPower()));
        case 22:
            return state.getHiddenPowerStrength();
        case 23:
            return QString::fromStdString(Translator::getGender(state.getGender()));
        case 24:
            return QString::fromStdString(Translator::getCharacteristic(state.getCharacteristic(), CharacteristicGeneration::Gen4));
        }
    }
    return QVariant();
}

QVariant WildSearcherModel4::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        if (!showStepEncounter && section >= 5)
        {
            section += 3;
        }
        else if (showStepEncounter && !showStepMovement && section >= 6)
        {
            section++;
        }
        return header[section];
    }
    return QVariant();
}

void WildSearcherModel4::setMethod(Method method)
{
    beginResetModel();
    this->method = method;
    endResetModel();
}

void WildSearcherModel4::setShowStats(bool flag)
{
    showStats = flag;
    int hp = showStepEncounter ? showStepMovement ? 15 : 14 : 12;
    emit dataChanged(index(0, hp), index(rowCount() - 1, hp + 5), { Qt::DisplayRole });
}

void WildSearcherModel4::setShowStepEncounter(bool flag)
{
    beginResetModel();
    showStepEncounter = flag;
    endResetModel();
}

void WildSearcherModel4::setShowStepMovement(bool flag)
{
    beginResetModel();
    showStepMovement = flag;
    endResetModel();
}
