// WD190A: bounded learned cooldowns. Native costs, auras and spell hooks own effects.
#ifndef PLAYERBOTS_WD_COOLDOWN_190_H
#define PLAYERBOTS_WD_COOLDOWN_190_H
#include "WitchDoctorTeam171.h"
namespace RebornWD190
{
template<class Can>inline RebornWD171::Decision Survival(Player* b,Can can)
{
 if(b->IsInCombat()&&b->GetHealthPct()<50.f&&!b->HasAura(9003855)&&
    !b->HasAura(9003581)&&!b->HasAura(9003800)&&can(9003855,b))
  return {9003855,b,"vigil_self_preservation"};
 return {};
}
template<class Can>inline RebornWD171::Decision Burst(Player* b,Unit* enemy,WD168::Snapshot const& s,RebornWD171::Scene const& scene,bool movementLocked,Can can)
{
 // An eight-second root is a commitment: reserve it for a healthy ranged caster
 // fighting a durable target with a group, never while forced movement is active.
 if(s.plan.spec!=0||!enemy||!b->GetGroup()||b->isMoving()||movementLocked||
    b->GetHealthPct()<80.f||scene.critical||!b->getAttackers().empty()||
    enemy->GetHealthPct()<70.f||uint64(enemy->GetMaxHealth())<uint64(b->GetMaxHealth())*4||
    !b->IsWithinDistInMap(enemy,30.f)||b->IsWithinDistInMap(enemy,10.f)||!b->IsWithinLOSInMap(enemy)||
    !s.maxMana||s.mana*100ull<s.maxMana*50ull||s.mimicPresent||
    b->HasAura(9003810)||b->HasAura(9003812)||b->HasAura(9003581)||
    b->HasAura(9003821)||b->HasAura(9004163)||
    b->HasAura(9003859)||b->HasAura(9003853)||b->HasAura(9004110))return {};
 if(can(9003810,b))return {9003810,b,"marionette_durable_target"};
 return {};
}
}
#endif
