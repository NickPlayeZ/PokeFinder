/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2022 by Admiral_Fish, bumba, and EzPzStreamz
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

#include "WildGenerator4.hpp"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen4/EncounterArea4.hpp>
#include <Core/Gen4/States/WildState4.hpp>
#include <Core/Parents/Slot.hpp>
#include <Core/RNG/LCRNG.hpp>
#include <Core/Util/EncounterSlot.hpp>

static u16 getItem(u8 rand, Lead lead, const PersonalInfo *info)
{
    constexpr u8 ItemTableRange[2][2] = { { 45, 95 }, { 20, 80 } };

    if (info->getItem(0) == info->getItem(1) && info->getItem(0) != 0)
    {
        return info->getItem(0);
    }
    else if (rand < ItemTableRange[lead == Lead::CompoundEyes ? 1 : 0][0])
    {
        return 0;
    }
    else if (rand < ItemTableRange[lead == Lead::CompoundEyes ? 1 : 0][1])
    {
        return info->getItem(0);
    }
    else
    {
        return info->getItem(1);
    }
}

WildGenerator4::WildGenerator4(u32 initialAdvances, u32 maxAdvances, u32 offset, u16 tid, u16 sid, Game version, Method method,
                               Encounter encounter, Lead lead, bool shiny, const WildStateFilter4 &filter) :
    WildGenerator<WildStateFilter4>(initialAdvances, maxAdvances, offset, tid, sid, version, method, encounter, lead, filter), shiny(shiny)
{
}

std::vector<WildGeneratorState4> WildGenerator4::generate(u32 seed, const EncounterArea4 &encounterArea, u8 index) const
{
    switch (method)
    {
    case Method::MethodJ:
        return generateMethodJ(seed, encounterArea);
    case Method::MethodK:
        return generateMethodK(seed, encounterArea);
    case Method::PokeRadar:
        return generatePokeRadar(seed, encounterArea, index);
    default:
        return std::vector<WildGeneratorState4>();
    }
}

std::vector<WildGeneratorState4> WildGenerator4::generateMethodJ(u32 seed, const EncounterArea4 &encounterArea) const
{
    std::vector<WildGeneratorState4> states;

    u8 thresh = encounterArea.getRate();
    std::vector<u8> modifiedSlots = encounterArea.getSlots(lead);

    PokeRNG rng(seed, initialAdvances + offset);
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 occidentary = initialAdvances + cnt;
        PokeRNG go(rng.getSeed());

        // Fishing nibble check
        if ((encounter == Encounter::OldRod || encounter == Encounter::GoodRod || encounter == Encounter::SuperRod)
            && go.nextUShort<false>(100, &occidentary) >= thresh)
        {
            rng.next();
            continue;
        }

        u8 encounterSlot;
        if ((lead == Lead::MagnetPull || lead == Lead::Static) && go.nextUShort<false>(2, &occidentary) && !modifiedSlots.empty())
        {
            encounterSlot = modifiedSlots[go.nextUShort(modifiedSlots.size(), &occidentary)];
        }
        else
        {
            encounterSlot = EncounterSlot::jSlot(go.nextUShort<false>(100, &occidentary), encounter);
        }

        if (!filter.compareEncounterSlot(encounterSlot))
        {
            rng.next();
            continue;
        }

        u8 level;
        if (encounter == Encounter::Grass)
        {
            level = encounterArea.calculateLevel<false, false>(encounterSlot, go, &occidentary, lead == Lead::Pressure);
        }
        else
        {
            level = encounterArea.calculateLevel<true, false>(encounterSlot, go, &occidentary, lead == Lead::Pressure);
        }

        const Slot &slot = encounterArea.getPokemon(encounterSlot);
        const PersonalInfo *info = slot.getInfo();

        bool cuteCharmFlag = false;
        u8 buffer = 0;
        if (lead == Lead::CuteCharmF || lead == Lead::CuteCharmM)
        {
            switch (info->getGender())
            {
            case 0:
            case 254:
            case 255:
                cuteCharmFlag = false;
                break;
            default:
                cuteCharmFlag = rng.nextUShort<false>(3, &occidentary) != 0;
                if (lead == Lead::CuteCharmM)
                {
                    buffer = 25 * ((info->getGender() / 25) + 1);
                }
                break;
            }
        }

        u8 nature;
        if (lead <= Lead::SynchronizeEnd)
        {
            nature = go.nextUShort<false>(2, &occidentary) == 0 ? toInt(lead) : go.nextUShort<false>(25, &occidentary);
        }
        else
        {
            nature = go.nextUShort<false>(25, &occidentary);
        }

        if (!filter.compareNature(nature))
        {
            rng.next();
            continue;
        }

        u32 pid;
        if (cuteCharmFlag)
        {
            pid = buffer + nature;
        }
        else
        {
            do
            {
                u16 low = go.nextUShort(&occidentary);
                u16 high = go.nextUShort(&occidentary);
                pid = (high << 16) | low;
            } while (pid % 25 != nature);
        }

        u16 iv1 = go.nextUShort(&occidentary);
        u16 iv2 = go.nextUShort(&occidentary);
        u16 item = getItem(go.nextUShort(100, &occidentary), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), initialAdvances + cnt, occidentary, pid, nature, iv1, iv2, tsv, level, encounterSlot,
                                  item, slot.getSpecie(), info);
        if (filter.compareState(state))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generateMethodK(u32 seed, const EncounterArea4 &encounterArea) const
{
    std::vector<WildGeneratorState4> states;

    u16 rate = encounterArea.getRate();
    if (lead == Lead::SuctionCups
        && (encounter == Encounter::OldRod || encounter == Encounter::GoodRod || encounter == Encounter::SuperRod))
    {
        rate *= 2;
    }
    else if (lead == Lead::ArenaTrap && encounter == Encounter::RockSmash)
    {
        rate *= 2;
    }
    std::vector<u8> modifiedSlots = encounterArea.getSlots(lead);

    PokeRNG rng(seed, initialAdvances + offset);
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 occidentary = initialAdvances + cnt;
        PokeRNG go(rng.getSeed());

        // Rock smash/fishing nibble check
        if ((encounter == Encounter::RockSmash || encounter == Encounter::OldRod || encounter == Encounter::GoodRod
             || encounter == Encounter::SuperRod)
            && go.nextUShort(100, &occidentary) >= rate)
        {
            rng.next();
            continue;
        }

        u8 encounterSlot;
        if ((lead == Lead::MagnetPull || lead == Lead::Static) && go.nextUShort(2, &occidentary) && !modifiedSlots.empty())
        {
            encounterSlot = modifiedSlots[go.nextUShort(modifiedSlots.size(), &occidentary)];
        }
        else
        {
            encounterSlot = EncounterSlot::kSlot(go.nextUShort(100, &occidentary), encounter);
        }

        if (!filter.compareEncounterSlot(encounterSlot))
        {
            rng.next();
            continue;
        }

        u8 level;
        if (encounter == Encounter::Grass)
        {
            level = encounterArea.calculateLevel<false, true>(encounterSlot, go, &occidentary, lead == Lead::Pressure);
        }
        else
        {
            level = encounterArea.calculateLevel<true, true>(encounterSlot, go, &occidentary, lead == Lead::Pressure);
        }

        const Slot &slot = encounterArea.getPokemon(encounterSlot);
        const PersonalInfo *info = slot.getInfo();

        bool cuteCharmFlag = false;
        u8 buffer = 0;
        if (lead == Lead::CuteCharmF || lead == Lead::CuteCharmM)
        {
            switch (info->getGender())
            {
            case 0:
            case 254:
            case 255:
                cuteCharmFlag = false;
                break;
            default:
                cuteCharmFlag = rng.nextUShort(3, &occidentary) != 0;
                if (lead == Lead::CuteCharmM)
                {
                    buffer = 25 * ((info->getGender() / 25) + 1);
                }
                break;
            }
        }

        u8 nature;
        u32 pid;
        u16 iv1;
        u16 iv2;
        if (cuteCharmFlag)
        {
            nature = go.nextUShort(25, &occidentary);
            if (!filter.compareNature(nature))
            {
                rng.next();
                continue;
            }

            pid = buffer + nature;
            iv1 = go.nextUShort(&occidentary);
            iv2 = go.nextUShort(&occidentary);
        }
        else
        {
            if (encounter == Encounter::BugCatchingContest)
            {
                for (u8 i = 0; i < 4; i++)
                {
                    if (lead <= Lead::SynchronizeEnd)
                    {
                        nature = go.nextUShort(2, &occidentary) == 0 ? toInt(lead) : go.nextUShort(25, &occidentary);
                    }
                    else
                    {
                        nature = go.nextUShort(25, &occidentary);
                    }

                    do
                    {
                        u16 low = go.nextUShort(&occidentary);
                        u16 high = go.nextUShort(&occidentary);
                        pid = (high << 16) | low;
                    } while (pid % 25 != nature);

                    iv1 = go.nextUShort(&occidentary);
                    iv2 = go.nextUShort(&occidentary);

                    bool exit = false;
                    for (int j = 0; j < 3; j++)
                    {
                        if (((iv1 >> (5 * j)) & 31) == 31)
                        {
                            exit = true;
                            break;
                        }
                        if (((iv2 >> (5 * j)) & 31) == 31)
                        {
                            exit = true;
                            break;
                        }
                    }

                    if (exit)
                    {
                        break;
                    }
                }

                if (!filter.compareNature(nature))
                {
                    rng.next();
                    continue;
                }
            }
            else
            {
                if (lead <= Lead::SynchronizeEnd)
                {
                    nature = go.nextUShort(2, &occidentary) == 0 ? toInt(lead) : go.nextUShort(25, &occidentary);
                }
                else
                {
                    nature = go.nextUShort(25, &occidentary);
                }

                if (!filter.compareNature(nature))
                {
                    rng.next();
                    continue;
                }

                do
                {
                    u16 low = go.nextUShort(&occidentary);
                    u16 high = go.nextUShort(&occidentary);
                    pid = (high << 16) | low;
                } while (pid % 25 != nature);

                iv1 = go.nextUShort(&occidentary);
                iv2 = go.nextUShort(&occidentary);
            }
        }

        u16 item = getItem(go.nextUShort(100, &occidentary), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), initialAdvances + cnt, occidentary, pid, nature, iv1, iv2, tsv, level, encounterSlot,
                                  item, slot.getSpecie(), info);
        if (filter.compareState(state))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generatePokeRadar(u32 seed, const EncounterArea4 &encounterArea, u8 index) const
{
    std::vector<WildGeneratorState4> states;

    const Slot &slot = encounterArea.getPokemon(index);
    const PersonalInfo *info = slot.getInfo();

    bool cuteCharm = false;
    u8 buffer = 0;
    switch (info->getGender())
    {
    case 0:
    case 254:
    case 255:
        break;
    default:
        cuteCharm = true;
        buffer = 25 + ((info->getGender() / 25) + 1);
        break;
    }

    auto cuteCharmCheck = [this](const PersonalInfo *info, u32 pid) {
        if (lead == Lead::CuteCharmF)
        {
            return (pid & 0xff) >= info->getGender();
        }
        return (pid & 0xff) < info->getGender();
    };

    PokeRNG rng(seed, initialAdvances + offset);
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 occidentary = initialAdvances + cnt;
        PokeRNG go(rng.getSeed());

        u8 nature;
        u32 pid;
        if (shiny)
        {
            auto shinyPID = [&go, &occidentary](u16 tsv) {
                u16 low = go.nextUShort(8, &occidentary);
                u16 high = go.nextUShort(8, &occidentary);
                for (int i = 3; i < 16; i++)
                {
                    low |= (go.nextUShort(&occidentary) & 1) << i;
                }
                high |= (tsv ^ low) & 0xfff8;
                return static_cast<u32>((high << 16) | low);
            };

            pid = shinyPID(tsv);
            if ((lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && cuteCharm && go.nextUShort<false>(3, &occidentary) != 0)
            {
                while (!cuteCharmCheck(info, pid))
                {
                    pid = shinyPID(tsv);
                }
            }
            else if (lead <= Lead::SynchronizeEnd && go.nextUShort<false>(2, &occidentary) == 0)
            {
                while (pid % 25 != toInt(lead))
                {
                    pid = shinyPID(tsv);
                }
            }

            nature = pid % 25;
            if (!filter.compareNature(nature))
            {
                rng.next();
                continue;
            }
        }
        else
        {
            bool cuteCharmFlag = false;
            if ((lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && cuteCharm)
            {
                cuteCharmFlag = go.nextUShort<false>(3, &occidentary) != 0;
            }

            if (lead <= Lead::SynchronizeEnd)
            {
                nature = go.nextUShort<false>(2, &occidentary) == 0 ? toInt(lead) : go.nextUShort<false>(25, &occidentary);
            }
            else
            {
                nature = go.nextUShort<false>(25, &occidentary);
            }

            if (!filter.compareNature(nature))
            {
                rng.next();
                continue;
            }

            if (cuteCharmFlag)
            {
                pid = buffer + nature;
            }
            else
            {
                do
                {
                    u16 low = go.nextUShort(&occidentary);
                    u16 high = go.nextUShort(&occidentary);
                    pid = (high << 16) | low;
                } while (pid % 25 != nature);
            }
        }

        u16 iv1 = go.nextUShort(&occidentary);
        u16 iv2 = go.nextUShort(&occidentary);
        u16 item = getItem(go.nextUShort(100, &occidentary), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), initialAdvances + cnt, occidentary, pid, nature, iv1, iv2, tsv, slot.getMaxLevel(),
                                  index, item, slot.getSpecie(), info);
        if (filter.compareState(state))
        {
            states.emplace_back(state);
        }
    }

    return states;
}