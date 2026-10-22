// WD171A: observe real nearby team state; choose one compatible Wuju and useful summons.
#ifndef PLAYERBOTS_WD_TEAM_171_H
#define PLAYERBOTS_WD_TEAM_171_H
#include "Player.h"
#include "Creature.h"
#include "Totem.h"
#include "Group.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "SpellMgr.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include "../../../../../mod-reborn-witchdoctor/src/RebornWitchDoctorBotState168.h"
namespace RebornWD171
{
inline bool Member(Player* b,Unit* u)
{
 if(!u||!u->IsAlive()||!u->IsInWorld()||!b->IsInMap(u)||!b->InSamePhase(u)||!b->IsValidAssistTarget(u))return false;
 if(u==b)return true;
 if(u->GetTypeId()==TYPEID_PLAYER)return b->GetGroup()&&b->IsInRaidWith(u);
#ifdef MOD_NPCERBOTS
 if(u->IsNPCBot())return b->GetGroup()&&u->ToCreature()->GetBotGroup()==b->GetGroup();
#endif
 return false;
}
inline bool NPCCombat(Player* b)
{
#ifdef MOD_NPCERBOTS
 if(!b->GetGroup())return false;
 std::list<Unit*> units;Acore::AnyUnitInObjectRangeCheck check(b,40.f);
 Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> search(b,units,check);Cell::VisitObjects(b,search,40.f);
 for(Unit* u:units)if(u&&u->IsNPCBot()&&Member(b,u)&&u->IsInCombat())return true;
#endif
 return false;
}
inline uint32 Class(Unit* u)
{
#ifdef MOD_NPCERBOTS
 if(u->IsNPCBot())return u->ToCreature()->GetBotClass();
#endif
 return u->getClass();
}
inline bool Physical(Unit* u)
{
 switch(Class(u))
 {
  case CLASS_WARRIOR:case CLASS_HUNTER:case CLASS_ROGUE:case CLASS_DEATH_KNIGHT:return true;
  case 13:if(auto* p=u->ToPlayer()){auto plan=WD168::ReadPlan(p);return plan.available&&plan.spec==2;}return false;
  case CLASS_PALADIN:case CLASS_SHAMAN:case CLASS_DRUID:
   return u->getPowerType()!=POWER_MANA||u->GetTotalAttackPowerValue(BASE_ATTACK)>1.5f*std::max(1,u->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
  default:return false;
 }
}
inline int WujuFamily(uint32 id)
{
 if(id==9003140||(id>=9003141&&id<=9003143)||id==9003290)return 0;
 if((id>=9003180&&id<=9003185)||id==9003300)return 1;
 if(id==9003190||id==9003280)return 2;
 return -1;
}
inline bool Restoration(uint32 spell,bool mana,uint32 depth=0)
{
 if(!spell||depth>3)return false;
 // Native Shaman effects are scripted dummy pulses: inspect their exact heal/mana spell families, not class presence.
 if(!mana)for(uint32 id:{52041u,52046u,52047u,52048u,52049u,52050u,58759u,58760u,58761u})if(spell==id)return true;
 if(mana)for(uint32 id:{52031u,52033u,52034u,52035u,52036u,58778u,58779u,58780u})if(spell==id)return true;
 auto* info=sSpellMgr->GetSpellInfo(spell);if(!info)return false;
 for(auto const& e:info->Effects)
 {
  if(!mana&&(e.Effect==SPELL_EFFECT_HEAL||e.Effect==SPELL_EFFECT_HEAL_PCT||e.ApplyAuraName==SPELL_AURA_PERIODIC_HEAL))return true;
  if(mana&&((e.Effect==SPELL_EFFECT_ENERGIZE||e.Effect==SPELL_EFFECT_ENERGIZE_PCT)&&e.MiscValue==POWER_MANA))return true;
  if(e.TriggerSpell&&e.TriggerSpell!=spell&&Restoration(e.TriggerSpell,mana,depth+1))return true;
 }
 return false;
}
// WD193: Swift is movement/resistance, not attack haste or a root cleanse.
inline bool SwiftRecipient(Player* b,Unit* u)
{
 return Member(b,u)&&u->IsInCombat()&&u->isMoving()&&
  !u->HasAuraType(SPELL_AURA_MOD_ROOT)&&!u->HasAura(9003433)&&
  b->IsWithinDistInMap(u,28.f)&&b->IsWithinLOSInMap(u);
}
struct Scene
{
 std::vector<Unit*> members,hostiles;
 uint32 swiftMoving=0;
 uint32 injured=0,uncoveredHealing=0,critical=0,lowMana=0,uncoveredMana=0,enemies=0,castingEnemies=0,physical=0;
 uint32 linkLow=0,linkHigh=0,linkPlayers=0,dispelNeeded=0;
};
inline bool Covers(Player* b,Unit* source,Unit* recipient,bool mana)
{
 if(!source||!source->IsAlive()||!source->IsInWorld()||!b->IsInMap(source)||!b->InSamePhase(source)||!source->IsWithinDistInMap(recipient,30.f)||!source->IsWithinLOSInMap(recipient))return false;
 auto* creature=source->ToCreature();if(!creature)return false;
 Unit* owner=source->GetOwner();if(!Member(b,owner))return false;
 if(!mana&&source->GetEntry()==900191)return true;
 if(mana&&source->GetEntry()==900192)return true;
 if(!source->IsTotem())return false;
 return Restoration(source->ToTotem()->GetSpell(),mana);
}
template<class EnemyGate>inline Scene Observe(Player* b,bool combat,EnemyGate engaged)
{
 Scene out;std::list<Unit*> nearby;
 // Out of combat, 100 yd includes the widest Greater Wuju; combat observations only need 40 yd.
 float radius=combat?40.f:100.f;
 Acore::AnyUnitInObjectRangeCheck check(b,radius);Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> search(b,nearby,check);Cell::VisitObjects(b,search,radius);
 nearby.push_back(b);
 for(auto* u:nearby)
 {
  if(Member(b,u)&&b->IsWithinDistInMap(u,radius)&&std::find(out.members.begin(),out.members.end(),u)==out.members.end())out.members.push_back(u);
  if(u!=b&&b->IsWithinDistInMap(u,30.f)&&engaged(u)){out.hostiles.push_back(u);++out.enemies;if(u->IsNonMeleeSpellCast(false,false,true))++out.castingEnemies;}
 }
 for(Unit* u:out.members)
 {
  if(SwiftRecipient(b,u))++out.swiftMoving;
  if(Physical(u))++out.physical;
  if(!b->IsWithinDistInMap(u,30.f)||!b->IsWithinLOSInMap(u))continue;
  bool hurt=u->GetHealthPct()<75.f,low=u->getPowerType()==POWER_MANA&&u->GetMaxPower(POWER_MANA)>0&&u->GetPower(POWER_MANA)*100ull<u->GetMaxPower(POWER_MANA)*65ull;
  bool healCovered=false,manaCovered=false;
  if(hurt||low)for(Unit* source:nearby){if(hurt&&!healCovered)healCovered=Covers(b,source,u,false);if(low&&!manaCovered)manaCovered=Covers(b,source,u,true);}
  if(hurt){++out.injured;if(!healCovered)++out.uncoveredHealing;}
  if(u->GetHealthPct()<35.f)++out.critical;
  if(low){++out.lowMana;if(!manaCovered||u->GetPower(POWER_MANA)*100ull<u->GetMaxPower(POWER_MANA)*30ull)++out.uncoveredMana;}
  if(u->GetTypeId()==TYPEID_PLAYER&&b->IsWithinDistInMap(u,10.f))
  {++out.linkPlayers;if(u->GetHealthPct()<35.f)++out.linkLow;if(u->GetHealthPct()>=80.f)++out.linkHigh;}
  for(auto const& pair:u->GetAppliedAuras())
   if(auto* a=pair.second?pair.second->GetBase():nullptr)
    if(!a->GetSpellInfo()->IsPositive()&&(a->GetSpellInfo()->Dispel==DISPEL_CURSE||a->GetSpellInfo()->Dispel==DISPEL_DISEASE||a->GetSpellInfo()->Dispel==DISPEL_POISON)){++out.dispelNeeded;break;}
 }
 return out;
}
struct Decision { uint32 spell=0;Unit* target=nullptr;char const* reason="none"; };
inline bool EquivalentPower(Player* b,Unit* target,uint32 desired)
{
 if(WujuFamily(desired)!=1)return false;
 auto* info=sSpellMgr->GetSpellInfo(desired);if(!info)return false;
 int32 melee=0,ranged=0;
 for(auto const& pair:target->GetAppliedAuras())
 {
  auto* a=pair.second?pair.second->GetBase():nullptr;if(!a||WujuFamily(a->GetId())>=0||!a->GetSpellInfo()->IsPositive())continue;
  for(uint32 i=0;i<3;++i)if(auto* e=a->GetEffect(i))
  {
   auto type=a->GetSpellInfo()->Effects[i].ApplyAuraName;
   if(type==SPELL_AURA_MOD_ATTACK_POWER)melee=std::max(melee,e->GetAmount());
   if(type==SPELL_AURA_MOD_RANGED_ATTACK_POWER)ranged=std::max(ranged,e->GetAmount());
  }
 }
 return melee>=std::abs(info->Effects[0].CalcValue(b))&&ranged>=std::abs(info->Effects[1].CalcValue(b));
}
// Return -1 for protected existing coverage, 0 for adequate own coverage, 1 for missing/expiring/weaker own buff.
inline int Coverage(Player* b,Unit* target,uint32 desired)
{
 auto* info=sSpellMgr->GetSpellInfo(desired);if(!info)return -1;
 bool ownCovered=false;
 for(auto const& pair:target->GetAppliedAuras())
 {
  auto* aura=pair.second?pair.second->GetBase():nullptr;if(!aura||WujuFamily(aura->GetId())<0)continue;
  if(aura->GetCasterGUID()!=b->GetGUID())return -1; // Do not fight another doctor's Wuju choice.
  if(WujuFamily(aura->GetId())!=WujuFamily(desired))continue;
  if(aura->GetDuration()>=0&&aura->GetDuration()<=30000)continue;
  auto* effect=aura->GetEffect(EFFECT_0);
  if(effect&&std::abs(effect->GetAmount())>=std::abs(info->Effects[EFFECT_0].CalcValue(b)))ownCovered=true;
 }
 return ownCovered||EquivalentPower(b,target,desired)?0:1;
}
inline bool GroupConflict(Player* b,Unit* target,uint32 groupSpell)
{
 auto* info=sSpellMgr->GetSpellInfo(groupSpell);if(!info)return true;
 for(auto const& pair:target->GetAppliedAuras())
 {
  auto* a=pair.second?pair.second->GetBase():nullptr;if(!a||WujuFamily(a->GetId())<0)continue;
  if(a->GetCasterGUID()!=b->GetGUID())return true;
  if(WujuFamily(a->GetId())==WujuFamily(groupSpell))if(auto* e=a->GetEffect(EFFECT_0))
   if(std::abs(e->GetAmount())>std::abs(info->Effects[EFFECT_0].CalcValue(b)))return true;
 }
 return false;
}
template<class Can>inline Decision Buff(Player* b,Scene const& scene,uint32 spec,Can can)
{
 if(b->isMoving()||scene.members.empty())return {};
 bool group=scene.members.size()>1;
 int first=group?(scene.physical*2>=scene.members.size()?1:0):(spec==2?1:0);
 uint32 singles[]={9003140,9003180,9003190},groups[]={9003290,9003300,9003280};
 int order[]={first,2,1-first};int family=-1;
 if(group)for(int f:order)if(b->HasSpell(groups[f])){family=f;break;}
 if(family<0)for(int f:order)if(WD168::ReadSpell(b,singles[f]).learned){family=f;break;}
 if(family<0)return {};
 uint32 single=WD168::ReadSpell(b,singles[family]).learned,greater=group&&b->HasSpell(groups[family])?groups[family]:0;
 float radius=family==0?30.f:family==1?40.f:100.f;
 bool conflict=false,missing=false;
 if(greater)for(Unit* u:scene.members)if(b->IsWithinDistInMap(u,radius))
 {if(GroupConflict(b,u,greater))conflict=true;if(Coverage(b,u,greater)>0)missing=true;}
 if(greater&&!conflict&&missing&&can(greater,b))return {greater,b,"group_wuju_missing"};
 if(single)for(Unit* u:scene.members)
  if(Coverage(b,u,single)>0&&can(single,u))return {single,u,group?"single_wuju_group_fallback":"solo_wuju_missing"};
 return {};
}
template<class Can>inline Decision Summon(Player* b,WD168::Snapshot const& s,Scene const& t,Can can)
{
 auto choose=[&](uint32 id,char const* why)->Decision{if(id&&can(id,b))return {id,b,why};return {};};
 bool ordinary=!s.summons[0].present||s.summons[0].entry==900190||s.summons[0].entry==900191;
 // Reactive healing can replace an ordinary attack ward once; recovery never replaces it back mid-life.
 if(ordinary&&t.uncoveredHealing&&(t.injured>=2||t.critical||s.plan.spec==1)&&s.summons[0].entry!=900191)
  if(auto d=choose(9003170,"uncovered_group_healing");d.spell)return d;
 if(!s.summons[1].present)
 {
  if(t.linkPlayers>=2&&t.linkLow&&t.linkHigh)if(auto d=choose(9003953,"group_health_imbalance");d.spell)return d;
  if(t.uncoveredMana)if(auto d=choose(9003370,"uncovered_low_mana");d.spell)return d;
  if(t.dispelNeeded)if(auto d=choose(9003440,"group_dispel_needed");d.spell)return d;
 }
 // WD201: native unit casts do not resolve the player's temporary button replacement.
 // Respect the actual mapping; never fall back to the replaced six-serpent spell.
 if(s.plan.spec!=1&&s.maxMana&&s.mana*100ull>=s.maxMana*45ull)
 {
  auto call=WD168::ReadSpell(b,9004080);
  if(call.replacement==9004182)
  {
   if(call.learned==9004080&&t.enemies&& !s.viperPresent&&
      b->HasSpell(9004180)&&b->HasAura(9004180,b->GetGUID()))
    if(auto d=choose(9004182,"viper_replacement_missing");d.spell)return d;
  }
  else if(!call.replacement&&t.enemies>=3&&s.massWards==0&&!b->HasAura(9004080,b->GetGUID()))
   if(auto d=choose(9004080,"three_engaged_enemies");d.spell)return d;
 }
 if(!s.summons[0].present)if(auto d=choose(9003160,"single_attack_ward_missing");d.spell)return d;
 if(!s.summons[1].present&&s.summons[1].selected&&s.summons[1].selected!=9003370&&s.summons[1].selected!=9003953&&s.summons[1].selected!=9003440&&s.summons[1].selected!=9003432)
  if(auto d=choose(s.summons[1].selected,"selected_support_idol");d.spell)return d;
 if(!s.summons[2].present&&t.castingEnemies)if(auto d=choose(9003420,"engaged_enemy_casting");d.spell)return d;
 return {};
}
}
#endif
