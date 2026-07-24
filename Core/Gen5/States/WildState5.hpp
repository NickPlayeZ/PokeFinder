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

#ifndef WILDSTATE5_HPP
#define WILDSTATE5_HPP

#include <Core/Enum/Lead.hpp>
#include <Core/Parents/States/WildState.hpp>

/**
 * @brief State class for Gen5 static generator encounters
 */
class WildState5 : public WildGeneratorState
{
public:
    /**
     * @brief Construct a new State5 object
     *
     * @param prng PRNG call to determine chatot pitch and needle
     * @param movingTrigger Moving battle trigger ratio
     * @param movingSteps Movement steps needed to trigger an encounter
     * @param advances Advances of the state
     * @param ivAdvances IV advances of the state
     * @param pid Pokemon PID
     * @param ivs Pokemon IVs
     * @param ability Pokemon ability
     * @param gender Pokemon gender
     * @param level Pokemon level
     * @param nature Pokemon nature
     * @param shiny Pokemon shininess
     * @param info Pokemon information
     * @param valid State can be hit
     */
    WildState5(u32 prng, u8 movingTrigger, u8 movingSteps, bool phenomenon, bool phenomenonItem, u32 advances, u32 ivAdvances, u32 pid,
               const std::array<u8, 6> &ivs, u8 ability, u8 gender, u8 level, u8 nature, u8 shiny, u8 encounterSlot, u16 item, u16 specie,
               u8 form, const PersonalInfo *info, bool valid = true, u8 passPower = 0, Lead lead = Lead::None) :
        WildGeneratorState(advances, pid, ivs, ability, gender, level, nature, shiny, encounterSlot, item, specie, form, info, valid),
        ivAdvances(ivAdvances),
        passPower(passPower),
        movingTrigger(movingTrigger),
        movingSteps(movingSteps),
        phenomenon(phenomenon),
        phenomenonItem(phenomenonItem),
        valid(valid),
        chatot(static_cast<u8>(((static_cast<u64>(prng) * 0x1fff) >> 32) / 82)),
        needle(static_cast<u8>((static_cast<u64>(prng) * 8) >> 32)),
        lead(lead),
        leadFlags(getLeadFlag(lead)),
        variableNature(false)
    {
    }

    u8 getChatot() const
    {
        return chatot;
    }

    u32 getIVAdvances() const
    {
        return ivAdvances;
    }

    u8 getMovingTrigger() const
    {
        return movingTrigger;
    }

    u8 getMovingSteps() const
    {
        return movingSteps;
    }

    bool isValid() const
    {
        return valid;
    }

    bool getPhenomenon() const
    {
        return phenomenon;
    }

    bool getPhenomenonItem() const
    {
        return phenomenonItem;
    }

    u8 getNeedle() const
    {
        return needle;
    }

    u8 getPassPower() const
    {
        return passPower;
    }

    Lead getLead() const
    {
        return lead;
    }

    u8 getLeadFlags() const
    {
        return leadFlags;
    }

    void addLead(Lead newLead)
    {
        if (newLead == Lead::None)
        {
            lead = Lead::None;
            leadFlags = getLeadFlag(Lead::None);
        }
        else if (lead != Lead::None)
        {
            leadFlags |= getLeadFlag(newLead);
        }
    }

    void setVariableNature(bool flag)
    {
        variableNature = flag;
    }

    bool getVariableNature() const
    {
        return variableNature;
    }

private:
    static constexpr u8 getLeadFlag(Lead lead)
    {
        if (lead <= Lead::SynchronizeEnd)
        {
            return 1 << 0;
        }

        switch (lead)
        {
        case Lead::CuteCharmM:
            return 1 << 1;
        case Lead::CuteCharmF:
            return 1 << 2;
        case Lead::MagnetPull:
            return 1 << 3;
        case Lead::Static:
            return 1 << 4;
        case Lead::Pressure:
            return 1 << 5;
        case Lead::CompoundEyes:
            return 1 << 6;
        case Lead::SuctionCups:
        case Lead::ArenaTrap:
            return 1 << 7;
        default:
            return 0;
        }
    }

    u32 ivAdvances;
    u8 passPower;
    u8 movingTrigger;
    u8 movingSteps;
    bool phenomenon;
    bool phenomenonItem;
    bool valid;
    u8 chatot;
    u8 needle;
    Lead lead;
    u8 leadFlags;
    bool variableNature;
};

#endif // WILDSTATE5_HPP
