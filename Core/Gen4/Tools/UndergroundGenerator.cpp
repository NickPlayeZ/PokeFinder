#include "UndergroundGenerator.hpp"
#include <algorithm>
#include <iterator>

namespace
{
struct MiningObjectData
{
    u8 item;
    u8 width;
    u8 height;
    u64 shape;
    u16 odd;
    u16 even;
    u16 oddDex;
    u16 evenDex;
    const char *name;
};

constexpr MiningObjectData platinumObjects[] = {
#include "UndergroundData.inc"
};

struct WeightData { u16 diamond, pearl, diamondDex, pearlDex; };
constexpr WeightData diamondPearlWeights[] = {
#include "UndergroundDiamondWeights.inc"
};

class MiningRNG
{
public:
    explicit MiningRNG(u64 seed) : state(seed), advances(0) {}
    u32 next(u32 bound)
    {
        state = state * 0x5D588B656C078965ULL + 0x269EC3ULL;
        advances++;
        return static_cast<u32>((static_cast<u64>(state >> 32) * bound) >> 32);
    }
    void advance(u32 count)
    {
        while (count--) next(1);
    }
    u64 getState() const { return state; }
    u32 getAdvances() const { return advances; }
private:
    u64 state;
    u32 advances;
};

u16 weight(const MiningObjectData &object, bool odd, bool dex)
{
    if (dex) return odd ? object.oddDex : object.evenDex;
    return odd ? object.odd : object.even;
}

bool place(const MiningObjectData &object, u8 x, u8 y, u8 value, std::array<u8, 130> &grid)
{
    if (x + object.width > 13 || y + object.height > 10) return false;
    for (u8 row = 0; row < object.height; row++)
    {
        for (u8 column = 0; column < object.width; column++)
        {
            if ((object.shape & (1ULL << (row * object.width + column))) && grid[(y + row) * 13 + x + column]) return false;
        }
    }
    for (u8 row = 0; row < object.height; row++)
    {
        for (u8 column = 0; column < object.width; column++)
        {
            if (object.shape & (1ULL << (row * object.width + column))) grid[(y + row) * 13 + x + column] = value;
        }
    }
    return true;
}
}

UndergroundGenerator::UndergroundGenerator(Game version, u16 tid, bool nationalDex, const std::array<bool, 16> &minedPlates,
                                           bool undergroundOrb) :
    version(version), tid(tid), nationalDex(nationalDex), undergroundOrb(undergroundOrb), minedPlates(minedPlates)
{
}

UndergroundResult UndergroundGenerator::generate(u64 seed) const
{
    MiningRNG rng(seed);
    UndergroundResult result;
    std::array<u8, 130> grid = {};
    const bool odd = (tid & 1) != 0;
    const auto &objects = platinumObjects;
    constexpr size_t treasureCount = 71;
    constexpr size_t rockStart = 71;
    constexpr size_t rockCount = std::size(objects) - rockStart;

    auto activeWeight = [&](size_t index) {
        if (version == Game::Platinum) return weight(objects[index], odd, nationalDex);
        const auto &entry = diamondPearlWeights[index];
        const bool diamond = version == Game::Diamond;
        u16 value = nationalDex ? (diamond ? entry.diamondDex : entry.pearlDex) : (diamond ? entry.diamond : entry.pearl);
        if (undergroundOrb)
        {
            if ((diamond && objects[index].item == 1) || (!diamond && objects[index].item == 2)) value += 66;
            if ((diamond && objects[index].item == 6) || (!diamond && objects[index].item == 7)) value += 33;
        }
        return value;
    };
    u32 totalWeight = 0;
    for (size_t i = 0; i < treasureCount; i++) totalWeight += activeWeight(i);
    result.itemCount = static_cast<u8>(rng.next(3) + 2);

    std::array<u8, 4> selectedPlates = {};
    for (u8 placed = 0; placed < result.itemCount;)
    {
        s32 roll = static_cast<s32>(rng.next(totalWeight));
        size_t index = 0;
        for (; index < treasureCount; index++)
        {
            roll -= activeWeight(index);
            if (roll < 0) break;
        }
        const auto &object = objects[index];
        if (object.item >= 44 && object.item <= 59)
        {
            if (minedPlates[object.item - 44]) continue;
            if (std::find(selectedPlates.begin(), selectedPlates.begin() + placed, object.item)
                != selectedPlates.begin() + placed) continue;
            selectedPlates[placed] = object.item;
        }
        const u8 x = static_cast<u8>(rng.next(13));
        const u8 y = static_cast<u8>(rng.next(10));
        if (!place(object, x, y, placed + 1, grid)) continue;
        result.items[placed] = object.item;
        result.objects.push_back({ object.item, static_cast<u8>(placed + 1), x, y, object.width, object.height, object.shape, false });
        placed++;
    }

    for (u16 attempt = 0; attempt < 100; attempt++)
    {
        const auto &object = objects[rockStart + rng.next(static_cast<u32>(rockCount))];
        const u8 x = static_cast<u8>(rng.next(13));
        const u8 y = static_cast<u8>(rng.next(10));
        if (result.objects.size() >= 8) continue;
        if (place(object, x, y, static_cast<u8>(result.objects.size() + 1), grid))
        {
            result.objects.push_back({ object.item, 0, x, y, object.width, object.height, object.shape, true });
        }
    }

    result.dirt.fill(2);
    constexpr u8 dirt4[8][8] = { { 0, 0, 4, 4, 4, 4, 0, 0 }, { 0, 4, 4, 4, 4, 4, 4, 0 },
        { 4, 4, 4, 4, 4, 4, 0, 0 }, { 4, 4, 4, 4, 4, 4, 0, 0 }, { 4, 4, 4, 4, 4, 4, 0, 0 },
        { 4, 4, 4, 4, 4, 4, 0, 0 }, { 0, 4, 4, 4, 4, 4, 4, 0 }, { 0, 0, 4, 4, 4, 4, 0, 0 } };
    constexpr u8 dirt6[5][5]
        = { { 0, 6, 6, 6, 0 }, { 6, 6, 6, 6, 6 }, { 6, 6, 6, 6, 6 }, { 6, 6, 6, 6, 6 }, { 0, 6, 6, 6, 0 } };
    for (u8 attempt = 0; attempt < 10; attempt++)
    {
        const int startX = static_cast<int>(rng.next(21)) - 8;
        const int startY = static_cast<int>(rng.next(18)) - 5;
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
        {
            const int boardX = startX + x;
            const int boardY = startY + y;
            if (boardX >= 0 && boardX < 13 && boardY >= 0 && boardY < 10 && dirt4[y][x])
                result.dirt[boardY * 13 + boardX] = dirt4[y][x];
        }
    }
    for (u8 attempt = 0; attempt < 15; attempt++)
    {
        const int startX = static_cast<int>(rng.next(18)) - 5;
        const int startY = static_cast<int>(rng.next(15)) - 5;
        bool valid = true;
        for (int y = 0; y < 5 && valid; y++) for (int x = 0; x < 5; x++)
        {
            const int boardX = startX + x;
            const int boardY = startY + y;
            if (boardX >= 0 && boardX < 13 && boardY >= 0 && boardY < 10 && dirt4[y][x]
                && result.dirt[boardY * 13 + boardX] < 4)
            {
                valid = false;
                break;
            }
        }
        if (!valid) continue;
        for (int y = 0; y < 5; y++) for (int x = 0; x < 5; x++)
        {
            const int boardX = startX + x;
            const int boardY = startY + y;
            if (boardX >= 0 && boardX < 13 && boardY >= 0 && boardY < 10 && dirt6[y][x])
                result.dirt[boardY * 13 + boardX] = dirt6[y][x];
        }
    }
    result.state = rng.getState();
    result.advances = rng.getAdvances();
    return result;
}

std::vector<u8> UndergroundGenerator::getAvailableItems() const
{
    std::vector<u8> items;
    constexpr size_t treasureCount = 71;
    const bool odd = (tid & 1) != 0;
    for (size_t index = 0; index < treasureCount; index++)
    {
        const auto &object = platinumObjects[index];
        u16 activeWeight;
        if (version == Game::Platinum) activeWeight = weight(object, odd, nationalDex);
        else
        {
            const auto &entry = diamondPearlWeights[index];
            const bool diamond = version == Game::Diamond;
            activeWeight = nationalDex ? (diamond ? entry.diamondDex : entry.pearlDex) : (diamond ? entry.diamond : entry.pearl);
            if (undergroundOrb)
            {
                if ((diamond && object.item == 1) || (!diamond && object.item == 2)) activeWeight += 66;
                if ((diamond && object.item == 6) || (!diamond && object.item == 7)) activeWeight += 33;
            }
        }
        if (activeWeight == 0 || (object.item >= 44 && object.item <= 59 && minedPlates[object.item - 44])) continue;
        if (std::find(items.begin(), items.end(), object.item) == items.end()) items.push_back(object.item);
    }
    return items;
}

u64 UndergroundGenerator::advanceSeed(u64 seed, u32 advances)
{
    MiningRNG rng(seed);
    rng.advance(advances);
    return rng.getState();
}

u64 UndergroundGenerator::advanceBetweenSpots(u64 state, const UndergroundResult &current, bool allItemsReceived,
                                               const std::array<bool, 4> &uncovered, const std::vector<u8> &tools, u32 &advances)
{
    MiningRNG rng(state);
    u16 lostIntegrity = 0;
    for (size_t hit = 0; hit < tools.size(); hit++)
    {
        lostIntegrity = std::min<u16>(196, lostIntegrity + (tools[hit] == 1 ? 8 : 4));
        const u8 duration = static_cast<u8>(lostIntegrity / 15);
        if (duration > 1) rng.advance(hit + 1 == tools.size() ? 2 : 2 * (duration - 1));
    }

    for (u8 i = 0; i < current.itemCount; i++)
    {
        if (allItemsReceived || uncovered[i])
        {
            rng.advance(6);
            if (current.items[i] >= 1 && current.items[i] <= 10) rng.advance(1);
        }
    }
    if (!allItemsReceived) rng.advance(88);
    advances = rng.getAdvances();
    return rng.getState();
}

std::string UndergroundGenerator::getItemName(u8 item)
{
    for (const auto &object : platinumObjects)
    {
        if (object.item == item) return object.name;
    }
    return "-";
}
