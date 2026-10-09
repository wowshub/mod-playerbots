/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CREATURE_NAME_ALIAS_H
#define PLAYERBOTS_CREATURE_NAME_ALIAS_H

#include "Define.h"

#include <string>
#include <vector>

// Dungeon / raid tactics find bosses and adds by their English name ("find target", "krystallus").
// On a realm whose creature_template names are translated (RebornWOW: Chinese) those lookups never
// match, so no tactic ever fires. This maps the English names the tactics use to creature entries
// (generated from upstream creature_template.sql, see CreatureNameAliasData.inc) so a creature can be
// matched by entry, or by whatever its name is in this realm's database.
namespace CreatureNameAlias
{
    // Entries whose upstream English name is `englishName` (case-insensitive); nullptr if unknown.
    std::vector<uint32> const* Entries(std::string const& englishName);

    // `entry` is a creature whose upstream English name is `englishName`.
    bool IsEntryNamed(uint32 entry, std::string const& englishName);

    // `localName` is this realm's database name of a creature whose English name is `englishName`.
    bool IsLocalNameOf(std::string const& localName, std::string const& englishName);
}

#endif
