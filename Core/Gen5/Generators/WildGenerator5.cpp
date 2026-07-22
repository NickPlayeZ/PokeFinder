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

#include "WildGenerator5.hpp"
#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Shiny.hpp>
#include <Core/Gen5/StepEncounter.hpp>
#include <Core/Gen5/States/WildState5.hpp>
#include <Core/RNG/LCRNG64.hpp>
#include <Core/RNG/MT.hpp>
#include <Core/RNG/RNGList.hpp>
#include <Core/Util/EncounterSlot.hpp>
#include <Core/Util/Utilities.hpp>
#include <algorithm>
#include <array>
#include <iterator>

static u8 gen(MT &rng)
{
    return rng.next() >> 27;
}

static u8 getEncounterRand(BWRNG &rng, u8 max, bool bw)
{
    if (bw)
    {
        return (rng.nextUInt(0xffff) / 656) % max;
    }
    else
    {
        return rng.nextUInt(max);
    }
}

static u8 getPercentRand(BWRNG &rng, bool bw)
{
    if (bw)
    {
        return rng.nextUInt(0xffff) / 656;
    }
    else
    {
        return rng.nextUInt(100);
    }
}

static u8 getMovingTrigger(BWRNG &rng)
{
    return (rng.nextUInt() >> 16) / 656;
}

static bool isStepModifier(Lead lead)
{
    return lead == Lead::ArenaTrap;
}

static u8 getLuckyPower(u8 passPower)
{
    u8 luckyPower = PassPower5::getLuckyPower(passPower);
    return luckyPower <= PassPower5::Lucky3 ? luckyPower : PassPower5::None;
}

static u16 getEncounterPowerModifier(u8 passPower)
{
    switch (PassPower5::getEncounterPower(passPower))
    {
    case 1:
        return 150;
    case 2:
        return 200;
    case 3:
        return 300;
    default:
        return 100;
    }
}

static u16 getStepEncounterModifier(Lead lead, u8 passPower)
{
    u16 modifier = isStepModifier(lead) ? 200 : 100;
    modifier = std::min<u16>(10000, modifier * getEncounterPowerModifier(passPower) / 100);
    return modifier;
}

static u8 getMovingTrigger(BWRNG &rng, bool bw, Lead lead, Encounter encounter)
{
    if (lead != Lead::None && lead != Lead::CompoundEyes && lead != Lead::SuctionCups && !(bw && isStepModifier(lead)))
    {
        if (lead == Lead::CuteCharmM || lead == Lead::CuteCharmF)
        {
            if (getPercentRand(rng, bw) >= 67)
            {
                getPercentRand(rng, bw);
            }
        }
        else
        {
            getPercentRand(rng, bw);
        }
    }

    if (encounter == Encounter::GrassDark)
    {
        getPercentRand(rng, bw);
    }

    return getMovingTrigger(rng);
}

static u16 getItem(BWRNG &rng, bool bw, Lead lead, Encounter encounter, const PersonalInfo *info)
{
    constexpr u8 ItemTable[2][3] = { { 50, 55, 0 }, { 60, 80, 0 } };
    constexpr u8 ItemTableRare[2][3] = { { 50, 55, 56 }, { 60, 80, 85 } };

    if (info->getItem(0) == info->getItem(1))
    {
        return info->getItem(0);
    }

    const u8 *table;
    if (encounter != Encounter::GrassDark)
    {
        table = ItemTable[lead == Lead::CompoundEyes ? 1 : 0];
    }
    else
    {
        table = ItemTableRare[lead == Lead::CompoundEyes ? 1 : 0];
    }

    u8 rand = getPercentRand(rng, bw);
    for (int i = 0; i < 3; i++)
    {
        if (rand < table[i])
        {
            return info->getItem(i);
        }
    }
    return 0;
}

static bool canTriggerPhenomenon(Encounter encounter)
{
    return encounter == Encounter::GrassRustling || encounter == Encounter::DustCloud || encounter == Encounter::SurfingRippling
        || encounter == Encounter::SuperRodRippling || encounter == Encounter::FlyingShadow;
}

static bool canYieldPhenomenonItem(Encounter encounter)
{
    return encounter == Encounter::DustCloud || encounter == Encounter::FlyingShadow;
}

static u16 getPhenomenonRate(Encounter encounter)
{
    return encounter == Encounter::FlyingShadow ? 150 : 100;
}

static bool skipsLeadCheck(Encounter encounter, Lead lead)
{
    return lead == Lead::CompoundEyes || lead == Lead::SuctionCups || (canTriggerPhenomenon(encounter) && lead == Lead::ArenaTrap);
}

static bool checkFlyingShadowBattle(BWRNG &rng)
{
    return ((static_cast<u64>(rng.nextUInt()) * 1000) >> 32) < 200;
}

static bool checkFlyingShadowLead(BWRNG &rng, Lead lead)
{
    if (lead == Lead::CuteCharmM || lead == Lead::CuteCharmF)
    {
        return (((static_cast<u64>(rng.nextUInt()) * 0xffff) >> 32) / 656) < 67;
    }

    return (rng.nextUInt() >> 31) == 1;
}

static bool hasShardDustCloudItems(Game version, u8 location)
{
    if ((version & Game::BW2) == Game::None)
    {
        return false;
    }

    return location == 49 || location == 50 || location == 52 || location == 81 || location == 82 || location == 83;
}

static u16 getDustCloudItem(BWRNG &rng, Game version, u8 location)
{
    constexpr std::array<u16, 10> stones = { 80, 81, 82, 83, 84, 85, 107, 108, 109, 110 };
    constexpr std::array<u16, 17> items = { 548, 549, 550, 551, 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 562, 563, 564 };
    constexpr std::array<u16, 4> shards = { 72, 73, 74, 75 };
    constexpr u16 everstone = 229;

    if (hasShardDustCloudItems(version, location))
    {
        return shards[rng.nextUInt(4)];
    }

    u16 prng = rng.nextUInt(1000);
    if (prng < 100)
    {
        u8 index = rng.nextUInt(stones.size() * 100) / 100;
        return stones[index];
    }
    else if (prng < 950)
    {
        u8 index = rng.nextUInt(items.size() * 100) / 100;
        return items[index];
    }

    return everstone;
}

static u16 getFlyingShadowItem(BWRNG &rng)
{
    constexpr std::array<u16, 6> wings = { 565, 566, 567, 568, 569, 570 };

    u32 current = rng.getSeed() >> 32;
    if (((static_cast<u64>(current) * 1000) >> 32) > 900)
    {
        return 571;
    }

    return wings[rng.nextUInt(6)];
}

static u16 getPhenomenonItem(BWRNG &rng, Encounter encounter, Game version, u8 location)
{
    return encounter == Encounter::DustCloud ? getDustCloudItem(rng, version, location) : getFlyingShadowItem(rng);
}

static bool usesNsPokemonReleasedOffset(Encounter encounter)
{
    return encounter == Encounter::Grass || encounter == Encounter::GrassDark || encounter == Encounter::Surfing;
}

WildGenerator5::WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, u8 passPower,
                               bool searchMovingTrigger, bool requireMovingTrigger,
                               const EncounterArea5 &area, const Profile5 &profile, const WildStateFilter &filter) :
    WildGenerator5(initialAdvances, maxAdvances, offset, method, lead, std::vector<u8> { passPower }, searchMovingTrigger, requireMovingTrigger,
                   area, profile, filter)
{
}

WildGenerator5::WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, Lead lead, const std::vector<u8> &passPowers,
                               bool searchMovingTrigger, bool requireMovingTrigger, const EncounterArea5 &area, const Profile5 &profile,
                               const WildStateFilter &filter, bool requirePassPowerIVAdvance) :
    WildGenerator(initialAdvances, maxAdvances, offset, method, lead, area, profile, filter),
    passPowers(passPowers),
    searchMovingTrigger(searchMovingTrigger),
    requireMovingTrigger(requireMovingTrigger),
    requirePassPowerIVAdvance(requirePassPowerIVAdvance)
{
    if ((profile.getVersion() & Game::BW) != Game::None)
    {
        for (u8 &passPower : this->passPowers)
        {
            passPower = PassPower5::combine(PassPower5::None, PassPower5::getEncounterPower(passPower));
        }
    }

    if (!searchMovingTrigger)
    {
        for (u8 &passPower : this->passPowers)
        {
            passPower = getLuckyPower(passPower);
        }
    }

    if (this->passPowers.empty())
    {
        this->passPowers.emplace_back(PassPower5::None);
    }
    std::ranges::sort(this->passPowers);
    this->passPowers.erase(std::ranges::unique(this->passPowers).begin(), this->passPowers.end());
}

std::vector<WildState5> WildGenerator5::generate(u64 seed, u32 initialAdvances, u32 maxAdvances) const
{
    bool bw = (profile.getVersion() & Game::BW) != Game::None;

    std::vector<std::pair<u32, std::array<u8, 6>>> ivs;

    RNGList<u8, MT, 8, gen> rngList(seed >> 32, initialAdvances + (bw ? 0 : 2));
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++, rngList.advanceState())
    {
        std::array<u8, 6> iv;
        std::ranges::generate(iv, [&rngList] { return rngList.next(); });
        if (filter.compareIV(iv))
        {
            ivs.emplace_back(initialAdvances + cnt, iv);
        }
    }

    if (ivs.empty())
    {
        return std::vector<WildState5>();
    }
    else
    {
        return generate(seed, ivs);
    }
}

std::vector<WildState5> WildGenerator5::generate(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs) const
{
    if (passPowers.size() == 1)
    {
        if (requirePassPowerIVAdvance && passPowers[0] != PassPower5::None)
        {
            auto powerIVs = ivs;
            std::erase_if(powerIVs, [](const auto &iv) { return iv.first < 2; });
            return powerIVs.empty() ? std::vector<WildState5>() : generate(seed, powerIVs, passPowers[0]);
        }

        return generate(seed, ivs, passPowers[0]);
    }

    std::vector<WildState5> states;
    for (u8 activePassPower : passPowers)
    {
        auto powerIVs = ivs;
        if (requirePassPowerIVAdvance && activePassPower != PassPower5::None)
        {
            std::erase_if(powerIVs, [](const auto &iv) { return iv.first < 2; });
        }
        if (powerIVs.empty())
        {
            continue;
        }

        auto powerStates = generate(seed, powerIVs, activePassPower);
        states.reserve(states.size() + powerStates.size());
        for (const auto &state : powerStates)
        {
            auto duplicate = std::ranges::find_if(states, [&state](const WildState5 &other) {
                return state.getAdvances() == other.getAdvances() && state.getIVAdvances() == other.getIVAdvances()
                    && state.getMovingTrigger() == other.getMovingTrigger() && state.getMovingSteps() == other.getMovingSteps()
                    && state.getItem() == other.getItem() && state.getEncounterSlot() == other.getEncounterSlot()
                    && state.getSpecie() == other.getSpecie() && state.getForm() == other.getForm() && state.getLevel() == other.getLevel()
                    && state.getPID() == other.getPID() && state.getShiny() == other.getShiny() && state.getNature() == other.getNature()
                    && state.getAbility() == other.getAbility() && state.getIVs() == other.getIVs()
                    && state.getGender() == other.getGender() && state.isValid() == other.isValid();
            });
            if (duplicate == states.end())
            {
                states.emplace_back(state);
            }
        }
    }
    return states;
}

std::vector<WildState5> WildGenerator5::generate(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs, u8 passPower) const
{
    u8 luckyPower = getLuckyPower(passPower);
    u32 advances = Utilities5::initialAdvances(seed, profile);
    u32 start = advances + initialAdvances;
    bool bw2 = (profile.getVersion() & Game::BW2) != Game::None;
    bool bw = (profile.getVersion() & Game::BW) != Game::None;
    BWRNG rng(seed, start);
    BWRNG encounterRNG(seed, start + (searchMovingTrigger && bw2 ? 1 : 0));
    u32 triggerOffset = 0;
    if (searchMovingTrigger)
    {
        if (bw2)
        {
            triggerOffset = isStepModifier(lead) ? 0 : 1;
        }
        else if (bw && lead == Lead::None)
        {
            triggerOffset = 1;
        }
    }
    BWRNG triggerRNG(seed, start + triggerOffset);
    auto jump = rng.getJump(offset);

    auto modifiedSlots = area.getSlots(lead);

    u8 rate = area.getRate();
    if (area.getEncounter() == Encounter::SuperRod && lead == Lead::SuctionCups)
    {
        rate *= 2;
    }

    u8 shinyRolls = 1;
    if ((profile.getVersion() & Game::BW2) != Game::None)
    {
        if (profile.getShinyCharm())
        {
            shinyRolls += 2;
        }

        if (luckyPower == 3)
        {
            shinyRolls++;
        }
    }

    std::vector<WildState5> states;
    bool nsPokemonReleasedOffset
        = profile.getMemoryLink() && profile.getNsPokemonReleased() && usesNsPokemonReleasedOffset(area.getEncounter());
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        BWRNG rowRng(searchMovingTrigger ? encounterRNG : rng, jump);
        BWRNG payloadRng(searchMovingTrigger ? encounterRNG : rng);
        if (nsPokemonReleasedOffset)
        {
            payloadRng.next();
        }
        BWRNG go(payloadRng, jump);

        bool cuteCharm = false;
        bool magnetStatic = false;
        bool phenomenonItem = false;
        bool pressure = false;
        bool sync = false;

        if (area.getEncounter() == Encounter::FlyingShadow)
        {
            phenomenonItem = !checkFlyingShadowBattle(go);
        }
        else if (canYieldPhenomenonItem(area.getEncounter()))
        {
            u8 battleRate = area.getEncounter() == Encounter::DustCloud ? 40 : 20;
            phenomenonItem = getPercentRand(go, bw) >= battleRate;
        }

        if (searchMovingTrigger && bw2 && lead == Lead::None)
        {
            getPercentRand(go, bw);
            getPercentRand(go, bw);
        }
        else if (searchMovingTrigger && bw && lead == Lead::None)
        {
            getPercentRand(go, bw);
        }

        if (area.getEncounter() == Encounter::FlyingShadow && !skipsLeadCheck(area.getEncounter(), lead))
        {
            if (lead == Lead::CuteCharmM || lead == Lead::CuteCharmF)
            {
                cuteCharm = checkFlyingShadowLead(go, lead);
                if (!cuteCharm)
                {
                    go.advance(1);
                }
            }
            else
            {
                bool flag = checkFlyingShadowLead(go, lead);
                if (lead == Lead::MagnetPull || lead == Lead::Static)
                {
                    magnetStatic = flag;
                }
                else if (lead == Lead::Pressure)
                {
                    pressure = flag;
                }
                else if (lead <= Lead::SynchronizeEnd)
                {
                    sync = flag;
                }
            }
        }
        else if (!phenomenonItem && !skipsLeadCheck(area.getEncounter(), lead) && (!searchMovingTrigger || !(bw && isStepModifier(lead))))
        {
            // Failed cute charm continues to check for other leads
            if ((lead == Lead::CuteCharmM || lead == Lead::CuteCharmF) && getPercentRand(go, bw) < 67)
            {
                cuteCharm = true;
            }
            else
            {
                bool flag;
                if (nsPokemonReleasedOffset && lead <= Lead::SynchronizeEnd)
                {
                    flag = getPercentRand(rowRng, bw) >= 50;
                    getPercentRand(go, bw);
                }
                else
                {
                    flag = getPercentRand(go, bw) >= 50;
                }

                if (lead == Lead::MagnetPull || lead == Lead::Static)
                {
                    magnetStatic = flag;
                }
                else if (lead == Lead::Pressure)
                {
                    pressure = flag;
                }
                else if (lead <= Lead::SynchronizeEnd)
                {
                    sync = flag;
                }
            }
        }

        bool doubleBattle = false;
        if (!phenomenonItem && area.getEncounter() == Encounter::GrassDark && getPercentRand(go, bw) < 40)
        {
            doubleBattle = true;
        }

        if (!phenomenonItem && area.getEncounter() == Encounter::SuperRod && getPercentRand(go, bw) > rate)
        {
            rng.next();
            continue;
        }

        BWRNG triggerGo(triggerRNG, jump);
        u8 movingTrigger = searchMovingTrigger ? (bw2 ? getMovingTrigger(triggerGo) : getMovingTrigger(triggerGo, bw, lead, area.getEncounter()))
                                               : StepEncounter5::impossible;
        u8 movingSteps = searchMovingTrigger
            ? StepEncounter5::getSteps(profile.getVersion(), area.getEncounter(), area.getRate(), movingTrigger,
                                       getStepEncounterModifier(lead, passPower))
            : StepEncounter5::impossible;
        bool valid = !searchMovingTrigger || movingSteps != StepEncounter5::impossible;
        if (requireMovingTrigger && movingSteps == StepEncounter5::impossible)
        {
            rng.nextUInt(0x1fff);
            encounterRNG.next();
            triggerRNG.next();
            continue;
        }

        if (searchMovingTrigger && !bw2)
        {
            getMovingTrigger(go);
        }

        u8 encounterSlot = 0;
        u8 level = 0;
        u16 item = 0;

        if (phenomenonItem)
        {
            item = getPhenomenonItem(go, area.getEncounter(), profile.getVersion(), area.getLocation());
            go.advance(1);
        }
        else
        {
            if (area.getPokemon(12).getSpecie() != 0 && getPercentRand(go, bw) < 40)
            {
                encounterSlot = 12;
                go.advance(1);
            }
            else if (magnetStatic && !modifiedSlots.empty())
            {
                encounterSlot = modifiedSlots[getEncounterRand(go, modifiedSlots.count, bw)];
            }
            else
            {
                encounterSlot = EncounterSlot::bwSlot(getPercentRand(go, bw), area.getEncounter(), luckyPower);
            }

            level = area.calculateLevel(encounterSlot, getPercentRand(go, bw), pressure);

            // RNG calls for left encounter slot and level
            if (doubleBattle)
            {
                go.advance(2);
            }
        }

        const Slot &slot = area.getPokemon(encounterSlot);
        const PersonalInfo *info = slot.getInfo();

        u32 pid = 0;
        for (u8 i = 0; i < shinyRolls; i++)
        {
            // Only allow cute charm if the target isn't fixed gender
            u8 gender = cuteCharm && !info->getFixedGender() ? (lead == Lead::CuteCharmF ? 0 : 1) : 255;

            pid = Utilities5::createPID(tsv, 2, gender, Shiny::Random, true, info->getGender(), go);
            if (Utilities::isShiny<true>(pid, tsv))
            {
                break;
            }
        }

        u8 ability = (pid >> 16) & 1;
        u8 gender = Utilities::getGender(pid, info);
        u8 shiny = Utilities::getShiny<true>(pid, tsv);

        u8 nature = go.nextUInt(25);
        if (sync)
        {
            nature = toInt(lead);
        }

        if (!phenomenonItem)
        {
            item = getItem(go, bw, lead, area.getEncounter(), info);
        }

        bool phenomenon = canTriggerPhenomenon(area.getEncounter()) && BWRNG(rng).nextUInt(1000) < getPhenomenonRate(area.getEncounter());
        u32 prng = rng.nextUInt();
        if (passPower != PassPower5::None && initialAdvances + cnt < 4)
        {
            if (searchMovingTrigger)
            {
                encounterRNG.next();
                triggerRNG.next();
            }
            continue;
        }

        if (searchMovingTrigger)
        {
            encounterRNG.next();
            triggerRNG.next();
        }
        for (const auto &iv : ivs)
        {
            WildState5 state(prng, movingTrigger, movingSteps, phenomenon, phenomenonItem, advances + initialAdvances + cnt, iv.first, pid,
                             iv.second, ability, gender, level, nature, shiny, encounterSlot, item, slot.getSpecie(), slot.getForm(), info, valid,
                             passPower);
            if (!valid || phenomenonItem || filter.compareState(static_cast<const WildState &>(state)))
            {
                states.emplace_back(state);
            }
        }
    }

    return states;
}
