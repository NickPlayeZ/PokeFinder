/*
 * This file is part of PokeFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 */

#include "PhenomenonMap.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <string_view>
#include <tuple>
#include <utility>

namespace
{
    constexpr u8 phenomenonTypeMask(PhenomenonTileType type)
    {
        return 1 << (static_cast<u8>(type) - 1);
    }

    struct PhenomenonMapRawChunk
    {
        u16 location;
        Game version;
        u8 number;
        u16 start;
        u16 waterStart;
        bool defaultGround;
        bool defaultWater;
        u8 typeMask;
        std::string_view data;
    };

#include "PhenomenonMapData.hpp"

    PhenomenonTileType getEncounterType(Encounter encounter)
    {
        switch (encounter)
        {
        case Encounter::GrassRustling:
            return PhenomenonTileType::Grass;
        case Encounter::DustCloud:
            return PhenomenonTileType::Cave;
        case Encounter::SurfingRippling:
        case Encounter::SuperRodRippling:
            return PhenomenonTileType::Water;
        case Encounter::FlyingShadow:
            return PhenomenonTileType::Bridge;
        default:
            return PhenomenonTileType::None;
        }
    }

    PhenomenonTileType getTileType(char tile)
    {
        switch (tile)
        {
        case 'g':
        case '1':
        case '5':
            return PhenomenonTileType::Grass;
        case 'c':
        case '2':
        case '6':
            return PhenomenonTileType::Cave;
        case 'w':
        case '3':
        case '7':
            return PhenomenonTileType::Water;
        case 'b':
        case '4':
        case '8':
            return PhenomenonTileType::Bridge;
        default:
            return PhenomenonTileType::None;
        }
    }

    bool versionMatches(Game entry, Game version)
    {
        return (entry & version) != Game::None;
    }

    PhenomenonChunk decodeChunk(const PhenomenonMapRawChunk &raw, PhenomenonTileType selectedType)
    {
        bool water = selectedType == PhenomenonTileType::Water;
        u16 start = water && raw.waterStart != 0xffff ? raw.waterStart : raw.start;
        PhenomenonChunk chunk { 0,
                                raw.number,
                                static_cast<u8>(start % 32),
                                static_cast<u8>(start / 32),
                                water ? raw.defaultWater : raw.defaultGround,
                                { },
                                { },
                                { },
                                { },
                                { } };
        for (size_t i = 0; i < raw.data.size(); i++)
        {
            char tile = raw.data[i];
            PhenomenonTileType type = getTileType(tile);
            bool activeType = type != PhenomenonTileType::None && (raw.typeMask & phenomenonTypeMask(type)) != 0;
            chunk.tileTypes[i] = activeType ? type : PhenomenonTileType::None;
            chunk.tiles[i] = activeType;
            chunk.npcTiles[i] = tile == 'n' || tile == 'm' || (tile >= '1' && tile <= '8');
            chunk.movingNpcTiles[i] = tile == 'm' || (tile >= '1' && tile <= '4');
            chunk.playerTiles[i] = tile != '.' && !chunk.npcTiles[i];
        }
        return chunk;
    }
}

std::vector<PhenomenonChunk> getPhenomenonChunks(u16 location, Game version, Encounter encounter)
{
    PhenomenonTileType selectedType = getEncounterType(encounter);
    if (selectedType == PhenomenonTileType::None || (version & Game::BW2) == Game::None)
    {
        return { };
    }

    using CacheKey = std::tuple<u16, Game, Encounter>;
    static std::map<CacheKey, std::vector<PhenomenonChunk>> cache;
    CacheKey key { location, version, encounter };
    if (auto iterator = cache.find(key); iterator != cache.end())
    {
        return iterator->second;
    }

    bool hasSpecific = std::any_of(bw2PhenomenonChunks.begin(), bw2PhenomenonChunks.end(), [=](const auto &raw) {
        return raw.location == location && raw.version == version && (raw.typeMask & phenomenonTypeMask(selectedType)) != 0;
    });
    std::vector<PhenomenonChunk> chunks;
    for (const auto &raw : bw2PhenomenonChunks)
    {
        if (raw.location != location || (raw.typeMask & phenomenonTypeMask(selectedType)) == 0 || !versionMatches(raw.version, version)
            || (hasSpecific && raw.version != version))
        {
            continue;
        }
        u16 start = selectedType == PhenomenonTileType::Water && raw.waterStart != 0xffff ? raw.waterStart : raw.start;
        if (start == 0xffff)
        {
            continue;
        }
        chunks.emplace_back(decodeChunk(raw, selectedType));
    }
    std::sort(chunks.begin(), chunks.end(), [](const auto &left, const auto &right) { return left.z < right.z; });
    auto iterator = cache.emplace(key, std::move(chunks)).first;
    return iterator->second;
}
