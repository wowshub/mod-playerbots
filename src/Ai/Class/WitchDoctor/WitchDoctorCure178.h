// WD178A: learned class support, real harmful auras, native dispel eligibility.
#ifndef PLAYERBOTS_REBORN_WITCHDOCTOR_CURE178_H
#define PLAYERBOTS_REBORN_WITCHDOCTOR_CURE178_H
#include "WitchDoctorTeam171.h"
#include "PlayerbotAI.h"
#include "SpellAuras.h"
#include <algorithm>
#include <vector>
namespace RebornWD178
{
constexpr uint32 Curse=1u<<DISPEL_CURSE,Poison=1u<<DISPEL_POISON,Disease=1u<<DISPEL_DISEASE;
struct Need { uint32 harmful=0,protectedMask=0;bool hexbreak=false; };
inline Need Observe(PlayerbotAI* ai,Unit* target,SpellInfo const* hexbreak)
{
 Need out;
 for(auto const& pair:target->GetAppliedAuras())
 {
  auto* app=pair.second;auto* aura=app?app->GetBase():nullptr;
  if(!aura||aura->IsRemoved()||aura->GetDuration()==0)continue;
  auto* info=aura->GetSpellInfo();if(!info)continue;
  uint32 bit=info->Dispel==DISPEL_CURSE?Curse:info->Dispel==DISPEL_POISON?Poison:info->Dispel==DISPEL_DISEASE?Disease:0;
  if(!bit)continue;
  // Preserve existing encounter exceptions (e.g. Mutating Injection). An area cure
  // must not remove a protected neighbour's aura to cure somebody else.
  // Native Allcure immunity purges every matching dispel category, even a
  // beneficial/passive aura. Do not remove those as collateral support.
  if(app->IsPositive()||aura->IsPassive()||!ai->canDispel(info,info->Dispel))out.protectedMask|=bit;
  else out.harmful|=bit;
 }
 if(hexbreak&&(out.harmful&Curse)&&!(out.protectedMask&Curse))
 {
  DispelChargesList list;target->GetDispellableAuraList(ai->GetBot(),Curse,list,hexbreak);
  for(auto const& pair:list)
  {
   auto* aura=pair.first;if(!aura||!pair.second||aura->IsRemoved()||aura->GetDuration()==0)continue;
   auto* app=aura->GetApplicationOfTarget(target->GetGUID());
   if(app&&!app->IsPositive()&&ai->canDispel(aura->GetSpellInfo(),DISPEL_CURSE)&&aura->CalcDispelChance(target,false)>0)
   {out.hexbreak=true;break;} // Observe eligibility; do not roll or remove an aura here.
  }
 }
 return out;
}
inline bool Covered(Unit* u)
{return u->HasAura(9003250)||u->HasAura(9003442)||u->HasAura(9004124);}
inline bool SafeHexbreakNeighbours(PlayerbotAI* ai,Unit* center)
{
 auto* b=ai->GetBot();if(!b->HasAura(9003911))return true;
 // WD28 ExtraAlly can reach a second friendly unit, including a non-party unit.
 // Conservatively refuse the parent if any possible secondary curse is protected.
 std::list<Unit*> nearby;Acore::AnyUnitInObjectRangeCheck check(center,8.f);
 Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> search(center,nearby,check);Cell::VisitObjects(center,search,8.f);
 for(Unit* u:nearby)
 {
  if(!u||u==center||!u->IsAlive()||!b->IsValidAssistTarget(u)||!b->IsInMap(u)||!b->InSamePhase(u)||
     !center->IsWithinDistInMap(u,8.f)||!center->IsWithinLOSInMap(u))continue;
  for(auto const& pair:u->GetAppliedAuras())
   if(auto* aura=pair.second?pair.second->GetBase():nullptr)
    if(auto* info=aura->GetSpellInfo();info&&info->Dispel==DISPEL_CURSE)
     if(!RebornWD171::Member(b,u)||pair.second->IsPositive()||!ai->canDispel(info,DISPEL_CURSE))return false;
 }
 return true;
}
template<class Can>inline RebornWD171::Decision Choose(PlayerbotAI* ai,RebornWD171::Scene const& scene,Can can)
{
 auto* b=ai->GetBot();auto hex=WD168::ReadSpell(b,9003240),cure=WD168::ReadSpell(b,9003250);
 uint32 hexSpell=hex.replacement?0:hex.learned;
 uint32 single=cure.replacement?0:cure.learned,group=0;
 if(cure.learned)
 {
  if(cure.replacement==9003442||cure.replacement==9004124)group=cure.replacement;
  else if(!cure.replacement&&b->HasSpell(9003442))group=9003442;
 }
 if(!hexSpell&&!single&&!group)return {};
 struct Row{Unit* target;Need need;};std::vector<Row> rows;
 auto add=[&](Unit* u)
 {
  if(!RebornWD171::Member(b,u)||!b->IsWithinDistInMap(u,40.f)||!b->IsWithinLOSInMap(u))return;
  if(std::find_if(rows.begin(),rows.end(),[&](Row const& r){return r.target==u;})!=rows.end())return;
  rows.push_back({u,Observe(ai,u,hexSpell?sSpellMgr->GetSpellInfo(hexSpell):nullptr)});
 };
 add(b);for(Unit* u:scene.members)add(u);
 std::sort(rows.begin(),rows.end(),[](Row const& a,Row const& c)
 {float ah=a.target->GetHealthPct(),ch=c.target->GetHealthPct();return ah==ch?a.target->GetGUID()<c.target->GetGUID():ah<ch;});
 uint32 affected=0;bool protectedGroup=false;
 for(auto const& row:rows)if(b->IsWithinDistInMap(row.target,30.f))
 {
  protectedGroup|=row.need.protectedMask!=0;
  if(row.need.harmful&&!row.need.protectedMask&&!Covered(row.target))++affected;
 }
 // A real learned group cure covers the local 30yd native area, never the whole raid by assumption.
 if(group&&affected>=2&&!protectedGroup&&can(group,b))return {group,b,"group_allcure"};
 for(auto const& row:rows)
 {
  if(row.need.hexbreak&&can(hexSpell,row.target)&&SafeHexbreakNeighbours(ai,row.target))return {hexSpell,row.target,"curse_hexbreak"};
  if(!row.need.harmful||row.need.protectedMask||Covered(row.target))continue;
  if(single&&can(single,row.target))return {single,row.target,"single_allcure"};
  // A real replacement can be the only version available; one afflicted ally is sufficient.
  if(group&&!protectedGroup&&b->IsWithinDistInMap(row.target,30.f)&&can(group,b))return {group,b,"group_allcure_fallback"};
 }
 return {};
}
}
#endif
