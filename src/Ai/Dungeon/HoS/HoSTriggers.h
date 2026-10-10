#ifndef PLAYERBOTS_HOSTRIGGERS_H
#define PLAYERBOTS_HOSTRIGGERS_H

#include "Trigger.h"
#include "PlayerbotAIConfig.h"
#include "GenericTriggers.h"
#include "DungeonStrategyUtils.h"

enum HallsOfStoneIDs
{
    // Krystallus
    SPELL_GROUND_SLAM               = 50827,
    DEBUFF_GROUND_SLAM              = 50833,

    // Tribunal of Ages (RebornWOW DCAI1B)
    NPC_DARK_MATTER_TARGET          = 28237,
    SPELL_DARK_MATTER_VISUAL        = 51000,

    // Sjonnir The Ironshaper
    SPELL_LIGHTNING_RING_N          = 50840,
    SPELL_LIGHTNING_RING_H          = 59848,
};

#define SPELL_LIGHTNING_RING        DUNGEON_MODE(bot, SPELL_LIGHTNING_RING_N, SPELL_LIGHTNING_RING_H)

class KrystallusGroundSlamTrigger : public Trigger
{
public:
    KrystallusGroundSlamTrigger(PlayerbotAI* ai) : Trigger(ai, "krystallus ground slam") {}
    bool IsActive() override;
};

// Tribunal of Ages: a glowing Dark Matter target (28237) is near -- it will explode (5 yd)
// where it stops, on a random player's spot.
class TribunalDarkMatterTrigger : public Trigger
{
public:
    TribunalDarkMatterTrigger(PlayerbotAI* ai) : Trigger(ai, "tribunal dark matter") {}
    bool IsActive() override;
};

class SjonnirLightningRingTrigger : public Trigger
{
public:
    SjonnirLightningRingTrigger(PlayerbotAI* ai) : Trigger(ai, "sjonnir lightning ring") {}
    bool IsActive() override;
};

#endif
