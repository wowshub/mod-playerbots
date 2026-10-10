#include "Playerbots.h"
#include "HoSTriggers.h"
#include "AiObject.h"
#include "AiObjectContext.h"

bool KrystallusGroundSlamTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "krystallus");
    if (!boss) { return false; }

    // Check both of these... the spell is applied first, debuff later.
    // Neither is active for the full duration so we need to trigger off both
    return bot->HasAura(SPELL_GROUND_SLAM) || bot->HasAura(DEBUFF_GROUND_SLAM);
}

bool TribunalDarkMatterTrigger::IsActive()
{
    Creature* target = bot->FindNearestCreature(NPC_DARK_MATTER_TARGET, 12.0f);
    return target && target->HasAura(SPELL_DARK_MATTER_VISUAL);
}

bool SjonnirLightningRingTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sjonnir the ironshaper");
    if (!boss) { return false; }

    return boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_RING);
}
