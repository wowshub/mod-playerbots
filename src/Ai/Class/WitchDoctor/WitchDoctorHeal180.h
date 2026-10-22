// WD180A: native learned Potion Toss / Splash triage. No grants or synthetic heals.
#ifndef PLAYERBOTS_WD_HEAL_180_H
#define PLAYERBOTS_WD_HEAL_180_H
#include "Player.h"
#include "SpellMgr.h"
#include <vector>
#include <algorithm>
namespace RebornWD180
{
inline uint32 Learned(Player* b,uint32 first,uint32 last)
{
 for(uint32 id=last;;--id)
 {
  if(b->HasSpell(id))if(auto* info=sSpellMgr->GetSpellInfo(id))
   if(b->GetLevel()>=info->SpellLevel)return id;
  if(id==first)break;
 }
 return 0;
}
inline bool Ingredient(Player* b)
{
 // Match the native OnCheckCast contract; never replace an existing manual preparation.
 for(uint32 id:{9003865u,9003901u,9003905u,9003915u})if(b->HasAura(id))return true;
 return false;
}
template<class Choice>struct Triage
{Choice choice{};bool needed=false,urgent=false;float lowest=100.f;};
template<class Scene,class Gate,class Offer,class Can>
inline auto Choose(Player* b,Scene const& scene,bool healer,uint32 brew,Gate eligible,Offer offer,Can can)
{
 using Choice=decltype(offer(0,nullptr));Triage<Choice> out;
 std::vector<Unit*> members;
 auto add=[&](Unit* u)
 {if(u&&eligible(u)&&std::find(members.begin(),members.end(),u)==members.end())members.push_back(u);};
 add(b);
 if(healer)
 {
  if(auto* g=b->GetGroup())for(auto* r=g->GetFirstMember();r;r=r->next())add(r->GetSource());
  for(Unit* u:scene.members)add(u);
 }
 std::stable_sort(members.begin(),members.end(),[](Unit* a,Unit* c){return a->GetHealthPct()<c->GetHealthPct();});
 float threshold=healer?85.f:40.f;
 for(Unit* u:members)if(u->GetHealthPct()<threshold)
 {out.needed=true;out.lowest=std::min(out.lowest,u->GetHealthPct());}
 out.urgent=out.needed&&out.lowest<(healer?35.f:40.f);
 if(!out.needed)return out;
 uint32 toss=Learned(b,9003870,9003876),splash=Learned(b,9003890,9003896);
 uint32 bottle=Learned(b,9003460,9003467);
 // WD184: preserve the native base-dependent Bottle replacement. The active
 // action must be learned separately; the passive alone never grants a cast.
 bool tikiBase=b->HasSpell(9003958)&&b->HasAura(9003958,b->GetGUID())&&
    (b->HasAura(9003942)||b->HasAura(9003944));
 uint32 tiki=tikiBase?Learned(b,9003959,9003959):0;
 if(tikiBase)bottle=0;
 bool ingredient=Ingredient(b);
 // Emergency: a direct instant heal on the lowest reachable member wins first.
 if(ingredient&&toss&&out.urgent)
  for(Unit* u:members)if(u->GetHealthPct()<(healer?35.f:40.f)&&can(toss,u))
   {out.choice=offer(toss,u);out.choice.reason="emergency_potion_toss";return out;}
 if(bottle&&out.urgent)
  for(Unit* u:members)if(u->GetHealthPct()<(healer?35.f:40.f)&&can(bottle,u))
   {out.choice=offer(bottle,u);out.choice.reason="emergency_spirit_bottle";return out;}
 // Splash is a destination heal. Count recipients around the actual destination,
 // using the native radius (including spell mods), not distance from the caster.
 if(ingredient&&healer&&splash)
 {
  auto* info=sSpellMgr->GetSpellInfo(splash);float radius=info->Effects[0].CalcRadius(b);
  Unit* center=nullptr;uint32 best=2;float deficit=0.f;
  if(radius>0.f)for(Unit* c:members)if(c->GetHealthPct()<75.f&&can(splash,c))
  {
   uint32 count=0;float missing=0.f;bool coversUrgent=false;
   for(Unit* u:members)if(u->GetHealthPct()<75.f&&c->IsWithinDistInMap(u,radius)&&c->IsWithinLOSInMap(u))
   {++count;missing+=75.f-u->GetHealthPct();if(u->GetHealthPct()==out.lowest)coversUrgent=true;}
   if(out.urgent&&!coversUrgent)continue;
   if(count>best||(count==best&&count>=3&&missing>deficit)){best=count;deficit=missing;center=c;}
  }
  if(center){out.choice=offer(splash,center);out.choice.reason="cluster_splash_potion";return out;}
 }
 // Tiki is a persistent ground HoT, not an instant Bottle heal or an emergency
 // substitute. Cover at least two wounded members without our existing Tiki.
 Choice tikiChoice{};
 if(healer&&!out.urgent&&tiki)
 {
  auto* info=sSpellMgr->GetSpellInfo(tiki);float radius=info->Effects[0].CalcRadius(b);
  Unit* center=nullptr;uint32 best=1;float deficit=0.f;
  if(radius>0.f)for(Unit* c:members)if(c->GetHealthPct()<75.f&&!c->HasAura(tiki,b->GetGUID())&&can(tiki,c))
  {
   uint32 count=0;float missing=0.f;
   for(Unit* u:members)if(u->GetHealthPct()<75.f&&!u->HasAura(tiki,b->GetGUID())&&
      c->IsWithinDistInMap(u,radius)&&c->IsWithinLOSInMap(u))
      {++count;missing+=75.f-u->GetHealthPct();}
   if(count>best||(count==best&&count>=2&&missing>deficit)){best=count;deficit=missing;center=c;}
  }
  if(center){tikiChoice=offer(tiki,center);tikiChoice.reason="cluster_tiki_splash";}
 }
 // Save limited instant cooldowns for meaningful deficits; ordinary Brew tops up.
 for(Unit* u:members)if(u->GetHealthPct()<threshold)
 {
  if(ingredient&&toss&&u->GetHealthPct()<70.f&&can(toss,u))
   {out.choice=offer(toss,u);out.choice.reason="single_potion_toss";return out;}
  if(tikiChoice.spell){out.choice=tikiChoice;return out;}
  if(bottle&&u->GetHealthPct()<70.f&&can(bottle,u))
   {out.choice=offer(bottle,u);out.choice.reason="single_spirit_bottle";return out;}
  if(auto c=offer(brew,u);c.spell){out.choice=c;return out;}
 }
 return out;
}
template<class Can>inline uint32 Preparation(Player* b,bool healer,bool combat,bool healingNeeded,Can can)
{
 if(!healer||combat||healingNeeded||Ingredient(b)||
    (!Learned(b,9003870,9003876)&&!Learned(b,9003890,9003896)))return 0;
 // Prefer the healing-over-time ingredient; alternatives retain their native effects.
 // Do not auto-select offensive Bloodthistle or alter Mixologist/FIFO/mojo state.
 for(uint32 id:{9003865u,9003901u,9003905u})
  if(Learned(b,id,id)&&can(id,b))return id;
 return 0;
}
}
#endif
