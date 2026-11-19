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

#ifndef IDSEARCHER5_HPP
#define IDSEARCHER5_HPP

#include <Core/Gen5/Generators/IDGenerator5.hpp>
#include <Core/Gen5/Searchers/Searcher5.hpp>
#include <Core/Gen5/States/SearcherState5.hpp>
#include <Core/Parents/States/IDState.hpp>

/**
 * @brief TID/SID searcher for Gen5
 */
class IDSearcher5 final : public Searcher5<IDGenerator5, IDState>
{
public:
    /**
     * @brief Construct a new IDSearcher5 object
     *
     * @param generator ID state generator
     * @param profile Profile information
     */
    IDSearcher5(const IDGenerator5 &generator, const Profile5 &profile) : Searcher5(generator, profile)
    {
    }

    /**
     * @brief Searches specified date/time range for filtered matches
     *
     * @param generator State generator
     * @param dateTime Target date/time
     * @param seconds Seconds to search before and after the target
     * @param timer0Min Minimum Timer0
     * @param timer0Max Maximum Timer0
     *
     * @return Vector of computed states
     */
    std::vector<SearcherState5<IDState>> search(const IDGenerator5 &generator, const DateTime &dateTime, int seconds, u16 timer0Min,
                                                u16 timer0Max);
};

#endif // IDSEARCHER5_HPP
