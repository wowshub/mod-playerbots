/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Value.h"

#include "PerfMonitor.h"
#include "Playerbots.h"
#include "Timer.h"

UnitCalculatedValue::UnitCalculatedValue(PlayerbotAI* botAI, std::string const name, int32 checkInterval)
    : CalculatedValue<Unit*>(botAI, name, checkInterval)
{
    value = nullptr;
}

std::string const UnitCalculatedValue::Format()
{
    Unit* unit = Get();
    return unit ? unit->GetName() : "<none>";
}

std::string const UnitManualSetValue::Format()
{
    Unit* unit = Get();
    return unit ? unit->GetName() : "<none>";
}

std::string const Uint8CalculatedValue::Format()
{
    std::ostringstream out;
    out << Calculate();
    return out.str();
}

std::string const Uint32CalculatedValue::Format()
{
    std::ostringstream out;
    out << Calculate();
    return out.str();
}

std::string const FloatCalculatedValue::Format()
{
    std::ostringstream out;
    out << Calculate();
    return out.str();
}

CDPairCalculatedValue::CDPairCalculatedValue(PlayerbotAI* botAI, std::string const name, int32 checkInterval)
    : CalculatedValue<CreatureData const*>(botAI, name, checkInterval)
{
    // lastCheckTime = getMSTime() - checkInterval / 2;
}

std::string const CDPairCalculatedValue::Format()
{
    CreatureData const* creatureData = Calculate();
    if (creatureData)
    {
        CreatureTemplate const* bmTemplate = sObjectMgr->GetCreatureTemplate(creatureData->id1);
        return bmTemplate ? bmTemplate->Name : "<none>";
    }

    return "<none>";
}

CDPairListCalculatedValue::CDPairListCalculatedValue(PlayerbotAI* botAI, std::string const name, int32 checkInterval)
    : CalculatedValue<std::vector<CreatureData const*>>(botAI, name, checkInterval)
{
    // lastCheckTime = time(nullptr) - checkInterval / 2;
}

std::string const CDPairListCalculatedValue::Format()
{
    std::ostringstream out;
    out << "{";
    std::vector<CreatureData const*> cdPairs = Calculate();
    for (CreatureData const* cdPair : cdPairs)
    {
        out << cdPair->id1 << ",";
    }

    out << "}";
    return out.str();
}

ObjectGuidCalculatedValue::ObjectGuidCalculatedValue(PlayerbotAI* botAI, std::string const name, int32 checkInterval)
    : CalculatedValue<ObjectGuid>(botAI, name, checkInterval)
{
    // lastCheckTime = time(nullptr) - checkInterval / 2;
}

std::string const ObjectGuidCalculatedValue::Format()
{
    ObjectGuid guid = Calculate();
    return guid ? std::to_string(guid.GetRawValue()) : "<none>";
}

ObjectGuidListCalculatedValue::ObjectGuidListCalculatedValue(PlayerbotAI* botAI, std::string const name,
                                                             int32 checkInterval)
    : CalculatedValue<GuidVector>(botAI, name, checkInterval)
{
}

std::string const ObjectGuidListCalculatedValue::Format()
{
    std::ostringstream out;
    out << "{";

    GuidVector guids = Calculate();
    for (GuidVector::iterator i = guids.begin(); i != guids.end(); ++i)
    {
        ObjectGuid guid = *i;
        out << guid.GetRawValue() << ",";
    }
    out << "}";

    return out.str();
}

// MBCRASH1B: resolve identity on every read, including inside the cache window.
// This runs in the normal map update context; it is not a cross-thread lifetime lock.
Unit* UnitCalculatedValue::ResolveCachedUnit()
{
    value = nullptr;
    if (cachedGuid.IsEmpty() || !bot->IsInWorld() || bot->IsDuringRemoveFromWorld())
        return nullptr;

    Unit* fresh = ObjectAccessor::GetUnit(*bot, cachedGuid);
    if (fresh && fresh->IsInWorld() && !fresh->IsDuringRemoveFromWorld())
        value = fresh;
    return value;
}

Unit* UnitCalculatedValue::Get()
{
    uint32 const now = getMSTime();
    // Unsigned subtraction also handles the millisecond clock wrapping.
    if (checkInterval < 2 || !cacheInitialized || uint32(now - lastCheckTime) >= checkInterval)
    {
        PerfMonitorOperation* pmo = sPerfMonitor.start(
            PERF_MON_VALUE, this->getName(), this->context ? &this->context->performanceStack : nullptr);
        Set(Calculate());
        lastCheckTime = now;
        if (pmo)
            pmo->finish();
    }
    return ResolveCachedUnit();
}

Unit* UnitCalculatedValue::LazyGet()
{
    return cacheInitialized ? ResolveCachedUnit() : Get();
}

Unit*& UnitCalculatedValue::RefGet()
{
    Get();
    // Legacy interface: callers must use Set() to change identity, and must not
    // retain this borrowed result across updates. No Unit* RefGet writers exist
    // in the audited module. Never derive cachedGuid from this scratch pointer.
    return value;
}

void UnitCalculatedValue::Set(Unit* unit)
{
    // Only a freshly calculated / caller-owned live pointer may enter Set().
    cachedGuid = unit ? unit->GetGUID() : ObjectGuid::Empty;
    value = nullptr;
    cacheInitialized = true;
}

void UnitCalculatedValue::Reset()
{
    cachedGuid = ObjectGuid::Empty;
    value = nullptr;
    lastCheckTime = 0;
    cacheInitialized = false;
}

Unit* UnitManualSetValue::Get()
{
    // Prevent crashing by InWorld check
    if (value && value->IsInWorld())
        return value;
    return nullptr;
}
