#ifndef PLAYERBOTS_HOSTRIGGERCONTEXT_H
#define PLAYERBOTS_HOSTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "AiObjectContext.h"
#include "HoSTriggers.h"

class WotlkDungeonHoSTriggerContext : public NamedObjectContext<Trigger>
{
    public:
        WotlkDungeonHoSTriggerContext()
        {
            creators["ground slam"] = &WotlkDungeonHoSTriggerContext::ground_slam;
            creators["lightning ring"] = &WotlkDungeonHoSTriggerContext::lightning_ring;
            creators["dark matter"] = &WotlkDungeonHoSTriggerContext::dark_matter;
        }
    private:
        static Trigger* ground_slam(PlayerbotAI* ai) { return new KrystallusGroundSlamTrigger(ai); }
        static Trigger* lightning_ring(PlayerbotAI* ai) { return new SjonnirLightningRingTrigger(ai); }
        static Trigger* dark_matter(PlayerbotAI* ai) { return new TribunalDarkMatterTrigger(ai); }
};

#endif
