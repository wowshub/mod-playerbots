// WD182A: start a real learned channel only for stable, clustered party injuries.
#ifndef PLAYERBOTS_WD_BEAM_182_H
#define PLAYERBOTS_WD_BEAM_182_H
#include "WitchDoctorHeal180.h"
#include "../../../../../mod-reborn-witchdoctor/src/RebornWitchDoctorBeamPolicy182.h"
namespace RebornWD182
{
template<class Scene,class Gate,class Offer,class Can>
inline auto Choose(Player* b,WD168::Snapshot const& s,Scene const& scene,bool combat,Gate eligible,Offer offer,Can can)
{
 auto none=offer(0,nullptr);
 if(s.plan.spec!=1||!combat||b->isMoving())return none;
 uint32 id=RebornWD180::Learned(b,9003922,9003928);if(!id)return none;
 auto* info=sSpellMgr->GetSpellInfo(id);
 int32 rawCost=info->CalcPowerCost(b,info->GetSchoolMask());
 uint32 upfront=uint32(std::max(0,rawCost));
 if(!WD182::StartReserve(s.mana,s.maxMana,upfront,WD182::FirstTickCost(id)))return none;
 std::vector<Unit*> members;
 auto add=[&](Unit* u){if(u&&eligible(u)&&std::find(members.begin(),members.end(),u)==members.end())members.push_back(u);};
 add(b);if(auto* g=b->GetGroup())for(auto* r=g->GetFirstMember();r;r=r->next())add(r->GetSource());
 for(Unit* u:scene.members)add(u);
 for(Unit* u:members)if(u->GetHealthPct()<35.f)return none;
 Unit* best=nullptr;uint32 coverage=1;
 for(Unit* center:members)if(center->GetHealthPct()<70.f&&can(id,center))
 {
  uint32 injured=0;
  for(Unit* u:members)if(u->GetHealthPct()<75.f&&center->IsWithinDistInMap(u,15.f)&&center->IsWithinLOSInMap(u))++injured;
  if(injured>coverage||(injured==coverage&&injured>=2&&best&&center->GetHealthPct()<best->GetHealthPct())){best=center;coverage=injured;}
 }
 if(best){auto c=offer(id,best);c.reason="cluster_mojo_beam";return c;}
 return none;
}
}
#endif
