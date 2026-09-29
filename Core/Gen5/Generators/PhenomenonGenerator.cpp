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

#include "PhenomenonGenerator.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/Enum/Lead.hpp>
#include <Core/Enum/Method.hpp>
#include <Core/Enum/Shiny.hpp>
#include <Core/Gen5/PhenomenonArea.hpp>
#include <Core/Gen5/States/PhenomenonState.hpp>
#include <Core/Gen5/States/State5.hpp>
#include <Core/RNG/LCRNG64.hpp>
#include <Core/Util/Utilities.hpp>
#include <algorithm>
#include <iterator>

PhenomenonGenerator::PhenomenonGenerator(u32 initialAdvances, u32 maxAdvances, u32 offset, const PhenomenonArea &area,
                                         const Profile5 &profile, const PhenomenonFilter &filter) :
    Generator(initialAdvances, maxAdvances, offset, Method::None, profile, filter),
    area(area),
    aggregateItems(false),
    minItemAmount(1),
    minItemDistance(0),
    postItemPhenomenonDistance(0),
    preItemPhenomenonDistance(0),
    exploringPowers({ 0 })
{
}

PhenomenonGenerator::PhenomenonGenerator(u32 initialAdvances, u32 maxAdvances, u32 offset, const PhenomenonArea &area,
                                         const Profile5 &profile, const PhenomenonFilter &filter, u8 minItemAmount,
                                         u32 minItemDistance, u32 postItemPhenomenonDistance, u32 preItemPhenomenonDistance,
                                         const std::vector<u8> &exploringPowers) :
    Generator(initialAdvances, maxAdvances, offset, Method::None, profile, filter),
    area(area),
    aggregateItems(true),
    minItemAmount(minItemAmount),
    minItemDistance(minItemDistance),
    postItemPhenomenonDistance(postItemPhenomenonDistance),
    preItemPhenomenonDistance(preItemPhenomenonDistance),
    exploringPowers(exploringPowers.empty() ? std::vector<u8> { 0 } : exploringPowers)
{
    std::ranges::sort(this->exploringPowers);
    this->exploringPowers.erase(std::ranges::unique(this->exploringPowers).begin(), this->exploringPowers.end());
}

std::vector<PhenomenonState> PhenomenonGenerator::generate(u64 seed) const
{
    u32 advances = Utilities5::initialAdvances(seed, profile);
    BWRNG rng(seed, advances + initialAdvances);
    auto jump = rng.getJump(offset);

    u16 ratio = area.getRate();
    u16 triggerRate = area.getTriggerRate();

    std::vector<PhenomenonState> states;
    std::vector<std::pair<u32, u16>> phenomenonRolls;
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        BWRNG go(rng, jump);
        bool valid = go.nextUInt(1000) >= ratio;
        u16 item = area.getItem(go);

        u32 prng = rng.nextUInt();
        u16 phenomenonRoll = (static_cast<u64>(prng) * 1000) >> 32;
        bool phenomenon = phenomenonRoll < triggerRate;
        PhenomenonState state(prng, advances + initialAdvances + cnt, item, phenomenon, valid);
        phenomenonRolls.emplace_back(state.getAdvances(), phenomenonRoll);
        if (filter.compare(state))
        {
            states.emplace_back(state);
        }
    }

    if (!aggregateItems)
    {
        return states;
    }

    static constexpr u16 triggerModifiers[] = { 0, 100, 150, 200 };
    for (u8 exploringPower : exploringPowers)
    {
        exploringPower = std::min<u8>(exploringPower, 3);
        std::vector<u32> phenomenonAdvances;
        for (const auto &[advance, roll] : phenomenonRolls)
        {
            if (roll < triggerRate + triggerModifiers[exploringPower])
            {
                phenomenonAdvances.emplace_back(advance);
            }
        }

        std::vector<PhenomenonState> counted;
        counted.reserve(minItemAmount);
        std::vector<std::vector<u32>> countedPhenomenonAdvances;
        countedPhenomenonAdvances.reserve(minItemAmount);
        u32 previousAdvance = advances + initialAdvances;
        u32 effectivePostDistance = exploringPower == 3 ? postItemPhenomenonDistance / 2 : postItemPhenomenonDistance;
        for (const auto &state : states)
        {
            if (!state.isValid())
            {
                continue;
            }

            if (!counted.empty() && state.getAdvances() - counted.back().getAdvances() < minItemDistance)
            {
                continue;
            }

            auto firstPhenomenon = std::ranges::upper_bound(phenomenonAdvances, previousAdvance);
            while (firstPhenomenon != phenomenonAdvances.cend() && *firstPhenomenon - previousAdvance < effectivePostDistance)
            {
                ++firstPhenomenon;
            }

            auto lastPhenomenon = phenomenonAdvances.cbegin();
            if (preItemPhenomenonDistance == 0)
            {
                lastPhenomenon = std::ranges::lower_bound(phenomenonAdvances, state.getAdvances());
            }
            else if (state.getAdvances() >= preItemPhenomenonDistance)
            {
                lastPhenomenon
                    = std::ranges::upper_bound(phenomenonAdvances, state.getAdvances() - preItemPhenomenonDistance);
            }
            if (firstPhenomenon < lastPhenomenon)
            {
                counted.emplace_back(state);
                auto firstDisplayedPhenomenon = lastPhenomenon - firstPhenomenon > 3 ? lastPhenomenon - 3 : firstPhenomenon;
                countedPhenomenonAdvances.emplace_back(firstDisplayedPhenomenon, lastPhenomenon);
                previousAdvance = state.getAdvances();
            }
        }

        if (counted.size() >= minItemAmount)
        {
            PhenomenonState result = counted.front();
            std::vector<u32> targetAdvances;
            targetAdvances.reserve(counted.size());
            std::ranges::transform(counted, std::back_inserter(targetAdvances), &PhenomenonState::getAdvances);
            result.setTargetAdvances(std::move(targetAdvances));
            result.setTargetPhenomenonAdvances(std::move(countedPhenomenonAdvances));
            result.setExploringPower(exploringPower);
            return { result };
        }
    }

    return {};
}
