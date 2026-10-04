#ifndef RESULTTOGENERATOR_HPP
#define RESULTTOGENERATOR_HPP

#include <Core/Enum/Lead.hpp>
#include <Core/Parents/States/State.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <bit>

namespace ResultToGenerator5
{
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
