#include "WhiteForestGenerator.hpp"
#include <Core/Gen5/Profile5.hpp>
#include <Core/Gen5/WhiteForestData.hpp>
#include <Core/RNG/LCRNG64.hpp>
#include <Core/Util/Utilities.hpp>
#include <algorithm>

std::vector<WhiteForestItemState> WhiteForestGenerator::generate(u64 seed) const
{
    auto states = generateItems(seed);
    std::erase_if(states, [this](const WhiteForestItemState &state) {
        std::array<u8, 256> generated {};
        for (u16 item : state.items)
            if (item < generated.size()) generated[item]++;
        for (size_t item = 0; item < requiredItems.size(); item++)
            if (requiredItems[item] > generated[item]) return true;
        return false;
    });
    return states;
}

std::vector<WhiteForestItemState> WhiteForestGenerator::generateItems(u64 seed) const
{
    std::vector<WhiteForestItemState> states;
    states.reserve(maxAdvances + 1);
    const u32 base = Utilities5::initialAdvances(seed, profile) + initialAdvances;

    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        BWRNG rng(seed, base + cnt);
        // The daily event update regenerates the mass outbreak location before
        // calculating the White Forest residents and items.
        rng.next();
        std::array<u16, 10> items {};
        bool hasResident = false;
        for (u8 resident : residents)
        {
            if (resident >= WhiteForest5::residents.size()) continue;
            hasResident = true;
            const auto &data = WhiteForest5::residents[resident];
            if (rng.nextUInt(100) < data.itemChance)
            {
                u8 position = rng.nextUInt(10);
                if (items[position] == 0) items[position] = data.item;
            }
        }
        if (hasResident) rng.next();
        states.emplace_back(WhiteForestItemState { base + cnt, items });
    }
    return states;
}
