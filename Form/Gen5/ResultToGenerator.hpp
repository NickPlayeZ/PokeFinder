#ifndef RESULTTOGENERATOR_HPP
#define RESULTTOGENERATOR_HPP

#include <Core/Enum/Lead.hpp>
#include <Core/Gen5/Profile5.hpp>
#include <Core/Parents/States/State.hpp>
#include <Core/Util/Utilities.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <bit>

namespace ResultToGenerator5
{
inline u32 getMaxAdvances(u64 seed, u32 targetAdvance, u32 padding, const Profile5 &profile)
{
    u32 initialAdvance = Utilities5::initialAdvances(seed, profile);
    return (targetAdvance >= initialAdvance ? targetAdvance - initialAdvance : 0) + padding;
}

inline void setPreferredLead(ComboMenu *comboMenu, u64 leadMask, u8 nature)
{
    if (leadMask == 0 || (leadMask & getLeadFlag(Lead::None)) != 0)
    {
        comboMenu->setCheckedData({ toInt(Lead::None) });
        return;
    }

    constexpr u64 synchronizeMask = (1ULL << 25) - 1;
    if ((leadMask & synchronizeMask) != 0)
    {
        u8 selectedNature = nature;
        if ((leadMask & getLeadFlag(static_cast<Lead>(selectedNature))) == 0)
        {
            selectedNature = static_cast<u8>(std::countr_zero(leadMask & synchronizeMask));
        }
        comboMenu->setCheckedData({ selectedNature });
        return;
    }

    constexpr Lead order[] = { Lead::CuteCharmF, Lead::CuteCharmM, Lead::ArenaTrap, Lead::CompoundEyes, Lead::FlashFire,
                               Lead::Harvest, Lead::Intimidate, Lead::KeenEye, Lead::MagnetPull, Lead::Pressure,
                               Lead::RockSmashMagnetPull, Lead::RockSmashSuctionCups, Lead::RockSmashSuperLuck,
                               Lead::SereneGrace, Lead::Static, Lead::StormDrain, Lead::SuctionCups };
    for (Lead lead : order)
    {
        if ((leadMask & getLeadFlag(lead)) != 0)
        {
            comboMenu->setCheckedData({ toInt(lead) });
            return;
        }
    }
}
}

#endif // RESULTTOGENERATOR_HPP
