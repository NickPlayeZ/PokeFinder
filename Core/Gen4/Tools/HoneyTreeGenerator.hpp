#ifndef HONEYTREEGENERATOR_HPP
#define HONEYTREEGENERATOR_HPP

#include <Core/Global.hpp>
#include <vector>

enum class Game : u32;

struct HoneyTreeState
{
    u32 advances;
    u8 chatot;
    u8 group;
    u8 slot;
    u16 species;
    u8 shakes;
};

class HoneyTreeGenerator
{
public:
    HoneyTreeGenerator(u32 initialAdvances, u32 maxAdvances, bool sameTree, u8 previousGroup, bool munchlaxTree, Game version);
    std::vector<HoneyTreeState> generate(u32 seed) const;

    static bool isMunchlaxTree(u16 tid, u16 sid, u8 tree);
    static u16 getSpecies(Game version, u8 group, u8 slot);

private:
    u32 initialAdvances;
    u32 maxAdvances;
    bool sameTree;
    u8 previousGroup;
    bool munchlaxTree;
    Game version;
};

#endif // HONEYTREEGENERATOR_HPP
