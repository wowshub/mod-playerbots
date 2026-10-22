// WD197: native interrupt eligibility for an already learned Spirit Shock.
#ifndef PLAYERBOTS_WD_INTERRUPT_197_H
#define PLAYERBOTS_WD_INTERRUPT_197_H
#include "WitchDoctorTeam171.h"
#include "Spell.h"
namespace RebornWD197
{
inline bool Interruptible(Unit* u,SpellInfo const* shock)
{
 if(!u||u->GetTypeId()==TYPEID_PLAYER||!u->IsAlive()||!shock||u->IsImmunedToSpell(shock)||u->IsImmunedToSpellEffect(shock,EFFECT_1))return false;
 for(auto slot:{CURRENT_GENERIC_SPELL,CURRENT_CHANNELED_SPELL})
  if(Spell* cast=u->GetCurrentSpell(slot))
  {
   auto* info=cast->GetSpellInfo();
   if(info&&(cast->getState()==SPELL_STATE_CASTING||(cast->getState()==SPELL_STATE_PREPARING&&cast->GetCastTime()>0))&&
      info->PreventionType==SPELL_PREVENTION_TYPE_SILENCE&&
      ((slot==CURRENT_GENERIC_SPELL&&(info->InterruptFlags&SPELL_INTERRUPT_FLAG_INTERRUPT))||
       (slot==CURRENT_CHANNELED_SPELL&&(info->ChannelInterruptFlags&CHANNEL_INTERRUPT_FLAG_INTERRUPT))))return true;
  }
 return false;
}
template<class Legal,class Can>inline RebornWD171::Decision Choose(Player* b,Unit* primary,RebornWD171::Scene const& scene,bool forced,Legal legal,Can can)
{
 if(forced||scene.critical||!b->HasSpell(9004162))return {};
 auto* spell=sSpellMgr->GetSpellInfo(9004162);
 auto attempt=[&](Unit* u)->RebornWD171::Decision{
  if(legal(u)&&Interruptible(u,spell)&&can(9004162,u))return {9004162,u,"spirit_shock_interrupt"};return {};};
 if(auto d=attempt(primary);d.spell)return d;
 for(auto* u:scene.hostiles)if(u!=primary)if(auto d=attempt(u);d.spell)return d;
 return {};
}
}
#endif
