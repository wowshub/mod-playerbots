// WD175A: local, learned-only Voodoo maintenance. No spell grants or synthetic auras.
// Based on the CoA rotation's missing-Hex rule; local Jinxes share one global group.
#ifndef PLAYERBOTS_REBORN_WITCHDOCTOR_VOODOO175_H
#define PLAYERBOTS_REBORN_WITCHDOCTOR_VOODOO175_H
#include "Player.h"
#include "Unit.h"
#include "../../../../../mod-reborn-witchdoctor/src/RebornWitchDoctorBotState168.h"
namespace RebornWD175
{
inline bool OwnHex(Player* bot,Unit* target)
{
 if(!bot||!target)return false;
 for(uint32 id:{9003104u,9003106u,9003107u,9003108u,9003109u,9003110u,9003111u,9003112u})
  if(target->HasAura(id,bot->GetGUID()))return true;
 return false;
}
inline bool JinxOccupied(Unit* target)
{
 if(!target)return false;
 // WD20AuraGroup(2): any caster. Recasting would erase a teammate's Jinx too.
 // Do not include linked spellpower children or unrelated curses in this group.
 for(uint32 id:{9003150u,9003200u,9003210u,9003220u,9003221u,9003222u,
                 9003223u,9003224u,9003225u,9003762u})
  if(target->HasAura(id))return true;
 return false;
}
template<class Offer>inline auto Choose(Player* bot,Unit* target,Offer offer)
{
 auto empty=offer(0,target);
 if(!bot||!target)return empty;
 // Only rank families actually learned in the current build. Temporary replacements
 // belong to their separate native rotation and are never cast via the hidden base.
 auto hex=WD168::ReadSpell(bot,9003104);
 if(!OwnHex(bot,target)&&!hex.replacement)
  if(auto c=offer(hex.learned,target);c.spell){c.reason="voodoo_missing_own_hex";return c;}
 if(!JinxOccupied(target))
 {
 auto slow=WD168::ReadSpell(bot,9003150),shrink=WD168::ReadSpell(bot,9003220);
 // An observable enemy cast is evidence for Lethargy; no guesses from NPC class IDs.
 bool casting=target->IsNonMeleeSpellCast(false,false,true);
 if(casting&&!slow.replacement)
  if(auto c=offer(slow.learned,target);c.spell){c.reason="voodoo_caster_jinx";return c;}
 if(!shrink.replacement)
  if(auto c=offer(shrink.learned,target);c.spell){c.reason="voodoo_shrinking_jinx";return c;}
 }
 // WD176: complete native casts only. Never invent a proc or teach a missing rank.
 auto wrath=WD168::ReadSpell(bot,9003100);
 if(wrath.replacement==9003822&&bot->HasAura(9003821,bot->GetGUID()))
  if(auto c=offer(9003822,target);c.spell){c.reason="voodoo_hexfire_ready";return c;}
 auto juju=WD168::ReadSpell(bot,9003103);
 if(!juju.replacement)
  if(auto c=offer(juju.learned,target);c.spell){c.reason="voodoo_bad_juju";return c;}
 auto fire=WD168::ReadSpell(bot,9003500);
 if(!fire.replacement)
  if(auto c=offer(fire.learned,target);c.spell){c.reason="voodoo_hexfire";return c;}
 return empty;
}
}
#endif
