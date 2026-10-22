// WD200: learned Fool's Play on a durable engaged PvE target, native cast only.
#ifndef PLAYERBOTS_WD_FOOL_200_H
#define PLAYERBOTS_WD_FOOL_200_H
#include "WitchDoctorTeam171.h"
namespace RebornWD200
{
template<class Can>inline RebornWD171::Decision Fool(Player* b,Unit* enemy,WD168::Snapshot const& s,RebornWD171::Scene const& scene,bool forced,Can can)
{
 if(s.plan.spec!=2||forced||b->isMoving()||!enemy||enemy->GetTypeId()==TYPEID_PLAYER||
    !b->HasSpell(9004181)||b->HasAura(9004186,b->GetGUID())||b->GetHealthPct()<70.f||
    scene.critical||scene.injured||scene.dispelNeeded||enemy->GetHealthPct()<70.f||
    uint64(enemy->GetMaxHealth())<uint64(b->GetMaxHealth())*4||
    !s.maxMana||s.mana*100ull<s.maxMana*60ull||b->HasAura(9004163)||b->HasAura(9004175))return {};
 if(can(9004181,enemy))return {9004181,enemy,"fool_durable_target"};
 return {};
}
}
#endif
