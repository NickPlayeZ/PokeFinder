/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2022 by Admiral_Fish, bumba, and EzPzStreamz
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

#ifndef SFMT_HPP
#define SFMT_HPP

#include <Core/Global.hpp>

/**
 * @brief Provides random numbers via the SIMD-oriented Fast Mersenne Twister algorithm.
 */
class SFMT
{
public:
    /**
     * @brief Creates a new SFMT
     * @param seed Starting PRNG state
     */
    SFMT(u32 seed);

    /**
     * @brief Advances the RNG by provided amount
     * @param advances Number of advances
     */
    void advance(u32 advances);

    /**
     * @brief Gets the next 64bit PRNG state
     * @return PRNG value
     */
    u64 next();

    /**
     * @brief Gets the next 32bit PRNG state
     * @return PRNG value
     */
    u32 nextUInt();

private:
    u16 index;
    alignas(16) u32 sfmt[624];

    /**
     * @brief Generates the next SFMT state after all 624 states have been consumed
     */
    void shuffle();
};

#endif // SFMT_HPP
