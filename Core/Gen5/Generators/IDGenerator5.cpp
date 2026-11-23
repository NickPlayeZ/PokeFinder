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

#include "IDGenerator5.hpp"
#include <Core/Parents/States/IDState.hpp>
#include <Core/Enum/Game.hpp>
#include <Core/Gen5/WhiteForestData.hpp>
#include <Core/RNG/LCRNG64.hpp>
#include <Core/Util/Utilities.hpp>
#include <algorithm>

IDGenerator5::IDGenerator5(u32 initialAdvances, u32 maxAdvances, u32 pid, bool checkPID, bool checkXOR, const Profile5 &profile,
                           const IDFilter &filter, const std::vector<u8> &residents, const std::vector<u8> &entralinkResidents,
                           bool generateResidents) :
    IDGenerator(initialAdvances, maxAdvances, filter), profile(profile), pid(pid), checkPID(checkPID), checkXOR(checkXOR),
    residents(residents), entralinkResidents(entralinkResidents), generateResidents(generateResidents)
{
}

std::vector<IDState> IDGenerator5::generate(u64 seed) const
{
    bool pidBit = (pid >> 31) ^ (pid & 1);
    u16 psv = (pid >> 16) ^ (pid & 0xffff);

    u32 advances = Utilities5::initialAdvancesID(seed, profile.getVersion());
    BWRNG rng(seed, advances + initialAdvances);

    std::vector<IDState> states;
    for (u32 cnt = 0; cnt <= maxAdvances; cnt++)
    {
        u32 rand = rng.nextUInt(0xffffffff);
        u16 tid = rand & 0xffff;
        u16 sid = rand >> 16;
        u16 tsv = (tid ^ sid) >> 3;

        IDState state(advances + initialAdvances + cnt, tid, sid, tsv);
        if (filter.compare(state))
        {
            bool shiny = (psv >> 3) == state.getTSV();

            // Check if PID is possible with TID/SID combo if Static/Wild box is checked
            if (shiny && checkXOR) // We need to do the check only if it was shiny first
            {
                bool idbit = (tid & 1) ^ (sid & 1);
                shiny = idbit == pidBit;
            }

            if (!checkPID || shiny)
            {
                std::array<u8, 10> generatedResidents {};
                std::array<u8, 3> generatedEntralinkResidents {};
                if (generateResidents && (profile.getVersion() & Game::BW) != Game::None)
                {
                    BWRNG residentRng = rng;
                    Utilities5::advanceBWIDRNGToFieldInit(residentRng);
                    std::array<bool, 30> used {};
                    u32 remainingWeight = 462;
                    for (u8 draw = 0; draw < 13; draw++)
                    {
                        u32 roll = residentRng.nextUInt(remainingWeight);
                        u32 cumulative = 0;
                        u8 resident = 0;
                        for (; resident < 30; resident++)
                        {
                            if (!used[resident])
                            {
                                cumulative += WhiteForest5::initialWeights[resident];
                                if (roll < cumulative)
                                {
                                    break;
                                }
                            }
                        }

                        if (resident >= 30)
                        {
                            break;
                        }
                        used[resident] = true;
                        remainingWeight -= WhiteForest5::initialWeights[resident];
                        if (draw < 10)
                        {
                            generatedResidents[draw] = resident;
                        }
                        else
                        {
                            generatedEntralinkResidents[draw - 10] = resident;
                        }
                    }
                }

                bool residentsMatch = std::ranges::all_of(residents, [&](u8 resident) {
                    return std::ranges::contains(generatedResidents, resident);
                });
                bool entralinkResidentsMatch = std::ranges::all_of(entralinkResidents, [&](u8 resident) {
                    return std::ranges::contains(generatedEntralinkResidents, resident);
                });
                if (residentsMatch && entralinkResidentsMatch)
                {
                    states.emplace_back(advances + initialAdvances + cnt, tid, sid, tsv, generatedResidents,
                                        generatedEntralinkResidents);
                }
            }
        }
    }

    return states;
}
