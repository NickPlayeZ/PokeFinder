#ifndef RESULTTOGENERATOR_HPP
#define RESULTTOGENERATOR_HPP

#include <Core/Gen5/Profile5.hpp>
#include <Core/Util/Utilities.hpp>

namespace ResultToGenerator5
{
inline u32 getMaxAdvances(u64 seed, u32 targetAdvance, u32 padding, const Profile5 &profile)
{
    u32 initialAdvance = Utilities5::initialAdvances(seed, profile);
    return (targetAdvance >= initialAdvance ? targetAdvance - initialAdvance : 0) + padding;
}

}

#endif // RESULTTOGENERATOR_HPP
