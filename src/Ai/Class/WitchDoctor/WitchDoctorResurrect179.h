// WD179A: native player-corpse selection and normal Reclaim Soul casts only.
#ifndef PLAYERBOTS_REBORN_WITCHDOCTOR_RESURRECT179_H
#define PLAYERBOTS_REBORN_WITCHDOCTOR_RESURRECT179_H
#include "PartyMemberToResurrect.h"
#include "PlayerbotAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "../../../../../mod-reborn-witchdoctor/src/RebornWitchDoctorBotState168.h"
namespace RebornWD179
{
inline bool Eligible(Player* b,Player* p,SpellInfo const* spell)
{
 return b&&p&&spell&&spell->IsAllowingDeadTarget()&&p!=b&&b->GetGroup()&&b->IsInRaidWith(p)&&!p->IsAlive()&&p->IsInWorld()&&
  b->IsInMap(p)&&b->InSamePhase(p)&&b->_IsValidAssistTarget(p,spell)&&!p->IsBeingTeleported()&&
  p->getDeathState()==DeathState::Corpse&&!p->isResurrectRequested()&&b->IsWithinDistInMap(p,55.f);
}
class Candidate final:public PartyMemberToResurrect
{
 Player* caster;SpellInfo const* spell;
 bool Check(Unit* u)override
 {
  auto* p=u?u->ToPlayer():nullptr;
  // The generic assist check rejects every dead unit; use the real resurrection spell.
  return Eligible(caster,p,spell)&&PartyMemberValue::Check(u);
 }
public:
 Candidate(PlayerbotAI* ai,SpellInfo const* info):PartyMemberToResurrect(ai),caster(ai->GetBot()),spell(info){}
 using PartyMemberToResurrect::Calculate;
};
template<class Offer>inline auto Choose(PlayerbotAI* ai,WD168::Snapshot const& s,Offer offer)
{
 auto empty=offer(0,nullptr);auto* b=ai->GetBot();auto const& spell=s.spells[4];
 if(!spell.learned||spell.replacement||!b->GetGroup())return empty;
 // Fresh native selection excludes pending requests and someone else's current
 // resurrection casts. No cached corpse/Aura pointer survives to the next tick.
 auto* info=sSpellMgr->GetSpellInfo(spell.learned);if(!info||!info->IsAllowingDeadTarget())return empty;
 Candidate finder(ai,info);Unit* target=finder.Calculate();
 if(!target||!Eligible(b,target->ToPlayer(),info))return empty;
 if(auto c=offer(spell.learned,target);c.spell){c.reason="party_reclaim_soul";return c;}
 return empty;
}
}
#endif
