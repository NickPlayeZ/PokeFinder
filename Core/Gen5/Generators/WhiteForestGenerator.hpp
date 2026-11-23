#ifndef WHITEFORESTGENERATOR_HPP
#define WHITEFORESTGENERATOR_HPP

#include <Core/Global.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <array>
#include <vector>

struct WhiteForestItemState
{
    u32 advances;
    std::array<u16, 10> items;
};

class WhiteForestGenerator
{
public:
    WhiteForestGenerator(u32 initialAdvances, u32 maxAdvances, const std::array<u8, 10> &residents, const Profile5 &profile,
                         const std::array<u8, 256> &requiredItems = {}) :
        initialAdvances(initialAdvances), maxAdvances(maxAdvances), residents(residents), requiredItems(requiredItems), profile(profile)
    {
    }

    std::vector<WhiteForestItemState> generate(u64 seed) const;
    std::vector<WhiteForestItemState> generateItems(u64 seed) const;

private:
    u32 initialAdvances;
    u32 maxAdvances;
    std::array<u8, 10> residents;
    std::array<u8, 256> requiredItems;
    Profile5 profile;
};

#endif // WHITEFORESTGENERATOR_HPP
