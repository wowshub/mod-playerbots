// WD204C: native caster-centred cone geometry, private class adapter only.
#ifndef PLAYERBOTS_WD_GLAIVE_204_H
#define PLAYERBOTS_WD_GLAIVE_204_H
#include "WitchDoctorTeam171.h"
namespace RebornWD204
{
inline bool GlaiveFamily(uint32 id){return id>=9004030&&id<=9004036;}
inline float GlaiveRange(Player* b,uint32 id)
{
 auto* info=sSpellMgr->GetSpellInfo(id);
 if(!GlaiveFamily(id)||!info||info->Effects[0].TargetA.GetTarget()!=TARGET_UNIT_CONE_ENEMY_24||
    info->Effects[2].TargetA.GetTarget()!=TARGET_UNIT_CONE_ENEMY_24)return 0.f;
 return std::max(0.f,info->Effects[0].CalcRadius(b));
}
}
#endif
