// WD193: bounded group mobility support using the existing exclusive idol slot.
#ifndef PLAYERBOTS_WD_IDOL_193_H
#define PLAYERBOTS_WD_IDOL_193_H
#include "WitchDoctorTeam171.h"
namespace RebornWD193
{
template<class Can>inline RebornWD171::Decision Swift(Player* b,WD168::Snapshot const& s,RebornWD171::Scene const& scene,bool forced,Can can)
{
 // Only use the already selected Swift idol: native Track remembers each cast.
 uint32 selected=s.summons[1].selected;
 if(forced||s.summons[1].present||scene.swiftMoving<2||scene.critical||(scene.uncoveredHealing&&scene.injured>=2)||scene.uncoveredMana||scene.dispelNeeded||
    selected!=9003432||
    !s.maxMana||s.mana*100ull<s.maxMana*50ull)return {};
 if(can(9003432,b))return {9003432,b,"moving_group_swift_idol"};
 return {};
}
}
#endif
