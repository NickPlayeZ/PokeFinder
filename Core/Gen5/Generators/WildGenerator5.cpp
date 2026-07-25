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
#include <unordered_map>

struct WildStateKey
{
    std::array<u8, 6> ivs;
    u32 advances;
    u32 ivAdvances;
    u32 movingTrigger;
    u32 movingSteps;
    u32 pid;
    u16 item;
    u16 specie;
    u8 ability;
    u8 gender;
    u8 level;
    u8 nature;
    u8 shiny;
    u8 encounterSlot;
    u8 form;
    bool phenomenon;
    bool phenomenonItem;
    bool valid;

    bool operator==(const WildStateKey &other) const = default;
};

struct WildStateKeyHash
{
    size_t operator()(const WildStateKey &key) const
    {
        size_t hash = 0;
        auto combine = [&hash](auto value) {
            hash ^= static_cast<size_t>(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };

        for (u8 iv : key.ivs)
        {
            combine(iv);
        }
        combine(key.advances);
        combine(key.ivAdvances);
        combine(key.movingTrigger);
        combine(key.movingSteps);
        combine(key.pid);
        combine(key.item);
        combine(key.specie);
        combine(key.ability);
        combine(key.gender);
        combine(key.level);
        combine(key.nature);
        combine(key.shiny);
        combine(key.encounterSlot);
        combine(key.form);
        combine(key.phenomenon);
        combine(key.phenomenonItem);
        combine(key.valid);

        return hash;
    }
};

struct WildTargetKey
{
    std::array<u8, 6> ivs;
    u32 advances;
    u32 ivAdvances;
    u32 pid;
    u16 item;
    u16 specie;
    u8 ability;
    u8 gender;
    u8 level;
    u8 nature;
    u8 shiny;
    u8 encounterSlot;
    u8 form;

    bool operator==(const WildTargetKey &other) const = default;
};

struct WildTargetKeyHash
{
    size_t operator()(const WildTargetKey &key) const
    {
        size_t hash = 0;
        auto combine = [&hash](auto value) {
            hash ^= static_cast<size_t>(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };

        for (u8 iv : key.ivs)
        {
            combine(iv);
        }
        combine(key.advances);
        combine(key.ivAdvances);
        combine(key.pid);
        combine(key.item);
        combine(key.specie);
        combine(key.ability);
        combine(key.gender);
        combine(key.level);
        combine(key.nature);
        combine(key.shiny);
        combine(key.encounterSlot);
        combine(key.form);

        return hash;
    }
};

static WildStateKey getStateKey(const WildState5 &state)
{
    return { { state.getIV(0), state.getIV(1), state.getIV(2), state.getIV(3), state.getIV(4), state.getIV(5) },
             state.getAdvances(),
             state.getIVAdvances(),
             state.getMovingTrigger(),
             state.getMovingSteps(),
             state.getPID(),
             state.getItem(),
             state.getSpecie(),
             state.getAbility(),
             state.getGender(),
             state.getLevel(),
             state.getVariableNature() ? static_cast<u8>(255) : state.getNature(),
             state.getShiny(),
             state.getEncounterSlot(),
             state.getForm(),
             state.getPhenomenon(),
             state.getPhenomenonItem(),
             state.isValid() };
}

static WildTargetKey getTargetKey(const WildState5 &state, u32 advances)
{
    return { { state.getIV(0), state.getIV(1), state.getIV(2), state.getIV(3), state.getIV(4), state.getIV(5) },
             advances,
             state.getIVAdvances(),
             state.getPID(),
             state.getItem(),
             state.getSpecie(),
             state.getAbility(),
             state.getGender(),
             state.getLevel(),
             state.getNature(),
             state.getShiny(),
             state.getEncounterSlot(),
             state.getForm() };
}

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
    if (lead != Lead::None && lead > Lead::SynchronizeEnd && lead != Lead::CompoundEyes && lead != Lead::SuctionCups
        && !(bw && isStepModifier(lead)))
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
    WildGenerator5(initialAdvances, maxAdvances, offset, method, std::vector<Lead> { lead }, passPowers, searchMovingTrigger, requireMovingTrigger,
                   area, profile, filter, requirePassPowerIVAdvance, false)
{
}

WildGenerator5::WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, const std::vector<Lead> &leads, u8 luckyPower,
                               const EncounterArea5 &area, const Profile5 &profile, const WildStateFilter &filter) :
    WildGenerator5(initialAdvances, maxAdvances, offset, method, leads, std::vector<u8> { luckyPower }, false, false, area, profile, filter)
{
}

WildGenerator5::WildGenerator5(u32 initialAdvances, u32 maxAdvances, u32 offset, Method method, const std::vector<Lead> &leads,
                               const std::vector<u8> &passPowers, bool searchMovingTrigger, bool requireMovingTrigger,
                               const EncounterArea5 &area, const Profile5 &profile, const WildStateFilter &filter,
                               bool requirePassPowerIVAdvance, bool filterNonRequiredLeads) :
    WildGenerator(initialAdvances, maxAdvances, offset, method, leads.empty() ? Lead::None : leads.front(), area, profile, filter),
    passPowers(passPowers),
    leads(leads.empty() ? std::vector<Lead> { Lead::None } : leads),
    searchMovingTrigger(searchMovingTrigger),
    requireMovingTrigger(requireMovingTrigger),
    requirePassPowerIVAdvance(requirePassPowerIVAdvance),
    filterNonRequiredLeads(filterNonRequiredLeads)
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

    std::ranges::sort(this->leads);
    this->leads.erase(std::ranges::unique(this->leads).begin(), this->leads.end());
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
        if (area.getEncounter() == Encounter::SuperRod || filter.compareIV(iv))
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
    std::vector<WildState5> states;
    std::unordered_map<WildStateKey, size_t, WildStateKeyHash> seen;
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

        for (Lead activeLead : leads)
        {
            auto leadStates = generate(seed, powerIVs, activePassPower, activeLead);
            states.reserve(states.size() + leadStates.size());
            for (auto state : leadStates)
            {
                if (requireMovingTrigger && !state.isValid())
                {
                    continue;
                }

                if (filterNonRequiredLeads && state.getLead() != Lead::None && !state.getLeadRequired())
                {
                    continue;
                }

                if (!state.getPhenomenonItem() && !filter.compareState(static_cast<const WildState &>(state)))
                {
                    continue;
                }

                auto key = getStateKey(state);
                auto entry = seen.find(key);
                if (entry == seen.end())
                {
                    states.emplace_back(state);
                    seen.emplace(key, states.size() - 1);
                }
                else
                {
                    states[entry->second].addLead(state.getLead());
                }
            }
        }
    }

    std::unordered_map<WildTargetKey, bool, WildTargetKeyHash> noneTargets;
    for (const auto &state : states)
    {
        if (state.getLead() == Lead::None)
        {
            noneTargets.emplace(getTargetKey(state, state.getAdvances()), true);
        }
    }

    if (!noneTargets.empty())
    {
        std::vector<WildState5> filtered;
        filtered.reserve(states.size());
        for (const auto &state : states)
        {
            if (state.getLead() == Lead::None || !noneTargets.contains(getTargetKey(state, state.getAdvances() + 1)))
            {
                filtered.emplace_back(state);
            }
        }
        return filtered;
    }

    return states;
}

std::vector<WildState5> WildGenerator5::generate(u64 seed, const std::vector<std::pair<u32, std::array<u8, 6>>> &ivs, u8 passPower,
                                                 Lead lead) const
{
    u8 luckyPower = getLuckyPower(passPower);
    u32 advances = Utilities5::initialAdvances(seed, profile);
    u32 start = advances + initialAdvances;
    bool bw2 = (profile.getVersion() & Game::BW2) != Game::None;
    bool bw = (profile.getVersion() & Game::BW) != Game::None;
    BWRNG rng(seed, start);
    BWRNG encounterRNG(seed, start);
    u32 triggerOffset = 0;
    if (searchMovingTrigger)
    {
        if (bw2)
        {
            triggerOffset = isStepModifier(lead) ? 0 : 1;
        }
        else if (bw && (lead == Lead::None || lead <= Lead::SynchronizeEnd))
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
        BWRNG payloadRng(searchMovingTrigger ? encounterRNG : rng);
        if (searchMovingTrigger && bw && start + cnt > 0)
        {
            payloadRng = BWRNG(seed, start + cnt - 1);
        }
        else if (!searchMovingTrigger && area.getEncounter() != Encounter::SuperRodRippling && canTriggerPhenomenon(area.getEncounter())
            && !canYieldPhenomenonItem(area.getEncounter()))
        {
            payloadRng.next();
        }

        BWRNG go(payloadRng, jump);
        bool valid = true;

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

        if (searchMovingTrigger && bw
            && (lead == Lead::None || lead <= Lead::SynchronizeEnd || isStepModifier(lead) || lead == Lead::CompoundEyes
                || lead == Lead::SuctionCups))
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
        else if (!phenomenonItem && !skipsLeadCheck(area.getEncounter(), lead) && (!searchMovingTrigger || !isStepModifier(lead)))
        {
            // Failed cute charm continues to check for other leads
            if ((lead == Lead::CuteCharmM || lead == Lead::CuteCharmF) && getPercentRand(go, bw) < 67)
            {
                cuteCharm = true;
            }
            else
            {
                bool flag;
                flag = getPercentRand(go, bw) >= 50;

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
            valid = false;
        }

        BWRNG triggerGo(triggerRNG, jump);
        u8 movingTrigger = searchMovingTrigger ? (bw2 ? getMovingTrigger(go) : getMovingTrigger(triggerGo, bw, lead, area.getEncounter()))
                                               : StepEncounter5::impossible;
        u8 movingSteps = searchMovingTrigger
            ? StepEncounter5::getSteps(profile.getVersion(), area.getEncounter(), area.getRate(), movingTrigger,
                                       getStepEncounterModifier(lead, passPower))
            : StepEncounter5::impossible;
        valid &= !searchMovingTrigger || movingSteps != StepEncounter5::impossible;
        if (requireMovingTrigger && !valid)
        {
            rng.nextUInt(0x1fff);
        }

        if (searchMovingTrigger && !bw2)
        {
            getMovingTrigger(go);
        }

        if (nsPokemonReleasedOffset)
        {
            go.advance(1);
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
        bool variableNature = sync;
        if (sync)
        {
            nature = toInt(lead);
        }

        bool leadRequired = lead == Lead::None || cuteCharm || magnetStatic || pressure || sync || lead == Lead::CompoundEyes
            || (lead == Lead::SuctionCups && area.getEncounter() == Encounter::SuperRod) || (lead == Lead::ArenaTrap && searchMovingTrigger);

        if (!phenomenonItem)
        {
            item = getItem(go, bw, lead, area.getEncounter(), info);
        }

        bool phenomenon = canTriggerPhenomenon(area.getEncounter()) && BWRNG(rng).nextUInt(1000) < getPhenomenonRate(area.getEncounter());
        u32 prng = rng.nextUInt();
        if (searchMovingTrigger)
        {
            encounterRNG.next();
            triggerRNG.next();
        }
        for (const auto &iv : ivs)
        {
            WildState5 state(prng, movingTrigger, movingSteps, phenomenon, phenomenonItem, advances + initialAdvances + cnt, iv.first, pid,
                             iv.second, ability, gender, level, nature, shiny, encounterSlot, item, slot.getSpecie(), slot.getForm(), info, valid,
                             passPower, lead, variableNature, leadRequired);
            if (!valid || phenomenonItem || filter.compareState(static_cast<const WildState &>(state)))
            {
                states.emplace_back(state);
            }
        }
    }

    return states;
}
