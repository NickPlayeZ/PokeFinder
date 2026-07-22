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

#include "WildGenerator4.hpp"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Gen4/States/WildState4.hpp>
#include <Core/Parents/PersonalInfo.hpp>
#include <Core/Parents/Slot.hpp>
#include <Core/RNG/LCRNG.hpp>
#include <Core/Util/EncounterSlot.hpp>
#include <Core/Util/Utilities.hpp>
#include <algorithm>
#include <array>

static u32 getBattleAdvances(const EncounterArea4 &area, Game version)
{
    u32 advances = 0;

    // Fishing uses an advance to determine the visual frames range in which you have to press A
    if (area.getEncounter() == Encounter::OldRod || area.getEncounter() == Encounter::GoodRod || area.getEncounter() == Encounter::SuperRod)
    {
        advances += 1;
    }

    // DP uses 4 advances to create the information that determine if quick claw will proc
    if ((version & Game::DP) != Game::None)
    {
        advances += 4;
    }

    // Advance used to determine the random ball position when thrown out
    if (!area.greatMarsh(version) && !area.safariZone(version))
    {
        advances += 1;
    }

    return advances;
}

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

static bool isStepModifier(Lead lead)
{
    return lead == Lead::ArenaTrap;
}

static bool getStepEncounter(u8 movementRatio, u8 encounterRatio, u16 encounterRate, Lead lead, bool whiteFlute, bool fastMovement)
{
    u16 movementRate = fastMovement ? 70 : 40;

    if (isStepModifier(lead))
    {
        encounterRate *= 2;
    }
    if (whiteFlute)
    {
        encounterRate = (encounterRate * 3) / 2;
    }

    return movementRatio < movementRate && encounterRatio < encounterRate;
}

static u8 getStepMovements(u16 encounterRate, Lead lead, bool whiteFlute)
{
    if (isStepModifier(lead))
    {
        encounterRate *= 2;
    }
    if (whiteFlute)
    {
        encounterRate = (encounterRate * 3) / 2;
    }

    u8 rate = encounterRate / 10;
    if (rate > 8)
    {
        rate = 8;
    }

    return 8 - rate;
}

static u16 modifyHGSSStepEncounterRate(u16 encounterRate, Lead lead, bool whiteFlute)
{
    if (isStepModifier(lead))
    {
        encounterRate *= 2;
    }
    if (whiteFlute)
    {
        encounterRate += encounterRate / 2;
    }

    return std::min<u16>(encounterRate, 100);
}

static u16 getHGSSMovementRate(Encounter encounter, u8 movement, u8 radio)
{
    u16 movementRate;
    switch (movement)
    {
    case 1: // Running
        movementRate = 40;
        break;
    case 2: // Biking
        movementRate = 70;
        break;
    case 3: // Walking long grass
        movementRate = 60;
        break;
    case 4: // Running long grass
        movementRate = 80;
        break;
    case 5: // Surfing
        movementRate = 40;
        break;
    case 0:
    default:
        movementRate = encounter == Encounter::Surfing ? 40 : 20;
        break;
    }

    if (radio == 4)
    {
        movementRate += 25;
    }
    else if (radio == 5)
    {
        movementRate -= 25;
    }

    return std::min<u16>(movementRate, 100);
}

static u8 getHGSSStepMovements(u8 movementRatio, u8 encounterRatio, u16 encounterRate, Encounter encounter, Lead lead, bool whiteFlute,
                               u8 movement, u8 radio)
{
    encounterRate = modifyHGSSStepEncounterRate(encounterRate, lead, whiteFlute);
    if (encounterRatio >= encounterRate)
    {
        return 0xff;
    }

    u16 movementRate = getHGSSMovementRate(encounter, movement, radio);
    if (movementRatio < movementRate)
    {
        return 3;
    }
    if (movementRatio < std::min<u16>(movementRate + 30, 100))
    {
        return 4;
    }
    if (movementRatio < std::min<u16>(movementRate + 40, 100))
    {
        return 5;
    }
    if (movementRatio < std::min<u16>(movementRate + 60, 100))
    {
        return 6;
    }

    return 0xff;
}

struct RockSmashItemData
{
    constexpr RockSmashItemData(u8 odds, u8 type) : odds(odds), type(type)
    {
    }

    u8 odds;
    u8 type;
};

static RockSmashItemData getRockSmashItemData(u8 location)
{
    switch (location)
    {
    case 5: // Violet City
    case 69: // Dark Cave (Route 31)
        return { 50, 0 };
    case 9: // Ruins of Alph (Outside)
        return { 30, 1 };
    case 51: // Cianwood City
    case 63: // Ice Path B3F
    case 93: // Route 19
    case 140: // Cerulean Cave 2F
        return { 20, 0 };
    case 80: // Mt. Silver Cave 2F
    case 98: // Vermilion City
    case 139: // Cerulean Cave 1F
        return { 25, 0 };
    case 83: // Cliff Cave
        return { 25, 2 };
    case 109: // Rock Tunnel B1F
    case 141: // Cerulean Cave B1F
        return { 30, 0 };
    case 113: // Route 3
    case 135: // Victory Road 3F
        return { 10, 0 };
    default:
        return { 0, 0 };
    }
}

static u8 getRockSmashItemIndex(u8 rand)
{
    constexpr std::array<u8, 8> ranges = { 25, 45, 55, 65, 75, 85, 95, 100 };
    return std::upper_bound(ranges.begin(), ranges.end(), rand) - ranges.begin();
}

static bool increasesRockSmashItemRate(Lead lead)
{
    return lead == Lead::RockSmashMagnetPull || lead == Lead::RockSmashSuctionCups || lead == Lead::KeenEye;
}

static bool improvesRockSmashItem(Lead lead)
{
    return lead == Lead::RockSmashSuperLuck || lead == Lead::SereneGrace;
}

static bool isRockSmashItemLead(Lead lead)
{
    return increasesRockSmashItemRate(lead) || improvesRockSmashItem(lead) || lead == Lead::Intimidate;
}

static bool isRockSmashEncounterSuppressed(Lead lead, u8 leadLevel, u8 level, PokeRNG &rng, u32 *battleAdvances)
{
    return (lead == Lead::KeenEye || lead == Lead::Intimidate) && leadLevel > 5 && level <= leadLevel - 5
        && rng.nextUShort(2, battleAdvances) == 0;
}

static u16 getRockSmashItem(u8 location, Game version, Lead lead, bool rockSmashPokemon, PokeRNG &rng, u32 *battleAdvances)
{
    RockSmashItemData data = getRockSmashItemData(location);
    if (data.odds == 0)
    {
        return 0;
    }

    if (increasesRockSmashItemRate(lead))
    {
        data.odds += 5;
    }
    if (rockSmashPokemon)
    {
        data.odds += 5;
    }
    data.odds = std::min<u8>(data.odds, 100);

    if (rng.nextUShort(100, battleAdvances) >= data.odds)
    {
        return 0;
    }

    u8 index = getRockSmashItemIndex(rng.nextUShort(100, battleAdvances));
    if (improvesRockSmashItem(lead) && index < 7)
    {
        index++;
    }

    constexpr std::array<u16, 8> defaultItems = { 39, 28, 93, 72, 73, 75, 74, 91 };
    constexpr std::array<u16, 8> ruinsHG = { 72, 74, 101, 39, 73, 75, 103, 29 };
    constexpr std::array<u16, 8> ruinsSS = { 73, 75, 102, 39, 72, 74, 103, 29 };
    constexpr std::array<u16, 8> cliffHG = { 39, 88, 89, 72, 74, 100, 100, 106 };
    constexpr std::array<u16, 8> cliffSS = { 39, 88, 89, 73, 75, 99, 99, 106 };

    switch (data.type)
    {
    case 1:
    {
        const auto &items = (version & Game::HeartGold) != Game::None ? ruinsHG : ruinsSS;
        return items[index];
    }
    case 2:
    {
        const auto &items = (version & Game::HeartGold) != Game::None ? cliffHG : cliffSS;
        return items[index];
    }
    default:
        return defaultItems[index];
    }
}

WildGenerator4::WildGenerator4(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, bool feebasTile, bool shiny,
                               bool unownRadio, u8 happiness, bool searchStepEncounter, bool whiteFlute, bool fastMovement, u8 movement,
                               u8 radio, bool rockSmashPokemon, u8 leadLevel, const EncounterArea4 &area, const Profile4 &profile,
                               const WildStateFilter &filter) :
    WildGenerator(initialAdvances, maxAdvances, offset, method, lead, area, profile, filter),
    feebasTile(feebasTile),
    shiny(shiny),
    unownRadio(unownRadio),
    searchStepEncounter(searchStepEncounter),
    whiteFlute(whiteFlute),
    fastMovement(fastMovement),
    rockSmashPokemon(rockSmashPokemon),
    movement(movement),
    radio(radio),
    happiness(happiness),
    leadLevel(leadLevel)
{
}

WildGenerator4::WildGenerator4(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, bool feebasTile, bool shiny,
                               bool unownRadio, u8 happiness, const EncounterArea4 &area, const Profile4 &profile,
                               const WildStateFilter &filter) :
    WildGenerator4(initialAdvances, maxAdvances, offset, method, lead, feebasTile, shiny, unownRadio, happiness, false, false, false, 0, 0,
                   false, 0, area, profile, filter)
{
}

std::vector<WildGeneratorState4> WildGenerator4::generate(u32 seed, u8 index) const
{
    switch (method)
    {
    case Method::MethodJ:
        return generateMethodJ(seed);
    case Method::MethodK:
        return generateMethodK(seed);
    case Method::HoneyTree:
        return generateHoneyTree(seed, index);
    case Method::PokeRadar:
        if (shiny)
        {
            return generatePokeRadarShiny(seed, index);
        }
        else
        {
            return generatePokeRadar(seed, index);
        }
    default:
        return std::vector<WildGeneratorState4>();
    }
}

std::vector<WildGeneratorState4> WildGenerator4::generateMethodJ(u32 seed) const
{
    std::vector<WildGeneratorState4> states;

    u8 thresh = area.getRate();
    auto modifiedSlots = area.getSlots(lead);
    bool feebas = area.feebasLocation(profile.getVersion()) && feebasTile;

    PokeRNG rng(seed, initialAdvances);

    u32 battleAdvancesConst = getBattleAdvances(area, profile.getVersion());
    u8 movements = searchStepEncounter ? getStepMovements(area.getRate(), lead, whiteFlute) : 0;

    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 targetAdvance = initialAdvances + offset + cnt;
        u32 payloadAdvance = targetAdvance + (searchStepEncounter ? 2 : 0);
        u32 battleAdvances = battleAdvancesConst + payloadAdvance;
        PokeRNG go(seed, payloadAdvance);
        bool valid = true;
        bool stepEncounter = false;
        if (searchStepEncounter)
        {
            PokeRNG movementRNG(seed, targetAdvance);
            PokeRNG encounterRNG(seed, targetAdvance + 1);
            u8 movementRatio = movementRNG.nextUShort() / 0x290;
            u8 encounterRatio = encounterRNG.nextUShort() / 0x290;
            stepEncounter = getStepEncounter(movementRatio, encounterRatio, area.getRate(), lead, whiteFlute, fastMovement);
            valid = stepEncounter;
        }

        // Fishing nibble check
        if ((area.getEncounter() == Encounter::OldRod || area.getEncounter() == Encounter::GoodRod
             || area.getEncounter() == Encounter::SuperRod)
            && go.nextUShort<false>(100, &battleAdvances) >= thresh)
        {
            valid = false;
        }

        u8 encounterSlot;
        if (feebas && go.nextUShort<false>(2, &battleAdvances))
        {
            encounterSlot = 5;
            go.advance((lead == Lead::MagnetPull || lead == Lead::Static) ? 2 : 1, &battleAdvances);
        }
        else
        {
            if ((lead == Lead::MagnetPull || lead == Lead::Static) && go.nextUShort<false>(2, &battleAdvances) == 0
                && !modifiedSlots.empty())
            {
                encounterSlot = modifiedSlots[go.nextUShort(modifiedSlots.count, &battleAdvances)];
            }
            else
            {
                encounterSlot = EncounterSlot::jSlot(go.nextUShort<false>(100, &battleAdvances), area.getEncounter());
            }
        }

        if (valid && !filter.compareEncounterSlot(encounterSlot))
        {
            rng.next();
            continue;
        }

        u8 level;
        if (area.getEncounter() == Encounter::Grass)
        {
            level = area.calculateLevel<false, false>(encounterSlot, go, &battleAdvances, lead == Lead::Pressure);
        }
        else
        {
            level = area.calculateLevel<true, false>(encounterSlot, go, &battleAdvances, lead == Lead::Pressure);
        }

        const Slot &slot = area.getPokemon(encounterSlot);
        const PersonalInfo *info = slot.getInfo();

        bool cuteCharmFlag = false;
        if ((lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && !info->getFixedGender())
        {
            cuteCharmFlag = go.nextUShort<false>(3, &battleAdvances) != 0;
        }

        u8 nature;
        if (lead <= Lead::SynchronizeEnd)
        {
            nature = go.nextUShort<false>(2, &battleAdvances) == 0 ? toInt(lead) : go.nextUShort<false>(25, &battleAdvances);
        }
        else
        {
            nature = go.nextUShort<false>(25, &battleAdvances);
        }

        if (valid && !filter.compareNature(nature))
        {
            rng.next();
            continue;
        }

        u32 pid;
        if (cuteCharmFlag)
        {
            u8 buffer = 0;
            if (lead == Lead::CuteCharmF)
            {
                buffer = 25 * ((info->getGender() / 25) + 1);
            }

            pid = buffer + nature;
        }
        else
        {
            do
            {
                u16 low = go.nextUShort(&battleAdvances);
                u16 high = go.nextUShort(&battleAdvances);
                pid = (high << 16) | low;
            } while (pid % 25 != nature);
        }

        u16 iv1 = go.nextUShort(&battleAdvances);
        u16 iv2 = go.nextUShort(&battleAdvances);
        std::array<u8, 6> ivs;
        ivs[0] = iv1 & 31;
        ivs[1] = (iv1 >> 5) & 31;
        ivs[2] = (iv1 >> 10) & 31;
        ivs[3] = (iv2 >> 5) & 31;
        ivs[4] = (iv2 >> 10) & 31;
        ivs[5] = iv2 & 31;

        u16 item = getItem(go.nextUShort(100, &battleAdvances), lead, info);

        u8 form = slot.getForm();
        if (slot.getSpecie() == 201)
        {
            form = area.unownForm(go.nextUShort(&battleAdvances));
        }

        WildGeneratorState4 state(rng.nextUShort(), battleAdvances, initialAdvances + cnt, pid, ivs, pid & 1,
                                  Utilities::getGender(pid, info), level, nature, Utilities::getShiny<true>(pid, tsv), encounterSlot, item,
                                  slot.getSpecie(), form, info, valid, stepEncounter, movements);
        if (!valid || filter.compareState(static_cast<const WildGeneratorState &>(state)))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generateMethodK(u32 seed) const
{
    std::vector<WildGeneratorState4> states;

    Lead encounterLead = area.getEncounter() == Encounter::RockSmash && isRockSmashItemLead(lead) ? Lead::None : lead;

    u16 rate = area.getRate();
    if (area.getEncounter() == Encounter::OldRod || area.getEncounter() == Encounter::GoodRod || area.getEncounter() == Encounter::SuperRod)
    {
        rate += happiness;
        if (encounterLead == Lead::SuctionCups)
        {
            rate *= 2;
        }
    }
    else if (encounterLead == Lead::ArenaTrap && area.getEncounter() == Encounter::RockSmash)
    {
        rate *= 2;
    }
    auto modifiedSlots = area.getSlots(encounterLead);
    bool safari = area.safariZone(profile.getVersion());

    auto unlockedUnown = profile.getUnlockedUnownForms();
    auto undiscoveredUnown = profile.getUndiscoveredUnownForms(unlockedUnown);

    PokeRNG rng(seed, initialAdvances);
    auto jump = rng.getJump(offset);

    u32 battleAdvancesConst = getBattleAdvances(area, profile.getVersion());
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 battleAdvances = battleAdvancesConst + initialAdvances + offset + cnt;
        PokeRNG go(rng, jump);
        bool valid = true;
        bool stepEncounter = false;
        u8 movements = 0;
        u8 movementRatio = 0;
        u8 encounterRatio = 0;

        if (searchStepEncounter)
        {
            movementRatio = go.nextUShort(100, &battleAdvances);
            encounterRatio = go.nextUShort(100, &battleAdvances);
            movements
                = getHGSSStepMovements(movementRatio, encounterRatio, area.getRate(), area.getEncounter(), encounterLead, whiteFlute, movement, radio);
            stepEncounter = movements != 0xff;
            valid = stepEncounter;
        }

        // Rock smash/fishing nibble check
        if ((area.getEncounter() == Encounter::RockSmash || area.getEncounter() == Encounter::OldRod
             || area.getEncounter() == Encounter::GoodRod || area.getEncounter() == Encounter::SuperRod)
            && go.nextUShort(100, &battleAdvances) >= rate)
        {
            if (area.getEncounter() == Encounter::RockSmash)
            {
                u16 item = getRockSmashItem(area.getLocation(), profile.getVersion(), lead, rockSmashPokemon, go, &battleAdvances);
                if (item != 0)
                {
                    const Slot &slot = area.getPokemon(0);
                    std::array<u8, 6> ivs = {};
                    states.emplace_back(rng.nextUShort(), battleAdvances, initialAdvances + cnt, 0, ivs, 0, 0, 0, 0, 0, 0, item, 0, 0,
                                        slot.getInfo(), true, stepEncounter, stepEncounter ? movements : 0, movementRatio, encounterRatio);
                    continue;
                }
            }

            valid = false;
        }

        u8 encounterSlot;
        if ((encounterLead == Lead::MagnetPull || encounterLead == Lead::Static) && go.nextUShort(2, &battleAdvances) == 0
            && !modifiedSlots.empty())
        {
            encounterSlot = modifiedSlots[go.nextUShort(modifiedSlots.count, &battleAdvances)];
        }
        else
        {
            if (safari)
            {
                encounterSlot = go.nextUShort(10, &battleAdvances);
            }
            else
            {
                encounterSlot = EncounterSlot::kSlot(go.nextUShort(100, &battleAdvances), area.getEncounter());
            }
        }

        if (valid && !filter.compareEncounterSlot(encounterSlot))
        {
            rng.next();
            continue;
        }

        u8 level;
        if (area.getEncounter() == Encounter::Grass || safari)
        {
            level = area.calculateLevel<false, true>(encounterSlot, go, &battleAdvances, encounterLead == Lead::Pressure);
        }
        else
        {
            level = area.calculateLevel<true, true>(encounterSlot, go, &battleAdvances, encounterLead == Lead::Pressure);
        }

        const Slot &slot = area.getPokemon(encounterSlot);
        const PersonalInfo *info = slot.getInfo();

        if (area.getEncounter() == Encounter::RockSmash && isRockSmashEncounterSuppressed(lead, leadLevel, level, go, &battleAdvances))
        {
            u16 item = getRockSmashItem(area.getLocation(), profile.getVersion(), lead, rockSmashPokemon, go, &battleAdvances);
            if (item != 0)
            {
                const Slot &itemSlot = area.getPokemon(0);
                std::array<u8, 6> ivs = {};
                states.emplace_back(rng.nextUShort(), battleAdvances, initialAdvances + cnt, 0, ivs, 0, 0, 0, 0, 0, 0, item, 0, 0,
                                    itemSlot.getInfo(), true, stepEncounter, stepEncounter ? movements : 0, movementRatio, encounterRatio);
            }

            rng.next();
            continue;
        }

        bool cuteCharmFlag = false;
        if ((encounterLead == Lead::CuteCharmF || encounterLead == Lead::CuteCharmM) && !info->getFixedGender())
        {
            cuteCharmFlag = go.nextUShort(3, &battleAdvances) != 0;
        }

        u8 nature;
        u32 pid;
        u16 iv1;
        u16 iv2;
        if (cuteCharmFlag)
        {
            nature = go.nextUShort(25, &battleAdvances);
            if (valid && !filter.compareNature(nature))
            {
                rng.next();
                continue;
            }

            u8 buffer = 0;
            if (encounterLead == Lead::CuteCharmF)
            {
                buffer = 25 * ((info->getGender() / 25) + 1);
            }

            pid = buffer + nature;
            iv1 = go.nextUShort(&battleAdvances);
            iv2 = go.nextUShort(&battleAdvances);
        }
        else
        {
            if (area.getEncounter() == Encounter::BugCatchingContest || safari)
            {
                for (u8 i = 0; i < 4; i++)
                {
                    if (encounterLead <= Lead::SynchronizeEnd)
                    {
                        nature = go.nextUShort(2, &battleAdvances) == 0 ? toInt(encounterLead) : go.nextUShort(25, &battleAdvances);
                    }
                    else
                    {
                        nature = go.nextUShort(25, &battleAdvances);
                    }

                    do
                    {
                        u16 low = go.nextUShort(&battleAdvances);
                        u16 high = go.nextUShort(&battleAdvances);
                        pid = (high << 16) | low;
                    } while (pid % 25 != nature);

                    iv1 = go.nextUShort(&battleAdvances);
                    iv2 = go.nextUShort(&battleAdvances);

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

                if (valid && !filter.compareNature(nature))
                {
                    rng.next();
                    continue;
                }
            }
            else
            {
                if (encounterLead <= Lead::SynchronizeEnd)
                {
                    nature = go.nextUShort(2, &battleAdvances) == 0 ? toInt(encounterLead) : go.nextUShort(25, &battleAdvances);
                }
                else
                {
                    nature = go.nextUShort(25, &battleAdvances);
                }

                if (valid && !filter.compareNature(nature))
                {
                    rng.next();
                    continue;
                }

                do
                {
                    u16 low = go.nextUShort(&battleAdvances);
                    u16 high = go.nextUShort(&battleAdvances);
                    pid = (high << 16) | low;
                } while (pid % 25 != nature);

                iv1 = go.nextUShort(&battleAdvances);
                iv2 = go.nextUShort(&battleAdvances);
            }
        }

        std::array<u8, 6> ivs;
        ivs[0] = iv1 & 31;
        ivs[1] = (iv1 >> 5) & 31;
        ivs[2] = (iv1 >> 10) & 31;
        ivs[3] = (iv2 >> 5) & 31;
        ivs[4] = (iv2 >> 10) & 31;
        ivs[5] = iv2 & 31;

        u16 item = getItem(go.nextUShort(100, &battleAdvances), encounterLead, info);

        u8 form = 0;
        if (slot.getSpecie() == 201 && !unlockedUnown.empty())
        {
            if (area.getLocation() == 10)
            {
                form = 26 + go.nextUShort(2, &battleAdvances);
            }
            else if (area.getLocation() == 11)
            {
                if (unownRadio && !undiscoveredUnown.empty() && go.nextUShort(100, &battleAdvances) < 50)
                {
                    form = undiscoveredUnown[go.nextUShort(undiscoveredUnown.size(), &battleAdvances)];
                }
                else
                {
                    form = unlockedUnown[go.nextUShort(unlockedUnown.size(), &battleAdvances)];
                }
            }
        }

        WildGeneratorState4 state(rng.nextUShort(), battleAdvances, initialAdvances + cnt, pid, ivs, pid & 1,
                                  Utilities::getGender(pid, info), level, nature, Utilities::getShiny<true>(pid, tsv), encounterSlot, item,
                                  slot.getSpecie(), form, info, valid, stepEncounter, stepEncounter ? movements : 0, movementRatio, encounterRatio);
        if (!valid || filter.compareState(static_cast<const WildGeneratorState &>(state)))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generateHoneyTree(u32 seed, u8 index) const
{
    std::vector<WildGeneratorState4> states;

    const Slot &slot = area.getPokemon(index);
    const PersonalInfo *info = slot.getInfo();

    bool cuteCharm = false;
    u8 buffer = 0;
    if ((lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && !info->getFixedGender())
    {
        cuteCharm = true;
        if (lead == Lead::CuteCharmF)
        {
            buffer = 25 * ((info->getGender() / 25) + 1);
        }
    }

    PokeRNG rng(seed, initialAdvances);
    auto jump = rng.getJump(offset);

    u32 battleAdvancesConst = getBattleAdvances(area, profile.getVersion());

    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 battleAdvances = battleAdvancesConst + initialAdvances + offset + cnt;
        PokeRNG go(rng, jump);

        u8 level = area.calculateLevel<true, false, true>(index, go, &battleAdvances, lead == Lead::Pressure);

        u8 nature;
        u32 pid;

        bool cuteCharmFlag = false;
        if (cuteCharm)
        {
            cuteCharmFlag = go.nextUShort<false>(3, &battleAdvances) != 0;
        }

        if (lead <= Lead::SynchronizeEnd)
        {
            nature = go.nextUShort<false>(2, &battleAdvances) == 0 ? toInt(lead) : go.nextUShort<false>(25, &battleAdvances);
        }
        else
        {
            nature = go.nextUShort<false>(25, &battleAdvances);
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
                u16 low = go.nextUShort(&battleAdvances);
                u16 high = go.nextUShort(&battleAdvances);
                pid = (high << 16) | low;
            } while (pid % 25 != nature);
        }

        u16 iv1 = go.nextUShort(&battleAdvances);
        u16 iv2 = go.nextUShort(&battleAdvances);
        std::array<u8, 6> ivs;
        ivs[0] = iv1 & 31;
        ivs[1] = (iv1 >> 5) & 31;
        ivs[2] = (iv1 >> 10) & 31;
        ivs[3] = (iv2 >> 5) & 31;
        ivs[4] = (iv2 >> 10) & 31;
        ivs[5] = iv2 & 31;

        u16 item = getItem(go.nextUShort(100, &battleAdvances), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), battleAdvances, initialAdvances + cnt, pid, ivs, pid & 1,
                                  Utilities::getGender(pid, info), level, nature, Utilities::getShiny<true>(pid, tsv), index, item,
                                  slot.getSpecie(), 0, info);
        if (filter.compareState(static_cast<const WildGeneratorState &>(state)))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generatePokeRadar(u32 seed, u8 index) const
{
    std::vector<WildGeneratorState4> states;

    const Slot &slot = area.getPokemon(index);
    const PersonalInfo *info = slot.getInfo();

    u8 buffer = 0;
    bool cuteCharm = false;
    if ((lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && !info->getFixedGender())
    {
        cuteCharm = true;
        if (lead == Lead::CuteCharmF)
        {
            buffer = 25 * ((info->getGender() / 25) + 1);
        }
    }

    PokeRNG rng(seed, initialAdvances);
    auto jump = rng.getJump(offset);

    u32 battleAdvancesConst = getBattleAdvances(area, profile.getVersion());

    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 battleAdvances = battleAdvancesConst + initialAdvances + offset + cnt;
        PokeRNG go(rng, jump);

        bool cuteCharmFlag = false;
        if (cuteCharm)
        {
            cuteCharmFlag = go.nextUShort<false>(3, &battleAdvances) != 0;
        }

        u8 nature;
        if (lead <= Lead::SynchronizeEnd)
        {
            nature = go.nextUShort<false>(2, &battleAdvances) == 0 ? toInt(lead) : go.nextUShort<false>(25, &battleAdvances);
        }
        else
        {
            nature = go.nextUShort<false>(25, &battleAdvances);
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
                u16 low = go.nextUShort(&battleAdvances);
                u16 high = go.nextUShort(&battleAdvances);
                pid = (high << 16) | low;
            } while (pid % 25 != nature);
        }

        u16 iv1 = go.nextUShort(&battleAdvances);
        u16 iv2 = go.nextUShort(&battleAdvances);
        std::array<u8, 6> ivs;
        ivs[0] = iv1 & 31;
        ivs[1] = (iv1 >> 5) & 31;
        ivs[2] = (iv1 >> 10) & 31;
        ivs[3] = (iv2 >> 5) & 31;
        ivs[4] = (iv2 >> 10) & 31;
        ivs[5] = iv2 & 31;

        u16 item = getItem(go.nextUShort(100, &battleAdvances), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), battleAdvances, initialAdvances + cnt, pid, ivs, pid & 1,
                                  Utilities::getGender(pid, info), slot.getMaxLevel(), nature, Utilities::getShiny<true>(pid, tsv), index,
                                  item, slot.getSpecie(), 0, info);
        if (filter.compareState(static_cast<const WildGeneratorState &>(state)))
        {
            states.emplace_back(state);
        }
    }

    return states;
}

std::vector<WildGeneratorState4> WildGenerator4::generatePokeRadarShiny(u32 seed, u8 index) const
{
    std::vector<WildGeneratorState4> states;

    const Slot &slot = area.getPokemon(index);
    const PersonalInfo *info = slot.getInfo();

    bool cuteCharm = (lead == Lead::CuteCharmF || lead == Lead::CuteCharmM) && !info->getFixedGender();

    auto cuteCharmCheck = [this](const PersonalInfo *info, u32 pid) {
        if (lead == Lead::CuteCharmF)
        {
            return (pid & 0xff) >= info->getGender();
        }
        return (pid & 0xff) < info->getGender();
    };

    PokeRNG rng(seed, initialAdvances);
    auto jump = rng.getJump(offset);

    u32 battleAdvancesConst = getBattleAdvances(area, profile.getVersion());

    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 battleAdvances = battleAdvancesConst + initialAdvances + offset + cnt;
        PokeRNG go(rng, jump);

        auto shinyPID = [this, &go, &battleAdvances]() {
            u16 low = go.nextUShort(8, &battleAdvances);
            u16 high = go.nextUShort(8, &battleAdvances);
            for (int i = 3; i < 16; i++)
            {
                low |= (go.nextUShort(&battleAdvances) & 1) << i;
            }
            high |= (tsv ^ low) & 0xfff8;
            return static_cast<u32>((high << 16) | low);
        };

        u32 pid;
        if (cuteCharm && go.nextUShort<false>(3, &battleAdvances) != 0)
        {
            do
            {
                pid = shinyPID();
            } while (!cuteCharmCheck(info, pid));
        }
        else if (lead <= Lead::SynchronizeEnd && go.nextUShort<false>(2, &battleAdvances) == 0)
        {
            do
            {
                pid = shinyPID();
            } while (pid % 25 != toInt(lead));
        }
        else
        {
            pid = shinyPID();
        }

        u8 nature = pid % 25;
        if (!filter.compareNature(nature))
        {
            rng.next();
            continue;
        }

        u16 iv1 = go.nextUShort(&battleAdvances);
        u16 iv2 = go.nextUShort(&battleAdvances);
        std::array<u8, 6> ivs;
        ivs[0] = iv1 & 31;
        ivs[1] = (iv1 >> 5) & 31;
        ivs[2] = (iv1 >> 10) & 31;
        ivs[3] = (iv2 >> 5) & 31;
        ivs[4] = (iv2 >> 10) & 31;
        ivs[5] = iv2 & 31;

        u16 item = getItem(go.nextUShort(100, &battleAdvances), lead, info);

        WildGeneratorState4 state(rng.nextUShort(), battleAdvances, initialAdvances + cnt, pid, ivs, pid & 1,
                                  Utilities::getGender(pid, info), slot.getMaxLevel(), nature, Utilities::getShiny<true>(pid, tsv), index,
                                  item, slot.getSpecie(), 0, info);
        if (filter.compareState(static_cast<const WildGeneratorState &>(state)))
        {
            states.emplace_back(state);
        }
    }

    return states;
}
