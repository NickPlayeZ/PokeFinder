/*
 * This file is part of PokeFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 */

#ifndef PHENOMENONMAP_HPP
#define PHENOMENONMAP_HPP

#include <Core/Enum/Encounter.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Global.hpp>
#include <array>
#include <vector>

enum class PhenomenonTileType : u8
{
    None,
    Grass,
    Cave,
    Water,
    Bridge
};

struct PhenomenonChunk
{
    u8 x;
    u8 z;
    u8 startX;
    u8 startZ;
    bool defaultChunk;
    std::array<bool, 1024> tiles;
    std::array<bool, 1024> npcTiles;
    std::array<bool, 1024> movingNpcTiles;
    std::array<bool, 1024> playerTiles;
    std::array<PhenomenonTileType, 1024> tileTypes;
};

/**
 * Returns the known 32x32 chunks for one location and phenomenon method.
 * Keeping the method in the key prevents similarly numbered grass, cave,
 * water, and bridge maps from sharing incompatible tile eligibility.
 */
std::vector<PhenomenonChunk> getPhenomenonChunks(u16 location, Game version, Encounter encounter);

#endif // PHENOMENONMAP_HPP
