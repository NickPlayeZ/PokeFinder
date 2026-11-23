/*
 * This file is part of PokeFinder
 * Copyright (C) 2017-2026 by Admiral_Fish, bumba, EzPzStreamz, and contributors
 */

#ifndef WHITEFORESTDATA_HPP
#define WHITEFORESTDATA_HPP

#include <Core/Global.hpp>
#include <array>

struct WhiteForestResident
{
    u16 grassSpecies;
    u16 waterSpecies;
    u16 item;
    u8 grassWeight;
    u8 waterWeight;
    u8 itemChance;
};

struct BlackCityResident
{
    u16 species1;
    u16 species2;
    u16 item;
    u32 price;
};

namespace WhiteForest5
{
    inline constexpr std::array<const char *, 30> residentNames = {
        "Leo",     "Silvia",  "Dave",   "Britney", "Robbie", "Miki",    "Ryder",  "Karenna", "Doug",  "Vincent",
        "Marie",   "Lena",    "Carlos", "Molly",   "Frederic", "Lynette", "Pierce", "Gene",    "Piper", "Miho",
        "Shane",   "Ralph",   "Eliza",  "Collin",  "Ken",    "Jacques", "Rosa",   "Herman",  "Emi",   "Grace"
    };

    inline constexpr std::array<u8, 30> residentNameOrder = {
        3, 12, 23, 2, 8, 22, 28, 14, 17, 29, 27, 25, 7, 24, 11,
        0, 15, 10, 19, 5, 13, 16, 18, 21, 4, 26, 6, 20, 1, 9
    };

    inline constexpr std::array<u8, 30> initialWeights = {
        20, 20, 20, 20, 20, 20, 20, 10, 20, 10, 20, 6, 20, 4, 1,
        20, 20, 20, 20, 20, 20, 10, 20, 20, 10, 20, 6, 4, 20, 1
    };

    inline constexpr std::array<WhiteForestResident, 30> residents = { {
        { 16, 283, 90, 20, 20, 10 }, { 265, 283, 4, 20, 20, 20 }, { 92, 283, 86, 20, 20, 30 },
        { 187, 283, 106, 20, 20, 5 }, { 239, 283, 3, 10, 20, 10 }, { 175, 283, 9, 10, 20, 5 },
        { 66, 270, 15, 20, 10, 5 }, { 287, 270, 85, 20, 10, 3 }, { 403, 270, 72, 20, 10, 10 },
        { 240, 341, 82, 10, 6, 3 }, { 81, 270, 75, 20, 10, 10 }, { 280, 194, 80, 6, 6, 3 },
        { 396, 341, 110, 20, 6, 3 }, { 298, 270, 107, 4, 10, 2 }, { 406, 194, 94, 6, 6, 5 },
        { 43, 283, 93, 20, 20, 5 }, { 179, 283, 4, 20, 20, 30 }, { 304, 283, 87, 20, 20, 20 },
        { 69, 283, 8, 20, 20, 10 }, { 273, 283, 2, 20, 20, 10 }, { 111, 283, 6, 20, 20, 5 },
        { 270, 283, 84, 20, 20, 3 }, { 328, 270, 73, 20, 10, 10 }, { 63, 270, 74, 10, 10, 10 },
        { 32, 341, 83, 10, 6, 3 }, { 29, 270, 7, 10, 10, 5 }, { 293, 270, 81, 10, 10, 3 },
        { 137, 194, 108, 4, 6, 2 }, { 440, 341, 10, 10, 6, 5 }, { 371, 194, 109, 4, 6, 1 },
    } };

    inline constexpr std::array<BlackCityResident, 30> blackCityResidents = { {
        { 18, 0, 90, 10000 }, { 267, 269, 88, 6000 }, { 94, 0, 86, 3000 }, { 189, 0, 106, 30000 },
        { 466, 0, 30, 1000 }, { 468, 0, 77, 5500 }, { 68, 0, 92, 50000 }, { 289, 0, 85, 10000 },
        { 405, 0, 72, 3000 }, { 467, 0, 82, 10000 }, { 462, 0, 75, 3000 }, { 282, 475, 80, 10000 },
        { 398, 0, 110, 10000 }, { 184, 0, 107, 10000 }, { 407, 0, 11, 50000 }, { 45, 182, 93, 10000 },
        { 181, 0, 4, 10000 }, { 306, 0, 87, 20000 }, { 71, 0, 91, 48000 }, { 275, 0, 89, 38000 },
        { 464, 0, 43, 1500 }, { 272, 0, 84, 10000 }, { 330, 0, 73, 3000 }, { 65, 0, 74, 3000 },
        { 34, 0, 83, 10000 }, { 31, 0, 63, 18000 }, { 295, 0, 81, 10000 }, { 474, 0, 108, 10000 },
        { 242, 0, 64, 15000 }, { 373, 0, 109, 10000 },
    } };
}

#endif // WHITEFORESTDATA_HPP
