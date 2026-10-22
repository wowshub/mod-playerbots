// WD189A: native learned-spell choices. Thresholds are candidates, not a DPS optimum.
#ifndef PLAYERBOTS_WD_VOODOO_189_H
#define PLAYERBOTS_WD_VOODOO_189_H
#include "WitchDoctorTeam171.h"
namespace RebornWD189
{
inline bool Reserve(WD168::Snapshot const& s,uint32 pct)
{return s.maxMana&&s.mana*100ull>=s.maxMana*pct;}
inline uint32 Cluster(Player* b,Unit* center,RebornWD171::Scene const& scene,float radius)
{
 if(!center||radius<=0.f)return 0;
 uint32 n=0;
 for(auto* u:scene.hostiles)if(u&&u->IsAlive()&&center->IsWithinDistInMap(u,radius)&&b->IsWithinLOSInMap(u))++n;
 return n;
}
template<class Can>inline RebornWD171::Decision Defense(Player* b,RebornWD171::Scene const& scene,Can can)
{
 // 13yd leaves room for the original 1.5/2yd spawn offset in a 15yd support radius.
 uint32 critical=0;bool hurt=false;
 for(auto* u:scene.members)if(u&&b->IsWithinDistInMap(u,13.f)&&b->IsWithinLOSInMap(u))
 {if(u->GetHealthPct()<40.f&&!u->HasAura(9003583))++critical;if(u->GetHealthPct()<70.f)hurt=true;}
 if(!b->HasAura(9003583)&&!b->HasAura(9003581)&&!b->HasAura(9003800)&&
    (b->GetHealthPct()<25.f||critical>=2)&&can(9003580,b))return {9003580,b,"voodoo_emergency_protection"};
 if(!b->HasAura(9003800)&&!b->HasAura(9003581)&&scene.castingEnemies&&hurt&&can(9003800,b))
  return {9003800,b,"golem_incoming_magic"};
 return {};
}
template<class Offer,class Can>inline auto Damage(Player* b,Unit* enemy,WD168::Snapshot const& s,RebornWD171::Scene const& scene,Offer offer,Can can)
{
 auto empty=offer(0,enemy);
 auto wrath=WD168::ReadSpell(b,9003100);
 if(!s.mimicPresent&&!b->isMoving()&&enemy->GetHealthPct()>40.f&&Reserve(s,40)&&
    b->IsWithinDistInMap(enemy,35.f)&&b->IsWithinLOSInMap(enemy)&&
    can(wrath.replacement?wrath.replacement:wrath.learned,enemy)&&can(9003673,b))
 {auto c=offer(9003673,b);c.reason="voodoo_mimic_window";return c;}
 // Dark Effigy is passive: native Juju/Hexfire copies remain the cleave mechanism.
 // During Mimic, spend filler casts on the spell it can copy rather than Shadowflare.
 auto flare=WD168::ReadSpell(b,9003560);
 if(!s.mimicPresent&&!flare.replacement&&Reserve(s,45)&&s.summons[0].present&&
    (scene.enemies<3||s.massWards||!can(9004080,b)))
  if(auto* info=sSpellMgr->GetSpellInfo(flare.learned))
   if(Cluster(b,enemy,scene,info->Effects[EFFECT_0].CalcRadius(b))>=3)
    if(auto c=offer(flare.learned,enemy);c.spell){c.reason="voodoo_cluster_shadowflare";return c;}
 return empty;
}
}
#endif
