/*
 * This file is part of PokéFinder
 * Copyright (C) 2017-2024 by Admiral_Fish, bumba, and EzPzStreamz
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

#include "IDSearcher5.hpp"
#include <Core/RNG/SHA1.hpp>

std::vector<SearcherState5<IDState>> IDSearcher5::search(const IDGenerator5 &generator, const DateTime &dateTime, int seconds, u16 timer0Min,
                                                         u16 timer0Max)
{
#ifdef ENABLE_SIMD
    if (hasSHA())
    {
        SHA1SIMD sha(profile);
        for (int offset = -seconds; offset <= seconds; offset++)
        {
            DateTime dt = dateTime.addSeconds(offset);
            const Date &date = dt.getDate();
            const Time &time = dt.getTime();
            sha.setDate(date);

            for (u32 timer0 = timer0Min; timer0 <= timer0Max; timer0++)
            {
                sha.setTimer0(static_cast<u16>(timer0), profile.getVCount());
                for (const auto &keypress : keypresses)
                {
                    sha.setButton(keypress.value);
                    sha.setTime(time.hour(), time.minute(), time.second(), profile.getDSType());
                    u64 seed = sha.hashSeed();

                    auto states = generator.generate(seed);
                    if (!states.empty())
                    {
                        results.reserve(results.capacity() + states.size());
                        for (const auto &state : states)
                        {
                            results.emplace_back(dt, seed, keypress.button, static_cast<u16>(timer0), state);
                        }
                    }
                }
            }
        }
    }
    else
#endif
    {
        SHA1 sha(profile);
        for (int offset = -seconds; offset <= seconds; offset++)
        {
            DateTime dt = dateTime.addSeconds(offset);
            const Date &date = dt.getDate();
            const Time &time = dt.getTime();
            sha.setDate(date);

            for (u32 timer0 = timer0Min; timer0 <= timer0Max; timer0++)
            {
                sha.setTimer0(static_cast<u16>(timer0), profile.getVCount());
                auto beta = sha.precompute();
                for (const auto &keypress : keypresses)
                {
                    sha.setButton(keypress.value);
                    sha.setTime(time.hour(), time.minute(), time.second(), profile.getDSType());
                    u64 seed = sha.hashSeed(beta);

                    auto states = generator.generate(seed);
                    if (!states.empty())
                    {
                        results.reserve(results.capacity() + states.size());
                        for (const auto &state : states)
                        {
                            results.emplace_back(dt, seed, keypress.button, static_cast<u16>(timer0), state);
                        }
                    }
                }
            }
        }
    }

    return results;
}
