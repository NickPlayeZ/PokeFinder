#ifndef UNDERGROUNDGENERATOR_HPP
#define UNDERGROUNDGENERATOR_HPP

#include <Core/Enum/Game.hpp>
#include <Core/Global.hpp>
#include <array>
#include <string>
#include <vector>

struct UndergroundObject
{
    u8 item;
    u8 number;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    u64 shape;
    bool metal;
};

struct UndergroundResult
{
    std::array<u8, 4> items = {};
    std::array<u8, 130> dirt = {};
    u8 itemCount = 0;
    std::vector<UndergroundObject> objects;
    u64 state = 0;
    u32 advances = 0;
};

struct UndergroundItemResult
{
    std::array<u8, 4> items = {};
    u8 itemCount = 0;
};

class UndergroundGenerator
{
public:
    UndergroundGenerator(Game version, u16 tid, bool nationalDex, const std::array<bool, 16> &minedPlates, bool undergroundOrb);
    UndergroundResult generate(u64 seed) const;
    UndergroundItemResult generateItems(u64 seed) const;
    static u64 advanceSeed(u64 seed, u32 advances);
    static u64 nextSeed(u64 seed);
    static u64 advanceBetweenSpots(u64 state, const UndergroundResult &current, bool allItemsReceived,
                                   const std::array<bool, 4> &uncovered, const std::vector<u8> &tools, u32 &advances);
    static std::string getItemName(u8 item);
    std::vector<u8> getAvailableItems() const;
    void setMinedPlate(u8 item);

private:
    Game version;
    u16 tid;
    bool nationalDex;
    bool undergroundOrb;
    std::array<bool, 16> minedPlates;
    std::array<u16, 71> activeWeights;
    std::array<u32, 71> cumulativeWeights;
    u32 totalWeight;
};

#endif // UNDERGROUNDGENERATOR_HPP
