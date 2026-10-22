// WD203: instant or short native casts with a genuine remaining enemy cast window.
#ifndef PLAYERBOTS_WD_CONTROL_196_H
#define PLAYERBOTS_WD_CONTROL_196_H
#include "WitchDoctorTeam171.h"
#include "Spell.h"
#include "RebornWitchDoctorControl196.h"
namespace RebornWD196
{
inline bool Window(Unit* u,uint32 castMs)
{
 if(!u)return false;
 if(!castMs)return u->IsNonMeleeSpellCast(false,false,true);
 for(auto slot:{CURRENT_GENERIC_SPELL,CURRENT_CHANNELED_SPELL})if(Spell* cast=u->GetCurrentSpell(slot))
  if((cast->getState()==SPELL_STATE_CASTING||(cast->getState()==SPELL_STATE_PREPARING&&cast->GetCastTime()>0))&&
     cast->GetTimer()>int32(castMs+500))return true;
 return false;
}
template<class Can>inline RebornWD171::Decision Control(Player* b,Unit* primary,
 RebornWD171::Scene const& scene,bool forced,Can can)
{
 if(forced||scene.critical||scene.dispelNeeded||scene.uncoveredHealing||scene.enemies<2||
    !primary||!b->HasSpell(9003861)||!WD196::Bound())return {};
 auto* info=sSpellMgr->GetSpellInfo(9003861);
 if(!info)return {};
 uint32 castMs=info->CalcCastTime(b);
 if(castMs>1500||(castMs&&(b->isMoving()||b->GetHealthPct()<80.f||!b->getAttackers().empty()||scene.injured||scene.lowMana)))return {};
 float clearance=info->Effects[EFFECT_0].CalcRadius(b)+4.f;
 for(Unit* u:scene.hostiles)
 {
  if(u==primary||!WD196::Safe(b,u)||!Window(u,castMs)||
     primary->IsWithinDistInMap(u,clearance))continue;
  bool focus=false;
  for(Unit* member:scene.members)if(member&&member->GetVictim()==u){focus=true;break;}
  if(!focus&&can(9003861,u))return {9003861,u,castMs?"short_cast_frog_offtarget_caster":"instant_frog_offtarget_caster"};
 }
 return {};
}
}
#endif
