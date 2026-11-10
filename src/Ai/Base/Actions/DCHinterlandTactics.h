/*
 * DarkChaos addition to mod-playerbots (https://github.com/mod-playerbots/mod-playerbots, commit 079f9e88).
 * Ported into this project (HLBG2A) with the type id / map id adjusted to the local renumbering:
 * battleground type 20, map 20411 (HinterlandBGTest).
 *
 * Hinterland BG (BattlegroundTypeId 20) is a custom battleground. The queue side already works unmodified -- the
 * core registers type 20 in BattlegroundMgr::queueToBg/bgToQueue, so BGJoinAction's scan over the queue types picks it
 * up like any other bracket. What was missing is what happens after the port: AiFactory only hands out the generic
 * "battleground" strategy for bgType <= BATTLEGROUND_EY or IC, and BGTactics dispatches on hardcoded per-BG waypoint
 * tables. HLBG matched neither, so bots landed on their start point and stood there until the battleground's own AFK
 * sweep removed them.
 *
 * HLBG is not an objective battleground: there are no flags, no capturable nodes and no graveyard ownership. Both
 * sides start with a resource pool and drain the enemy's by killing their players, their guard camps and finally
 * their faction boss. So this is deliberately NOT another case in BGTactics -- it needs roaming, not objective
 * capture. All this action does is keep the bot walking towards somewhere worth fighting; the ordinary combat engine
 * and the "pvp" / "dps assist" / "attack tagged" strategies that AiFactory already adds inside a battleground handle
 * the fighting.
 *
 * The `modules` and `scripts` libraries are siblings and do not link each other, so nothing here may reach into
 * BattlegroundHLBG. The battleground is addressed by its numeric type id and its geography is a local table.
 */

#ifndef PLAYERBOTS_DCHINTERLANDTACTICS_H
#define PLAYERBOTS_DCHINTERLANDTACTICS_H

#include "MovementActions.h"

class PlayerbotAI;

#define DC_BATTLEGROUND_HLBG_TYPE_ID 20
#define DC_BATTLEGROUND_HLBG_MAP_ID 20411

#define DC_HLBG_OBJECTIVE_KEY "dc hlbg objective"

class DCHinterlandTacticsAction : public MovementAction
{
public:
    DCHinterlandTacticsAction(PlayerbotAI* botAI, std::string const name = "dc hinterland tactics")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;

    static bool IsInHinterlandBG(Player* player);

private:
    bool MoveToStaging();
    bool MoveToObjective();
    bool SelectObjective();

    uint32 _lastPickMs = 0;
    uint32 _repickIntervalMs = 0;

    // HLBG V18: the team starts are piers over deep water and the server has no vmaps, so a bot can sit in the water with
    // no walkable route. Every few seconds the position is sampled; a bot that has not moved gets a short hop towards its
    // objective (teleport on the same map, landing on the water surface or the ground) so the battle actually happens.
    uint32 _stuckSampleMs = 0;
    float _stuckX = 0.0f;
    float _stuckY = 0.0f;
    bool _stuckSampleSet = false;
    uint32 _stuckCount = 0;
    bool HopTowards(float x, float y);

    // HLBG V23: land-only route design (DCHinterlandRoutes.h). The objective is one of ten key places; the bot walks a
    // precomputed chain of short legs on ground polygons from the key place nearest to it, instead of one long
    // move that the core turns into a swimming shortcut or a straight line through the terrain.
    bool FollowRoute(uint32 mapId, float x, float y, float z);
    int32 _targetNode = -1;
    int32 _routeFrom = -1;
    int32 _routeTarget = -1;
    uint32 _wpIdx = 0;
};

class DCHinterlandResetObjectiveAction : public Action
{
public:
    DCHinterlandResetObjectiveAction(PlayerbotAI* botAI, std::string const name = "dc hinterland reset objective")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
