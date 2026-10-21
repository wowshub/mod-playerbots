// WD169A: local Class13 adapter for the existing GPLv2 Playerbots engine.
// Inspired by Zyth45 CoaGenericContext (coa 3b5a29b0); local private IDs and saved builds.
#ifndef PLAYERBOTS_REBORN_WITCHDOCTOR_CONTEXT_H
#define PLAYERBOTS_REBORN_WITCHDOCTOR_CONTEXT_H
#include "AiObjectContext.h"
#include "Action.h"
#include "MovementActions.h"
#include "WitchDoctorEquipment170.h"
#include "WitchDoctorTeam171.h"
#include "WitchDoctorVoodoo175.h"
#include "Timer.h"
#include "Log.h"
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
 if(Unit* victim=u->GetVictim())if(RebornWD171::Member(b,victim)&&victim->IsInCombatWith(u))return true;
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
// Readiness without the movement/range rejection. The native spell engine remains the final authority.
inline bool Prepared(PlayerbotAI* ai,uint32 spell)
{
 auto* b=ai->GetBot();if(!spell||!b->HasSpell(spell))return false;
 auto* info=sSpellMgr->GetSpellInfo(spell);if(!info||b->HasSpellCooldown(spell)||b->GetGlobalCooldownMgr().HasGlobalCooldown(info))return false;
 if(info->PowerType<MAX_POWERS)
 {int32 cost=info->CalcPowerCost(b,info->GetSchoolMask());if(cost>0&&int32(b->GetPower(Powers(info->PowerType)))<cost)return false;}
 return true;
}
struct Choice{uint32 spell=0;Unit* target=nullptr;bool damage=false;bool reposition=false;char const* reason="rotation";};
inline Choice Offer(PlayerbotAI* ai,uint32 spell,Unit* target,bool damage)
{
 if(!target||!Prepared(ai,spell))return {};
 if(CanUse(ai,spell,target))return {spell,target,damage,false};
 auto* b=ai->GetBot();auto* info=sSpellMgr->GetSpellInfo(spell);
 bool geometry=b!=target&&(!b->IsWithinLOSInMap(target)||!b->IsWithinCombatRange(target,info->GetMaxRange(info->IsPositive(),b))||
  (info->GetMinRange(info->IsPositive())>0&&b->IsWithinCombatRange(target,info->GetMinRange(info->IsPositive()))));
 if(geometry||(b->isMoving()&&info->CalcCastTime(b)>0))return {spell,target,damage,true};
 return {};
}
inline bool PartyCombat(PlayerbotAI* ai)
{
 auto* b=ai->GetBot();if(b->IsInCombat())return true;
 auto busy=[&](Player* p){return p&&p->IsInWorld()&&b->IsInMap(p)&&b->InSamePhase(p)&&p->IsInCombat();};
 if(busy(ai->GetMaster()))return true;
 if(auto* g=b->GetGroup())for(auto* r=g->GetFirstMember();r;r=r->next())if(busy(r->GetSource()))return true;
 return RebornWD171::NPCCombat(b);
}
inline bool Occupied(WD168::Snapshot const& s)
{for(auto const& slot:s.summons)if(slot.present)return true;return false;}
inline Choice Choose(PlayerbotAI* ai)
{
 Player* b=ai->GetBot();auto s=WD168::Capture(b);
 if(!s.plan.available||!s.alive||!s.inWorld||b->IsBeingTeleported()||(b->IsNonMeleeSpellCast(false,false,true)||b->HasUnitState(UNIT_STATE_LOST_CONTROL))||!b->GetCharmerGUID().IsEmpty())return {};
 bool partyCombat=PartyCombat(ai);
 auto scene=RebornWD171::Observe(b,partyCombat,[&](Unit* u){return Engaged(b,u,ai->GetMaster());});
 bool healer=s.plan.spec==1,healingNeeded=false;Choice heal;float health=healer?85.f:40.f;
 auto consider=[&](Unit* u){if(MemberTarget(b,u)&&u->GetHealthPct()<health){healingNeeded=true;if(auto c=Offer(ai,s.spells[1].learned,u,false);c.spell){health=u->GetHealthPct();heal=c;}}};
 consider(b);
 if(healer)if(auto* group=b->GetGroup())for(auto* ref=group->GetFirstMember();ref;ref=ref->next())consider(ref->GetSource());
 if(healer)for(Unit* u:scene.members)if(u->GetTypeId()!=TYPEID_PLAYER)consider(u);
 if(heal.spell)return heal;
 if(healer&&healingNeeded)
 {
  // A blocked heal must not also block the real zero-cost mana recovery opportunity.
  if(!s.summons[1].present&&scene.uncoveredMana&&CanUse(ai,9003370,b))return {9003370,b,false,false,"healer_blocked_low_mana"};
  return {};
 }
 if(!partyCombat)
  if(auto d=RebornWD171::Buff(b,scene,s.plan.spec,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
   return {d.spell,d.target,false,false,d.reason};
 Unit* enemy=nullptr;
 if(auto* value=ai->GetAiObjectContext()->GetValue<Unit*>("current target"))enemy=value->Get();
 Player* master=ai->GetMaster();
 if(!Engaged(b,enemy,master)&&master)enemy=master->GetSelectedUnit();
 // Native combat may know the actual attacker before the assist target refreshes.
 if(!Engaged(b,enemy,master))enemy=b->GetVictim();
 if(!Engaged(b,enemy,master))for(auto* attacker:b->getAttackers())if(Engaged(b,attacker,master)){enemy=attacker;break;}
 if(!Engaged(b,enemy,master))return {};
 // Get into casting range before spending three global cooldowns on immobile summons far from the fight.
 uint32 attack=b->HasSpell(9003968)?s.spells[5].learned:s.spells[0].learned;
 // WD176: keep the real proc replacement ID through native prepare and damage hooks.
 if(s.plan.spec==0&&s.spells[0].replacement==9003822)attack=9003822;
 auto approach=Offer(ai,attack,enemy,true);if(approach.reposition)return approach;
 // Do not spend an expiring Price window on summon maintenance; emergency heals still win.
 bool priceReady=b->HasAura(9004175,b->GetGUID());
 if(s.plan.spec==2&&priceReady)if(auto c=Offer(ai,s.spells[8].learned,enemy,true);c.spell){c.reason="price_glaive";return c;}
 if(auto d=RebornWD171::Summon(b,s,scene,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 if(s.plan.spec==0)
  if(auto c=RebornWD175::Choose(b,enemy,[&](uint32 id,Unit* u){return Offer(ai,id,u,true);});c.spell)return c;
 if(s.plan.spec==2)
 {
  // Do not refresh our own still-active Hex every tick; another doctor's aura is not ours.
  bool ownHex=false;for(uint32 id:{9003104u,9003106u,9003107u,9003108u,9003109u,9003110u,9003111u,9003112u})
   if(enemy->HasAura(id,b->GetGUID())){ownHex=true;break;}
  if(!ownHex&&s.spells[2].replacement==0)if(auto c=Offer(ai,s.spells[2].learned,enemy,true);c.spell)return c;
  // Eclipse snapshots Spirits, it does not consume them. Only the native completed cast grants Price.
  // Keep an existing window; missing talent/Glaive must not block the learned Eclipse itself.
  if(s.souls==5&&!priceReady)if(auto c=Offer(ai,s.spells[7].learned,enemy,true);c.spell){c.reason="five_spirit_eclipse";return c;}
  // Reclamation is the real Spirit generator. The mapped single-target Volley is a real cast, never a synthetic proc.
  uint32 generator=s.spells[6].replacement==9004174?s.spells[6].replacement:s.spells[6].learned;
  if(s.souls<5||s.spells[6].replacement==9004174)if(auto c=Offer(ai,generator,enemy,true);c.spell)return c;
  if(auto c=Offer(ai,s.spells[8].learned,enemy,true);c.spell)return c;
 }
 if(auto c=Offer(ai,attack,enemy,true);c.spell)return c;
 // Missing ranged weapon/ammo must not freeze every otherwise legal spell.
 if(attack!=s.spells[0].learned&&!(s.plan.spec==0&&s.spells[0].replacement==9003822))return Offer(ai,s.spells[0].learned,enemy,true);
 return {};
}
class TickAction:public MovementAction
{
 PlayerbotAI* ai;uint32 quietSince=0,lastMaintenance=0,lastLog=0;
 bool SafeActor()
 {auto* b=ai->GetBot();return b->IsAlive()&&b->IsInWorld()&&!b->IsBeingTeleported()&&!b->HasUnitState(UNIT_STATE_LOST_CONTROL)&&b->GetCharmerGUID().IsEmpty()&&!b->IsNonMeleeSpellCast(false,false,true);}
 bool MaintenanceDue()
 {
  uint32 now=getMSTime();if(PartyCombat(ai)){quietSince=0;return false;}
  if(!quietSince)quietSince=now?now:1;
  return SafeActor()&&getMSTimeDiff(lastMaintenance,now)>=5000;
 }
public:
 explicit TickAction(PlayerbotAI* a):MovementAction(a,"wd169 tick"),ai(a){}
 bool isUseful()override
 {if(PartyCombat(ai))quietSince=0;return Choose(ai).spell!=0||MaintenanceDue();}
 bool Execute(Event)override
 {
  if(!SafeActor())return false;
  auto snapshot=WD168::Capture(ai->GetBot());if(!snapshot.plan.available)return false;
  uint32 now=getMSTime();auto* b=ai->GetBot();
  if(PartyCombat(ai))quietSince=0;
  else
  {
   if(!quietSince)quietSince=now?now:1;
   if(getMSTimeDiff(lastMaintenance,now)>=5000)
   {
    lastMaintenance=now;
    if(Occupied(snapshot)&&getMSTimeDiff(quietSince,now)>=6000&&CanUse(ai,snapshot.spells[9].learned,b))
     return ai->CastSpell(snapshot.spells[9].learned,b); // Original recall owns removal and paid-cost refund.
    if(RebornWD170::EquipOne(b,snapshot.plan.spec==2&&b->HasSpell(9003968)))return true;
   }
  }
  auto next=Choose(ai);
  if(!next.spell||!next.target)return false;
  if(next.damage&&!Engaged(b,next.target,ai->GetMaster()))return false;
  if(next.reposition)
  {
   auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement");
   if(value){auto const& last=value->Get();if(last.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(last.msTime,now)<last.lastdelayTime)return false;}
   auto* info=sSpellMgr->GetSpellInfo(next.spell);if(!info||!Prepared(ai,next.spell))return false;
   if(b!=next.target&&!b->IsWithinLOSInMap(next.target))
   {if(ai->HasStrategy("stay",BOT_STATE_COMBAT)||!b->IsWithinDistInMap(next.target,55.f))return false;return MoveToLOS(next.target,true);}
   float max=info->GetMaxRange(info->IsPositive(),b),min=info->GetMinRange(info->IsPositive());
   if(b!=next.target&&(!b->IsWithinCombatRange(next.target,max)||(min>0&&b->IsWithinCombatRange(next.target,min))))
   {
    if(ai->HasStrategy("stay",BOT_STATE_COMBAT)||!b->IsWithinDistInMap(next.target,55.f))return false;
    if(min>0&&b->IsWithinCombatRange(next.target,min))return MoveAway(next.target,std::max(min+2.f,15.f));
    return ReachCombatTo(next.target,std::max(min+2.f,std::min(25.f,max-2.f)));
   }
   if(b->isMoving()&&info->CalcCastTime(b)>0)
   {b->StopMoving();ai->SetNextCheckDelay(150);return true;} // Re-evaluate on the next tick, after native movement stops.
   return false;
  }
  if(!CanUse(ai,next.spell,next.target))return false;
  if(getMSTimeDiff(lastLog,now)>=5000){lastLog=now;LOG_DEBUG("playerbots","WD176 {} spec={} spell={} souls={} reason={}",b->GetName(),snapshot.plan.spec,next.spell,snapshot.souls,next.reason);}
  WD168::AutomaticCastScope scope(b,next.spell);
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
