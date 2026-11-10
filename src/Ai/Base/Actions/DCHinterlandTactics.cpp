/*
 * DarkChaos addition to mod-playerbots (ported, HLBG2A). See DCHinterlandTactics.h.
 */

#include "DCHinterlandTactics.h"
#include "DCHinterlandRoutes.h"

#include <algorithm>
#include <cmath>

#include "Battleground.h"
#include "Config.h"
#include "Event.h"
#include "Player.h"
#include "Playerbots.h"
#include "PositionValue.h"

namespace
{
    struct HLBGPoint
    {
        float x;
        float y;
        float z;
    };

    // Geography read off the map spawn table. The battleground runs in an
    // instance, so only the coordinates are reused -- the map id comes
    // from Battleground::GetMapId() at the call site.
    //
    // Alliance holds the Wildhammer camp in the east, Horde the Revantusk /
    // Kor'kron camp in the west, and both sides garrison the two mixed outposts
    // in the middle where their infantry lines meet.
    constexpr HLBGPoint HLBG_ALLIANCE_CAMP     = { 17.0f, -4662.0f, 10.5f };
    constexpr HLBGPoint HLBG_ALLIANCE_LINE     = { -110.0f, -4617.0f, 11.0f };
    constexpr HLBGPoint HLBG_ALLIANCE_BOSS     = { 175.9f, -4736.8f, 14.7f };  // King Varian Wrynn, 810003
    // The battleground's own team start (game_graveyard 1721). Using the spot the
    // BG teleports arrivals to is guaranteed walkable and inside StartMaxDist.
    constexpr HLBGPoint HLBG_ALLIANCE_STAGING  = { 197.165f, -4808.54f, 7.848f };

    constexpr HLBGPoint HLBG_HORDE_CAMP        = { -546.0f, -4560.0f, 12.0f };
    constexpr HLBGPoint HLBG_HORDE_LINE        = { -500.0f, -4560.0f, 10.0f };
    constexpr HLBGPoint HLBG_HORDE_BOSS        = { -623.7f, -4581.6f, 11.7f };  // Thrall Warchief, 810002
    // game_graveyard 1722. The Revantusk Drummer spawns sit on a platform at z 30.5 - about 20 yards above
    // the surrounding ground - so the graveyard point is used instead.
    constexpr HLBGPoint HLBG_HORDE_STAGING     = { -628.484f, -4684.51f, 5.144f };

    constexpr HLBGPoint HLBG_MID_NORTH         = { -370.0f, -4428.0f, 12.5f };
    constexpr HLBGPoint HLBG_MID_CENTER        = { -315.0f, -4510.0f, 12.5f };

    struct HLBGSideView
    {
        HLBGPoint ownCamp;
        HLBGPoint ownLine;
        HLBGPoint ownStaging;
        HLBGPoint enemyCamp;
        HLBGPoint enemyLine;
        HLBGPoint enemyBoss;
    };

    HLBGSideView GetSideView(TeamId teamId)
    {
        if (teamId == TEAM_ALLIANCE)
            return { HLBG_ALLIANCE_CAMP, HLBG_ALLIANCE_LINE, HLBG_ALLIANCE_STAGING,
                     HLBG_HORDE_CAMP, HLBG_HORDE_LINE, HLBG_HORDE_BOSS };

        return { HLBG_HORDE_CAMP, HLBG_HORDE_LINE, HLBG_HORDE_STAGING,
                 HLBG_ALLIANCE_CAMP, HLBG_ALLIANCE_LINE, HLBG_ALLIANCE_BOSS };
    }

    struct HLBGConfig
    {
        bool enabled = true;
        uint32 repickSecondsMin = 45;
        uint32 repickSecondsMax = 90;
        float arrivalDistance = 18.0f;
        float engageDistance = 18.0f;
        // HLBG V20: the "stuck hop" is only a safety net now that the map has vmaps/mmaps. 0 = never hop.
        uint32 stuckSamplesNeeded = 3;   // consecutive 12 s samples without moving before a hop
        float hopDistance = 25.0f;
    };

    HLBGConfig const& GetConfig()
    {
        // Read once. isUseful() runs for every bot on every tick, and a
        // string-keyed config lookup per knob per tick across a full roster is
        // not free. A restart picks up changes; nothing here is hot-tunable.
        static HLBGConfig const cfg = []
        {
            HLBGConfig c;
            c.enabled = sConfigMgr->GetOption<bool>("AiPlayerbot.DCHinterland.Enable", true);
            c.repickSecondsMin = sConfigMgr->GetOption<uint32>("AiPlayerbot.DCHinterland.RepickSecondsMin", 45);
            c.repickSecondsMax = sConfigMgr->GetOption<uint32>("AiPlayerbot.DCHinterland.RepickSecondsMax", 90);
            c.arrivalDistance = sConfigMgr->GetOption<float>("AiPlayerbot.DCHinterland.ArrivalDistance", 18.0f);
            c.engageDistance = sConfigMgr->GetOption<float>("AiPlayerbot.DCHinterland.EngageDistance", 18.0f);
            c.stuckSamplesNeeded = sConfigMgr->GetOption<uint32>("AiPlayerbot.DCHinterland.StuckHopSamples", 3);
            c.hopDistance = sConfigMgr->GetOption<float>("AiPlayerbot.DCHinterland.HopDistance", 25.0f);

            if (c.repickSecondsMax < c.repickSecondsMin)
                c.repickSecondsMax = c.repickSecondsMin;

            return c;
        }();

        return cfg;
    }
}

bool DCHinterlandTacticsAction::IsInHinterlandBG(Player* player)
{
    if (!player || !player->InBattleground())
        return false;

    Battleground* bg = player->GetBattleground();
    if (!bg)
        return false;

    return bg->GetBgTypeID(true) == BattlegroundTypeId(DC_BATTLEGROUND_HLBG_TYPE_ID);
}

bool DCHinterlandTacticsAction::isUseful()
{
    if (!GetConfig().enabled)
        return false;

    if (!IsInHinterlandBG(bot))
        return false;

    // Nothing to steer while the bot is dead, mid-teleport, or already busy
    // fighting -- the combat engine owns movement then.
    if (bot->isDead() || bot->IsBeingTeleported() || bot->IsInCombat())
        return false;

    return true;
}

bool DCHinterlandTacticsAction::Execute(Event /*event*/)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    if (bg->GetStatus() == STATUS_WAIT_JOIN)
        return MoveToStaging();

    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return false;

    return MoveToObjective();
}

bool DCHinterlandTacticsAction::MoveToStaging()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    // HLBG V18: the team starts are piers over deep water (terrain z -18 / -8, water level 0) and the server has no vmaps,
    // so the walk-to-staging step could only produce "skipped a straight move" log lines (PBMOVE1A). Stand still during the
    // countdown instead; the objective walk starts the moment the doors open.
    return false;
}

bool DCHinterlandTacticsAction::SelectObjective()
{
    HLBGSideView const side = GetSideView(bot->GetBgTeamId());

    // "bg role" is rolled 0-9 when the bot accepts the battleground port. It is
    // set for every battleground type, HLBG included, so it is free to reuse as
    // a stable per-match disposition instead of re-rolling one here.
    uint32 const role = AI_VALUE(uint32, "bg role");

    (void)side;
    using namespace DCHLBGRoutes;
    bool const ally = bot->GetBgTeamId() == TEAM_ALLIANCE;
    auto rollTarget = [ally, role]() -> Node
    {
        if (role == 0)
        {
            // Home defence. HLBG drains resources when a team's own guards and
            // boss die, so somebody has to stay back or the camp is free to farm.
            if (ally)
                return urand(0, 1) ? A_CAMP : A_LINE;
            return urand(0, 1) ? H_CAMP : H_LINE;
        }

        if (role <= 3)
        {
            // Midfield. The two mixed outposts are where both infantry lines
            // meet, which is where the player kills happen.
            return urand(0, 1) ? MID_CENTER : MID_NORTH;
        }

        // Offence (roles 4-9, 60 % of the bots): push the enemy camp, and occasionally go for the boss, which
        // is worth 200 resources against the 5 a normal guard is worth.
        uint32 const roll = urand(0, 2);
        if (ally)
            return roll == 0 ? H_BOSS : (roll == 1 ? H_CAMP : H_LINE);
        return roll == 0 ? A_BOSS : (roll == 1 ? A_CAMP : A_LINE);
    };

    // A roll that lands where the bot already stands is a wasted cycle: it would
    // hold the same spot until the next expiry, and a bot that never moves is
    // what the battleground's AFK sweep exists to remove. A bounded retry is
    // enough - the buckets a role can draw from are far enough apart that one or
    // two rolls almost always move it.
    float const rerollRadius = GetConfig().arrivalDistance * 2.0f;
    Node node = rollTarget();
    for (uint32 attempt = 0; attempt < 3; ++attempt)
    {
        if (!bot->IsWithinDist3d(NodePos[node].x, NodePos[node].y, NodePos[node].z, rerollRadius))
            break;

        node = rollTarget();
    }
    HLBGPoint const target = { NodePos[node].x, NodePos[node].y, NodePos[node].z };
    _targetNode = static_cast<int32>(node);
    _routeFrom = -1;
    _routeTarget = -1;
    _wpIdx = 0;

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    PositionMap& posMap = AI_VALUE(PositionMap&, "position");
    PositionInfo objective;
    objective.Set(target.x, target.y, target.z, bg->GetMapId());
    posMap[DC_HLBG_OBJECTIVE_KEY] = objective;

    HLBGConfig const& cfg = GetConfig();
    _lastPickMs = getMSTime();
    _repickIntervalMs = urand(cfg.repickSecondsMin, cfg.repickSecondsMax) * IN_MILLISECONDS;
    return true;
}

bool DCHinterlandTacticsAction::MoveToObjective()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    HLBGConfig const& cfg = GetConfig();

    // An enemy player in reach outranks any waypoint: stop steering and let the
    // combat engine pick the target. Without this the bot walks past a fight to
    // reach a coordinate.
    GuidVector const enemies = AI_VALUE(GuidVector, "nearest enemy players");
    for (ObjectGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (enemy && enemy->IsAlive() && bot->IsWithinDist(enemy, cfg.engageDistance, false))
            return false;
    }

    PositionMap& posMap = AI_VALUE(PositionMap&, "position");
    PositionInfo objective = posMap[DC_HLBG_OBJECTIVE_KEY];

    // Unsigned arithmetic, so a getMSTime() wrap resolves the same way it does
    // everywhere else in the module.
    bool const expired = _repickIntervalMs && (getMSTime() - _lastPickMs) >= _repickIntervalMs;
    bool const wrongMap = objective.isSet() && objective.mapId != bg->GetMapId();

    if (!objective.isSet() || wrongMap || expired)
    {
        if (!SelectObjective())
            return false;

        objective = posMap[DC_HLBG_OBJECTIVE_KEY];
    }

    // Arriving means hold the ground, not pick somewhere new. The objective
    // only changes when the dwell timer expires.
    if (bot->IsWithinDist3d(objective.x, objective.y, objective.z, cfg.arrivalDistance))
    {
        _stuckSampleSet = false;
        return false;
    }

    return FollowRoute(objective.mapId, objective.x, objective.y, objective.z);
}

bool DCHinterlandTacticsAction::FollowRoute(uint32 mapId, float x, float y, float z)
{
    using namespace DCHLBGRoutes;
    HLBGConfig const& cfg = GetConfig();
    if (_targetNode < 0 || _targetNode >= static_cast<int32>(NODE_COUNT))
        return MoveTo(mapId, x, y, z);

    auto dist2d = [this](float x, float y)
    {
        float const dx = x - bot->GetPositionX();
        float const dy = y - bot->GetPositionY();
        return std::sqrt(dx * dx + dy * dy);
    };

    // (re)start the route from the key place nearest to the bot
    if (_routeFrom < 0 || _routeTarget != _targetNode)
    {
        float best = 1e9f;
        int32 from = 0;
        for (uint32 n = 0; n < NODE_COUNT; ++n)
        {
            float const d = dist2d(NodePos[n].x, NodePos[n].y);
            if (d < best)
            {
                best = d;
                from = static_cast<int32>(n);
            }
        }
        _routeFrom = from;
        _routeTarget = _targetNode;
        _wpIdx = 0;
    }

    Route const& route = Routes[_routeFrom][_routeTarget];
    if (_routeFrom == _routeTarget || !route.pts)
        return MoveTo(mapId, NodePos[_targetNode].x, NodePos[_targetNode].y, NodePos[_targetNode].z);

    // waypoints already reached
    while (_wpIdx < route.count && dist2d(route.pts[_wpIdx].x, route.pts[_wpIdx].y) <= 9.0f)
        ++_wpIdx;

    // pushed far off the route (a fight, a knock-back, a resurrection): start again from the nearest key place
    if (_wpIdx < route.count && dist2d(route.pts[_wpIdx].x, route.pts[_wpIdx].y) > 90.0f)
    {
        _routeFrom = -1;
        return false;
    }

    // stuck check: every 12 s compare with the last sample; a bot that has not moved is put on the current waypoint (always a
    // ground position), never along a straight line over the water
    uint32 const now = getMSTime();
    if (!_stuckSampleSet || (now - _stuckSampleMs) >= 12000)
    {
        bool const stuck = _stuckSampleSet &&
            (bot->GetPositionX() - _stuckX) * (bot->GetPositionX() - _stuckX) +
            (bot->GetPositionY() - _stuckY) * (bot->GetPositionY() - _stuckY) < 4.0f * 4.0f;
        _stuckSampleSet = true;
        _stuckSampleMs = now;
        _stuckX = bot->GetPositionX();
        _stuckY = bot->GetPositionY();
        _stuckCount = stuck ? _stuckCount + 1 : 0;
        if (cfg.stuckSamplesNeeded && _stuckCount >= cfg.stuckSamplesNeeded)
        {
            _stuckCount = 0;
            Wp const& hopTo = _wpIdx < route.count ? route.pts[_wpIdx] : NodePos[_targetNode];
            if (bot->TeleportTo(bot->GetMapId(), hopTo.x, hopTo.y, hopTo.z + 0.5f, bot->GetOrientation()))
                return true;
        }
    }

    if (_wpIdx >= route.count)
        return MoveTo(mapId, NodePos[_targetNode].x, NodePos[_targetNode].y, NodePos[_targetNode].z);

    Wp const& leg = route.pts[_wpIdx];
    return MoveTo(mapId, leg.x, leg.y, leg.z);
}

bool DCHinterlandTacticsAction::HopTowards(float x, float y)
{
    float const dx = x - bot->GetPositionX();
    float const dy = y - bot->GetPositionY();
    float const dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 20.0f)
        return false;

    float const hop = std::min(dist, GetConfig().hopDistance);
    float const nx = bot->GetPositionX() + dx / dist * hop;
    float const ny = bot->GetPositionY() + dy / dist * hop;
    // water surface over the sea, ground on land
    float const nz = bot->GetMap()->GetWaterOrGroundLevel(bot->GetPhaseMask(), nx, ny, bot->GetPositionZ() + 6.0f);
    if (nz <= INVALID_HEIGHT + 1.0f)
        return false;

    return bot->TeleportTo(bot->GetMapId(), nx, ny, nz + 0.5f, std::atan2(dy, dx));
}

bool DCHinterlandResetObjectiveAction::isUseful()
{
    return DCHinterlandTacticsAction::IsInHinterlandBG(bot);
}

bool DCHinterlandResetObjectiveAction::Execute(Event /*event*/)
{
    // Fired on death. The bot releases to a graveyard well away from wherever
    // it was heading, so the stored waypoint is stale by definition.
    PositionMap& posMap = AI_VALUE(PositionMap&, "position");
    PositionInfo objective = posMap[DC_HLBG_OBJECTIVE_KEY];
    objective.Reset();
    posMap[DC_HLBG_OBJECTIVE_KEY] = objective;
    return true;
}
