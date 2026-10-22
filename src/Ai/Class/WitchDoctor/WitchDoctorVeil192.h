// WD192: learned Veil on a durable engaged target; normal native cast only.
#ifndef PLAYERBOTS_WD_VEIL_192_H
#define PLAYERBOTS_WD_VEIL_192_H
#include "WitchDoctorTeam171.h"
namespace RebornWD192
{
template<class Can>inline RebornWD171::Decision Veil(Player* b,Unit* enemy,WD168::Snapshot const& s,bool movementLocked,Can can)
{
 if(!enemy||b->isMoving()||movementLocked||enemy->GetHealthPct()<60.f||
    uint64(enemy->GetMaxHealth())<uint64(b->GetMaxHealth())*2||
    enemy->HasAura(9003962,b->GetGUID())||!s.maxMana||
    s.mana*100ull<s.maxMana*(s.plan.spec==1?70ull:40ull)||
    b->HasAura(9003821)||b->HasAura(9004163)||b->HasAura(9004175))return {};
 if(can(9003962,enemy))return {9003962,enemy,"durable_target_veil"};
 return {};
}
}
#endif
