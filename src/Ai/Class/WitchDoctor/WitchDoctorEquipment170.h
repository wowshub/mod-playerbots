// WD170A: existing carried items only; final authority is the native inventory handler.
#ifndef PLAYERBOTS_WD_EQUIPMENT_170_H
#define PLAYERBOTS_WD_EQUIPMENT_170_H
#include "Player.h"
#include "Bag.h"
#include "Item.h"
#include "ItemPackets.h"
#include "WorldSession.h"
#include "Log.h"
#include <algorithm>
namespace RebornWD170
{
inline bool Shooting(ItemTemplate const* t)
{return t&&t->Class==ITEM_CLASS_WEAPON&&(t->SubClass==ITEM_SUBCLASS_WEAPON_BOW||t->SubClass==ITEM_SUBCLASS_WEAPON_CROSSBOW||t->SubClass==ITEM_SUBCLASS_WEAPON_GUN);}
inline float Score(ItemTemplate const* t,bool shadow,bool ranged)
{
 if(!t||t->Class!=ITEM_CLASS_WEAPON)return -1.f;
 if(ranged&&(shadow?!Shooting(t):t->SubClass!=ITEM_SUBCLASS_WEAPON_WAND))return -1.f;
 float v=0;
 for(uint32 i=0;i<std::min(uint32(MAX_ITEM_PROTO_STATS),t->StatsCount);++i)
 {
  auto const& s=t->ItemStat[i];float weight=0;
  switch(s.ItemStatType)
  {
   case ITEM_MOD_AGILITY:weight=shadow?2.f:0.f;break;
   case ITEM_MOD_INTELLECT:weight=shadow?.1f:1.2f;break;
   case ITEM_MOD_SPIRIT:weight=shadow?0.f:1.f;break;
   case ITEM_MOD_SPELL_POWER:weight=shadow?.25f:2.f;break;
   case ITEM_MOD_ATTACK_POWER:case ITEM_MOD_RANGED_ATTACK_POWER:weight=shadow?1.f:0.f;break;
   default:break;
  }
  v+=s.ItemStatValue*weight;
 }
 // DPS matters for the shooting slot, not as a reason for a caster to choose a melee weapon.
 if(shadow&&t->Delay)for(auto const& d:t->Damage)v+=(d.DamageMin+d.DamageMax)*500.f/t->Delay*(ranged?8.f:.25f);
 return v;
}
template<class F>inline void Carried(Player* b,F f)
{
 for(uint8 i=INVENTORY_SLOT_ITEM_START;i<INVENTORY_SLOT_ITEM_END;++i)
  if(auto* item=b->GetItemByPos(INVENTORY_SLOT_BAG_0,i))f(item);
 for(uint8 slot=INVENTORY_SLOT_BAG_START;slot<INVENTORY_SLOT_BAG_END;++slot)
  if(auto* bag=b->GetBagByPos(slot))for(uint32 i=0;i<bag->GetBagSize();++i)if(auto* item=bag->GetItemByPos(i))f(item);
}
inline Item* BestAmmo(Player* b,ItemTemplate const* weapon)
{
 if(!Shooting(weapon))return nullptr;
 uint32 kind=weapon->SubClass==ITEM_SUBCLASS_WEAPON_GUN?ITEM_SUBCLASS_BULLET:ITEM_SUBCLASS_ARROW;
 Item* best=nullptr;float score=-1.f;
 Carried(b,[&](Item* item)
 {
  auto* t=item->GetTemplate();
  if(!item->GetCount()||t->Class!=ITEM_CLASS_PROJECTILE||t->SubClass!=kind||b->CanUseAmmo(t->ItemId)!=EQUIP_ERR_OK)return;
  float value=0;for(auto const& d:t->Damage)value+=d.DamageMin+d.DamageMax;
  if(value>score){score=value;best=item;}
 });
 return best;
}
inline bool Supplied(Player* b,ItemTemplate const* weapon)
{return Shooting(weapon)&&(b->HasAura(46699)||BestAmmo(b,weapon));}
inline bool SelectAmmo(Player* b,Item* weapon)
{
 if(!weapon||!Shooting(weapon->GetTemplate()))return false;
 // Native Requires No Ammo only skips the check when PLAYER_AMMO_ID is zero.
 if(b->HasAura(46699))
 {if(!b->GetUInt32Value(PLAYER_AMMO_ID))return false;b->RemoveAmmo();return !b->GetUInt32Value(PLAYER_AMMO_ID);}
 if(auto* ammo=BestAmmo(b,weapon->GetTemplate()))if(b->GetUInt32Value(PLAYER_AMMO_ID)!=ammo->GetEntry())
 {b->SetAmmo(ammo->GetEntry());return b->GetUInt32Value(PLAYER_AMMO_ID)==ammo->GetEntry();}
 return false;
}
inline bool EquipOne(Player* b,bool shadow)
{
 if(!b||!b->IsAlive()||!b->IsInWorld()||b->IsInCombat()||b->isMoving()||!b->GetSession())return false;
 for(uint8 slot:{uint8(EQUIPMENT_SLOT_RANGED),uint8(EQUIPMENT_SLOT_MAINHAND)})
 {
  bool ranged=slot==EQUIPMENT_SLOT_RANGED;Item* old=b->GetItemByPos(INVENTORY_SLOT_BAG_0,slot);Item* best=nullptr;
  float score=Score(old&&!old->IsBroken()?old->GetTemplate():nullptr,shadow,ranged);
  // WD185: a high-score gun with no carried usable bullets is not a usable
  // shooting loadout. Rank weapons only after checking the matching supply.
  if(shadow&&ranged&&(!old||!Supplied(b,old->GetTemplate())))score=-1.f;
  Carried(b,[&](Item* item)
  {
   if(item->IsBroken()||!item->GetCount())return;
   if(shadow&&ranged&&!Supplied(b,item->GetTemplate()))return;
   float v=Score(item->GetTemplate(),shadow,ranged);if(v<0||v<=score+.01f)return;
   uint16 dest=0;if(b->CanEquipItem(slot,dest,item,true)!=EQUIP_ERR_OK)return;
   score=v;best=item;
  });
  if(best)
  {
   ObjectGuid guid=best->GetGUID();WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT,9);packet<<guid<<slot;
   WorldPackets::Item::AutoEquipItemSlot request(std::move(packet));request.Read();
   b->GetSession()->HandleAutoEquipItemSlotOpcode(request);
   auto* now=b->GetItemByPos(INVENTORY_SLOT_BAG_0,slot);
   if(now&&now->GetGUID()==guid)
   {
    if(shadow&&ranged)
    {
     SelectAmmo(b,now); // Configure the matched ammo in the same successful equip step.
     LOG_DEBUG("playerbots","WD185 {} equipped_ranged={} selected_ammo={} supplied={}",b->GetName(),now->GetEntry(),b->GetUInt32Value(PLAYER_AMMO_ID),Supplied(b,now->GetTemplate()));
    }
    return true; // No manual item movement or destruction on failure.
   }
  }
 }
 if(shadow)
 {
  auto* weapon=b->GetItemByPos(INVENTORY_SLOT_BAG_0,EQUIPMENT_SLOT_RANGED);
  if(SelectAmmo(b,weapon))return true;
 }
 return false;
}
}
#endif

