#include "HoneyTreeGenerator.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/RNG/LCRNG.hpp>
#include <array>

namespace
{
    u8 nextPercent(PokeRNG &rng)
    {
        return static_cast<u8>(rng.nextUShort<false>(100));
    }

    u8 getGroup(PokeRNG &rng, bool munchlax)
    {
        const u8 roll = nextPercent(rng);
        if (munchlax)
        {
            if (roll < 1)
                return 3;
            if (roll < 10)
                return 0;
            if (roll < 30)
                return 1;
            return 2;
        }
        if (roll < 10)
            return 0;
        if (roll < 30)
            return 2;
        return 1;
    }

    u8 getSlot(PokeRNG &rng)
    {
        const u8 roll = nextPercent(rng);
        if (roll < 5)
            return 5;
        if (roll < 10)
            return 4;
        if (roll < 20)
            return 3;
        if (roll < 40)
            return 2;
        if (roll < 60)
            return 1;
        return 0;
    }

    u8 getShakes(PokeRNG &rng, u8 group)
    {
        const u8 roll = nextPercent(rng);
        if (group == 3)
        {
            if (roll < 5)
                return 2;
            if (roll < 6)
                return 1;
            if (roll < 7)
                return 0;
            return 3;
        }
        if (group == 2)
        {
            if (roll < 75)
                return 2;
            if (roll < 95)
                return 1;
            if (roll < 96)
                return 0;
            return 3;
        }
        if (group == 1)
        {
            if (roll < 19)
                return 2;
            if (roll < 79)
                return 1;
            if (roll < 99)
                return 0;
            return 3;
        }
        if (roll < 1)
            return 2;
        if (roll < 19)
            return 1;
        if (roll < 99)
            return 0;
        return 3;
    }
}

HoneyTreeGenerator::HoneyTreeGenerator(u32 initialAdvances, u32 maxAdvances, bool sameTree, u8 previousGroup, bool munchlaxTree,
                                       Game version) :
    initialAdvances(initialAdvances),
    maxAdvances(maxAdvances),
    sameTree(sameTree),
    previousGroup(previousGroup),
    munchlaxTree(munchlaxTree),
    version(version)
{
}

std::vector<HoneyTreeState> HoneyTreeGenerator::generate(u32 seed) const
{
    std::vector<HoneyTreeState> states;
    states.reserve(maxAdvances + 1);
    PokeRNG rng(seed, initialAdvances);
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        PokeRNG go(rng);
        u8 group;
        bool repeated = false;
        if (sameTree && nextPercent(go) < 90)
        {
            group = previousGroup;
            repeated = true;
        }
        else
        {
            group = getGroup(go, munchlaxTree);
        }

        u8 slot = 0xff;
        u16 species = 0;
        if (group != 0 || repeated)
        {
            slot = getSlot(go);
            species = getSpecies(version, group == 0 ? 1 : group, slot);
        }
        const u8 shakes = getShakes(go, group);
        const u16 prng = rng.nextUShort();
        states.push_back({ initialAdvances + cnt, static_cast<u8>(((prng % 8192) * 100) >> 13), group, slot, species, shakes });
    }
    return states;
}

bool HoneyTreeGenerator::isMunchlaxTree(u16 tid, u16 sid, u8 tree)
{
    std::array<u8, 4> trees = { static_cast<u8>(sid >> 8), static_cast<u8>(sid), static_cast<u8>(tid >> 8), static_cast<u8>(tid) };
    for (u8 &value : trees)
        value %= 21;
    for (u8 i = 1; i < trees.size(); i++)
    {
        for (u8 j = 0; j < i; j++)
        {
            if (trees[i] == trees[j])
                trees[i] = static_cast<u8>((trees[i] + 1) % 21);
        }
    }
    for (u8 value : trees)
    {
        if (value == tree)
            return true;
    }
    return false;
}

u16 HoneyTreeGenerator::getSpecies(Game version, u8 group, u8 slot)
{
    static constexpr std::array<std::array<u16, 6>, 3> diamond
        = { { { 265, 266, 415, 412, 420, 190 }, { 415, 412, 420, 190, 214, 265 }, { 446, 446, 446, 446, 446, 446 } } };
    static constexpr std::array<std::array<u16, 6>, 3> pearl
        = { { { 265, 268, 415, 412, 420, 190 }, { 415, 412, 420, 190, 214, 265 }, { 446, 446, 446, 446, 446, 446 } } };
    static constexpr std::array<std::array<u16, 6>, 3> platinum
        = { { { 415, 265, 412, 420, 190, 190 }, { 412, 420, 415, 190, 190, 214 }, { 446, 446, 446, 446, 446, 446 } } };
    const auto &table = (version & Game::Platinum) != Game::None ? platinum : (version & Game::Pearl) != Game::None ? pearl : diamond;
    return table[group - 1][slot];
}
