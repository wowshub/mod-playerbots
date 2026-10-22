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
#include "WitchDoctorVoodoo189.h"
#include "WitchDoctorCooldown190.h"
#include "WitchDoctorVeil192.h"
#include "WitchDoctorIdol193.h"
#include "WitchDoctorControl196.h"
#include "WitchDoctorInterrupt197.h"
#include "WitchDoctorFool200.h"
#include "WitchDoctorCure178.h"
#include "WitchDoctorResurrect179.h"
#include "WitchDoctorHeal180.h"
#include "WitchDoctorBeam182.h"
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
#include <string_view>
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
// WD188A: summon spell is self-targeted; its range does NOT describe ward coverage.
inline float WardCoverageRange()
{
 auto* bolt=sSpellMgr->GetSpellInfo(9003161);if(!bolt)return 0.f;
 // Ward AI scans 30 yards. Reserve 5 for the 2.5-yard mass ring and target motion.
 float range=std::min(30.f,bolt->GetMaxRange(false));return range>5.f ? range-5.f : 0.f;
}
inline bool AttackWard(uint32 spell){return spell==9003160||spell==9004080||spell==9004182;}
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
inline Choice BeamSideChoice(PlayerbotAI* ai)
{
 auto* b=ai->GetBot();if(!WD168::BeamSideReady(b))return {};
 if(auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement"))
 {auto const& m=value->Get();if(m.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(m.msTime,getMSTime())<m.lastdelayTime)return {};}
 auto scene=RebornWD171::Observe(b,PartyCombat(ai),[&](Unit* u){return Engaged(b,u,ai->GetMaster());});
 auto can=[&](uint32 id,Unit* u){return WD168::BeamSideReady(b,id)&&CanUse(ai,id,u);};
 auto triage=RebornWD180::Choose(b,scene,true,0,
  [&](Unit* u){return MemberTarget(b,u)&&(u==b||(u->GetTypeId()==TYPEID_PLAYER&&b->GetGroup()&&b->IsInRaidWith(u))||RebornWD171::Member(b,u));},
  [&](uint32 id,Unit* u)->Choice{return id&&can(id,u)?Choice{id,u,false,false,"beam_side_heal"}:Choice{};},can);
 return triage.choice; // No preparation, summons, cures or repositioning during a channel.
}
inline Unit* CurrentEnemy(PlayerbotAI* ai)
{
 auto* b=ai->GetBot();
 Unit* enemy=nullptr;
 if(auto* value=ai->GetAiObjectContext()->GetValue<Unit*>("current target"))enemy=value->Get();
 Player* master=ai->GetMaster();
 if(!Engaged(b,enemy,master)&&master)enemy=master->GetSelectedUnit();
 // Native combat may know the actual attacker before the assist target refreshes.
 if(!Engaged(b,enemy,master))enemy=b->GetVictim();
 if(!Engaged(b,enemy,master))for(auto* attacker:b->getAttackers())if(Engaged(b,attacker,master)){enemy=attacker;break;}
 return Engaged(b,enemy,master)?enemy:nullptr;
}
inline Choice Choose(PlayerbotAI* ai)
{
 Player* b=ai->GetBot();
 if(b->GetCurrentSpell(CURRENT_CHANNELED_SPELL))return BeamSideChoice(ai);
 auto s=WD168::Capture(b);
 if(!s.plan.available||!s.alive||!s.inWorld||b->IsBeingTeleported()||(b->IsNonMeleeSpellCast(false,false,true)||b->HasUnitState(UNIT_STATE_LOST_CONTROL))||!b->GetCharmerGUID().IsEmpty())return {};
 bool partyCombat=PartyCombat(ai);
 auto scene=RebornWD171::Observe(b,partyCombat,[&](Unit* u){return Engaged(b,u,ai->GetMaster());});
 bool healer=s.plan.spec==1;
 if(auto d=RebornWD190::Survival(b,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 auto triage=RebornWD180::Choose(b,scene,healer,s.spells[1].learned,
  [&](Unit* u){return MemberTarget(b,u)&&(u==b||(u->GetTypeId()==TYPEID_PLAYER&&b->GetGroup()&&b->IsInRaidWith(u))||RebornWD171::Member(b,u));},
  [&](uint32 id,Unit* u){return Offer(ai,id,u,false);},[&](uint32 id,Unit* u){return CanUse(ai,id,u);});
 bool healingNeeded=triage.needed;auto heal=triage.choice;
 // Urgent healing precedes cures; routine healing retains the prior cure priority.
 if(heal.spell&&triage.urgent)return heal;
 if(auto d=RebornWD178::Choose(ai,scene,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 // WD182: emergency/instant healing and actual cures stay ahead of a new channel.
 if(healer&&(!heal.spell||heal.spell==s.spells[1].learned||std::string_view(heal.reason)=="single_spirit_bottle"))
  if(auto c=RebornWD182::Choose(b,s,scene,partyCombat,
   [&](Unit* u){return MemberTarget(b,u)&&(u==b||(u->GetTypeId()==TYPEID_PLAYER&&b->GetGroup()&&b->IsInRaidWith(u))||RebornWD171::Member(b,u));},
   [&](uint32 id,Unit* u){return Offer(ai,id,u,false);},[&](uint32 id,Unit* u){return CanUse(ai,id,u);});c.spell)return c;
 if(heal.spell)return heal;
 if(healer&&healingNeeded)
 {
  // A blocked heal must not also block the real zero-cost mana recovery opportunity.
  if(!s.summons[1].present&&scene.uncoveredMana&&CanUse(ai,9003370,b))return {9003370,b,false,false,"healer_blocked_low_mana"};
  return {};
 }
 // WD179: safe post-combat player resurrection before routine Wuju refresh.
 // Keep triage/cures first and do not start a long cast beside a critical ally.
 if(!partyCombat&&!scene.enemies&&!scene.critical&&b->GetHealthPct()>=60.f&&Prepared(ai,s.spells[4].learned))
  if(auto c=RebornWD179::Choose(ai,s,[&](uint32 id,Unit* u){return Offer(ai,id,u,false);});c.spell)return c;
 if(auto prep=RebornWD180::Preparation(b,healer,partyCombat,healingNeeded,[&](uint32 id,Unit* u){return CanUse(ai,id,u);}))
  return {prep,b,false,false,"prepare_healing_ingredient"};
 if(!partyCombat)
  if(auto d=RebornWD171::Buff(b,scene,s.plan.spec,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
   return {d.spell,d.target,false,false,d.reason};
 Unit* enemy=CurrentEnemy(ai);Player* master=ai->GetMaster();
 if(!Engaged(b,enemy,master))return {};
 if(auto d=RebornWD189::Defense(b,scene,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 bool swiftForced=false;
 if(auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement"))
 {auto const& m=value->Get();swiftForced=m.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(m.msTime,getMSTime())<m.lastdelayTime;}
 if(auto d=RebornWD197::Choose(b,enemy,scene,swiftForced,[&](Unit* u){return Engaged(b,u,master);},[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,true,false,d.reason};
 if(auto d=RebornWD196::Control(b,enemy,scene,swiftForced,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,true,false,d.reason};
 if(auto d=RebornWD193::Swift(b,s,scene,swiftForced,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 // Get into casting range before spending three global cooldowns on immobile summons far from the fight.
 uint32 attack=b->HasSpell(9003968)?s.spells[5].learned:s.spells[0].learned;
 // WD176: keep the real proc replacement ID through native prepare and damage hooks.
 if(s.plan.spec==0&&s.spells[0].replacement==9003822)attack=9003822;
 auto approach=Offer(ai,attack,enemy,true);if(approach.reposition)return approach;
 // Do not spend an expiring Price window on summon maintenance; emergency heals still win.
 bool priceReady=b->HasAura(9004175,b->GetGUID());
 if(s.plan.spec==2&&priceReady)if(auto c=Offer(ai,s.spells[8].learned,enemy,true);c.spell){c.reason="price_glaive";return c;}
 // WD177: learned class utilities are shared. Spec selects the main damage/heal chain,
 // not a whitelist of spellbook columns. Urgent party recovery still outranks debuffs.
 auto summon=RebornWD171::Summon(b,s,scene,[&](uint32 id,Unit* u){return (!AttackWard(id)||WardCoverageRange()>0.f)&&CanUse(ai,id,u);});
 std::string_view why=summon.reason;
 if(summon.spell&&(why=="uncovered_group_healing"||why=="group_health_imbalance"||
                  why=="uncovered_low_mana"||why=="group_dispel_needed"))
  return {summon.spell,summon.target,false,false,summon.reason};
 // Brewing keeps a reserve and only reaches this point when no ally needs its heal.
 if(!healer||(s.maxMana&&s.mana*100ull>=s.maxMana*60ull))
  if(auto c=RebornWD175::Maintain(b,enemy,[&](uint32 id,Unit* u){return Offer(ai,id,u,true);});c.spell)return c;
 bool movementLocked=false;
 if(auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement"))
 {auto const& m=value->Get();movementLocked=m.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(m.msTime,getMSTime())<m.lastdelayTime;}
 if(auto d=RebornWD190::Burst(b,enemy,s,scene,movementLocked,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,false,false,d.reason};
 if(auto d=RebornWD192::Veil(b,enemy,s,movementLocked,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,true,false,d.reason};
 // Ready damage and passive cleave must not wait behind routine slot maintenance.
 if(s.plan.spec==0)
 {
  if(auto c=RebornWD175::Choose(b,enemy,[&](uint32 id,Unit* u){return Offer(ai,id,u,true);});c.spell)return c;
  if(auto c=RebornWD189::Damage(b,enemy,s,scene,
   [&](uint32 id,Unit* u){return Offer(ai,id,u,u!=b);},[&](uint32 id,Unit* u){return CanUse(ai,id,u);});c.spell)return c;
 }
 // Routine summons no longer starve missing Hex/Jinx; existing slot/cooldown safety stays native.
 if(summon.spell)
 {
  if(AttackWard(summon.spell)&&(!b->IsWithinDistInMap(enemy,WardCoverageRange())||!b->IsWithinLOSInMap(enemy)))
   return {summon.spell,enemy,true,true,"ward_coverage_move"};
  return {summon.spell,summon.target,false,false,summon.reason};
 }
 if(auto d=RebornWD200::Fool(b,enemy,s,scene,movementLocked,[&](uint32 id,Unit* u){return CanUse(ai,id,u);});d.spell)
  return {d.spell,d.target,true,false,d.reason};
 if(s.plan.spec==2)
 {
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
 {auto* b=ai->GetBot();return b->IsAlive()&&b->IsInWorld()&&!b->IsBeingTeleported()&&!b->HasUnitState(UNIT_STATE_LOST_CONTROL)&&b->GetCharmerGUID().IsEmpty()&&(!b->IsNonMeleeSpellCast(false,false,true)||WD168::BeamSideReady(b));}
 bool MaintenanceDue()
 {
  if(ai->GetBot()->GetCurrentSpell(CURRENT_CHANNELED_SPELL))return false;
  uint32 now=getMSTime();if(PartyCombat(ai)){quietSince=0;return false;}
  if(!quietSince)quietSince=now?now:1;
  return SafeActor()&&getMSTimeDiff(lastMaintenance,now)>=5000;
 }
public:
 explicit TickAction(PlayerbotAI* a):MovementAction(a,"wd169 tick"),ai(a){}
 bool isUseful()override
 {
  // WD199: unavailable is not an empty, valid build. Do not schedule maintenance.
  if(!WD168::ReadPlan(ai->GetBot()).available){quietSince=0;return false;}
  if(PartyCombat(ai))quietSince=0;return Choose(ai).spell!=0||MaintenanceDue();
 }
 bool Execute(Event)override
 {
  if(!SafeActor())return false;
  auto snapshot=WD168::Capture(ai->GetBot());if(!snapshot.plan.available){quietSince=0;return false;}
  uint32 now=getMSTime();auto* b=ai->GetBot();
  // WD198: select recovery before optional equipment/recall maintenance.
  auto next=Choose(ai);
  if(PartyCombat(ai)||b->GetCurrentSpell(CURRENT_CHANNELED_SPELL))quietSince=0;
  else
  {
   if(!quietSince)quietSince=now?now:1;
   if(!next.spell&&getMSTimeDiff(lastMaintenance,now)>=5000)
   {
    lastMaintenance=now;
    if(auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement"))
    {auto const& movement=value->Get();if(movement.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(movement.msTime,now)<movement.lastdelayTime)return false;}
    // A heal may be unavailable (mana/range/cooldown); keep existing recovery support.
    auto recovery=RebornWD171::Observe(b,false,[&](Unit* u){return Engaged(b,u,ai->GetMaster());});
    if(recovery.enemies||recovery.injured||recovery.lowMana||recovery.critical||recovery.dispelNeeded)return false;
    if(Occupied(snapshot)&&getMSTimeDiff(quietSince,now)>=6000&&CanUse(ai,snapshot.spells[9].learned,b))
     return ai->CastSpell(snapshot.spells[9].learned,b); // Original recall owns removal and paid-cost refund.
    if(RebornWD170::EquipOne(b,snapshot.plan.spec==2&&b->HasSpell(9003968)))return true;
   }
  }
  if(!next.spell||!next.target)return false;
  if(next.damage&&!Engaged(b,next.target,ai->GetMaster()))return false;
  if(next.reposition)
  {
   auto* value=ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement");
   if(value){auto const& last=value->Get();if(last.priority==MovementPriority::MOVEMENT_FORCED&&getMSTimeDiff(last.msTime,now)<last.lastdelayTime)return false;}
   auto* info=sSpellMgr->GetSpellInfo(next.spell);if(!info||!Prepared(ai,next.spell))return false;
   if(std::string_view(next.reason)=="ward_coverage_move")
   {
    // Choose is recomputed each Execute. Never cast a self-summon on this enemy.
    float range=WardCoverageRange();if(range<=0.f||ai->HasStrategy("stay",BOT_STATE_COMBAT)||!b->IsWithinDistInMap(next.target,55.f))return false;
    if(!b->IsWithinLOSInMap(next.target))return MoveToLOS(next.target,true);
    return ReachCombatTo(next.target,std::max(1.f,range-2.f));
   }
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
  if(getMSTimeDiff(lastLog,now)>=5000){lastLog=now;LOG_DEBUG("playerbots","WD193 {} spec={} spell={} souls={} reason={}",b->GetName(),snapshot.plan.spec,next.spell,snapshot.souls,next.reason);}
  WD168::AutomaticCastScope scope(b,next.spell);
  WD196::TargetScope controlTarget(b,next.spell==9003861?next.target:nullptr,next.spell==9003861?CurrentEnemy(ai):nullptr);
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
