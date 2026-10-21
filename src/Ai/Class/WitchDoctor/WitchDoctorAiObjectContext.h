// WD169A: local Class13 adapter for the existing GPLv2 Playerbots engine.
// Inspired by Zyth45 CoaGenericContext (coa 3b5a29b0); local private IDs and saved builds.
#ifndef PLAYERBOTS_REBORN_WITCHDOCTOR_CONTEXT_H
#define PLAYERBOTS_REBORN_WITCHDOCTOR_CONTEXT_H
#include "AiObjectContext.h"
#include "Action.h"
#include "PlayerbotAI.h"
#include "Player.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "CharmInfo.h"
#include <memory>
#include "../../../../../mod-reborn-witchdoctor/src/RebornWitchDoctorBotState168.h"
namespace RebornWD169
{
inline bool MemberTarget(Player* bot,Unit* u)
{
 return u&&u->IsAlive()&&u->IsInWorld()&&bot->IsInMap(u)&&bot->InSamePhase(u)&&
  bot->IsValidAssistTarget(u)&&bot->IsWithinDistInMap(u,40.f)&&bot->IsWithinLOSInMap(u);
}
inline bool Engaged(Player* b,Unit* u,Player* master)
{
 if(!u||!u->IsAlive()||!u->IsInWorld()||!b->IsInMap(u)||!b->InSamePhase(u)||
    !b->IsValidAttackTarget(u)||u->HasBreakableByDamageCrowdControlAura())return false;
 if(u->GetTypeId()==TYPEID_PLAYER&&b->IsInRaidWith(u))return false;
 if(u->HasAuraType(SPELL_AURA_REFLECT_SPELLS)||u->HasAuraType(SPELL_AURA_REFLECT_SPELLS_SCHOOL))return false;
 if(b->IsInCombatWith(u))return true;
 if(master&&master->IsAlive()&&b->IsInMap(master)&&b->InSamePhase(master)&&master->IsInCombatWith(u))return true;
 if(auto* group=b->GetGroup())for(auto* ref=group->GetFirstMember();ref;ref=ref->next())
  if(auto* ally=ref->GetSource())if(ally->IsAlive()&&b->IsInMap(ally)&&b->InSamePhase(ally)&&ally->IsInCombatWith(u))return true;
 return false;
}
// Playerbots' native CanCastSpell intentionally probes with power cost ignored.
// As in the upstream CoA power gate, check actual resources separately.
inline bool CanUse(PlayerbotAI* ai,uint32 spell,Unit* target)
{
 auto* b=ai->GetBot();if(!spell||!target||!b->HasSpell(spell))return false;
 auto* info=sSpellMgr->GetSpellInfo(spell);if(!info||b->HasSpellCooldown(spell)||b->GetGlobalCooldownMgr().HasGlobalCooldown(info))return false;
 if(info->PowerType<MAX_POWERS)
 {int32 cost=info->CalcPowerCost(b,info->GetSchoolMask());if(cost>0&&int32(b->GetPower(Powers(info->PowerType)))<cost)return false;}
 if(b!=target)
 {float max=info->GetMaxRange(info->IsPositive(),b);if(max<=0||!b->IsWithinCombatRange(target,max)||!b->IsWithinLOSInMap(target))return false;
  float min=info->GetMinRange(info->IsPositive());if(min>0&&b->IsWithinCombatRange(target,min))return false;}
 return ai->CanCastSpell(spell,target,true);
}
struct Choice{uint32 spell=0;Unit* target=nullptr;bool damage=false;};
inline Choice Choose(PlayerbotAI* ai)
{
 Player* b=ai->GetBot();auto s=WD168::Capture(b);
 if(!s.plan.available||!s.alive||!s.inWorld||b->IsBeingTeleported()||(b->IsNonMeleeSpellCast(false,false,true)||b->HasUnitState(UNIT_STATE_LOST_CONTROL))||!b->GetCharmerGUID().IsEmpty())return {};
 // Triage uses the actual saved Brewing spec; DPS only emergency self-heals.
 bool healer=s.plan.spec==1,healingNeeded=false;Unit* low=nullptr;float health=healer?85.f:40.f;
 auto consider=[&](Unit* u){if(MemberTarget(b,u)&&u->GetHealthPct()<health){healingNeeded=true;if(CanUse(ai,s.spells[1].learned,u)){health=u->GetHealthPct();low=u;}}};
 consider(b);
 if(healer)if(auto* group=b->GetGroup())for(auto* ref=group->GetFirstMember();ref;ref=ref->next())consider(ref->GetSource());
 auto can=[&](uint32 spell,Unit* u){return CanUse(ai,spell,u);};
 if(low&&can(s.spells[1].learned,low))return {s.spells[1].learned,low,false};
 if(healer&&healingNeeded)return {}; // Do not spend DPS mana while an ally still needs a blocked heal.
 Unit* enemy=nullptr;
 if(auto* value=ai->GetAiObjectContext()->GetValue<Unit*>("current target"))enemy=value->Get();
 Player* master=ai->GetMaster();
 if(!Engaged(b,enemy,master)&&master)enemy=master->GetSelectedUnit();
 if(!Engaged(b,enemy,master))return {};
 // No periodic resummon: regular ward ownership is read from the original slot.
 if(!s.serpentPresent&&can(s.spells[3].learned,b))return {s.spells[3].learned,b,false};
 uint32 attack=b->HasSpell(9003968)?s.spells[5].learned:s.spells[0].learned;
 if(can(attack,enemy))return {attack,enemy,true};
 return {};
}
class TickAction:public Action
{
 PlayerbotAI* ai;
public:
 explicit TickAction(PlayerbotAI* a):Action(a,"wd169 tick"),ai(a){}
 bool isUseful()override{return Choose(ai).spell!=0;}
 bool Execute(Event)override
 {
  auto next=Choose(ai); // Re-evaluate after any previous action or snapshot change.
  if(!next.spell||!next.target)return false;
  if(next.damage&&!Engaged(ai->GetBot(),next.target,ai->GetMaster()))return false;
  if(!CanUse(ai,next.spell,next.target))return false;
  return ai->CastSpell(next.spell,next.target);
 }
};
class SpecStrategy:public Strategy
{
 PlayerbotAI* ai;std::string name;
public:
 SpecStrategy(PlayerbotAI* a,std::string n):Strategy(a),ai(a),name(std::move(n)){}
 std::string const getName()override{return name;}
 uint32 GetType()const override
 {auto p=WD168::ReadPlan(ai->GetBot());return STRATEGY_TYPE_RANGED|(p.available&&p.spec==1?STRATEGY_TYPE_HEAL:STRATEGY_TYPE_DPS);}
 std::vector<NextAction> getDefaultActions()override{return {NextAction("wd169 tick",ACTION_HIGH)};}
};
class Context:public AiObjectContext
{
 PlayerbotAI* ai;std::unique_ptr<TickAction> tick;
 std::unique_ptr<SpecStrategy> combat,support;
public:
 explicit Context(PlayerbotAI* a):AiObjectContext(a),ai(a){}
 Strategy* GetStrategy(std::string const name)override
 {
  if(name=="wd169 combat"){if(!combat)combat=std::make_unique<SpecStrategy>(ai,name);return combat.get();}
  if(name=="wd169 support"){if(!support)support=std::make_unique<SpecStrategy>(ai,name);return support.get();}
  // Claude's fallback is retained for other classes; Class13 uses its learned-spell strategy.
  if(name=="dc selfbot basic")return nullptr;
  return AiObjectContext::GetStrategy(name);
 }
 Action* GetAction(std::string const name)override
 {if(name=="wd169 tick"){if(!tick)tick=std::make_unique<TickAction>(ai);return tick.get();}return AiObjectContext::GetAction(name);}
};
}
#endif
