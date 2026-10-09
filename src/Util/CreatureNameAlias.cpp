/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CreatureNameAlias.h"

#include "ObjectMgr.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace
{
    struct AliasRow
    {
        char const* name;  // upstream English name, lower case
        uint32 entry;
    };

    AliasRow const kRows[] = {
#include "CreatureNameAliasData.inc"
    };

    struct AliasTables
    {
        std::unordered_map<std::string, std::vector<uint32>> entries;
        std::unordered_map<std::string, std::unordered_set<std::string>> localNames;
    };

    std::string Lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    // Built on first use, which is always after creature templates are loaded (bots only think in
    // world). Read-only afterwards, so map threads can share it.
    AliasTables const& Tables()
    {
        static AliasTables const tables = []
        {
            AliasTables t;
            for (AliasRow const& row : kRows)
            {
                t.entries[row.name].push_back(row.entry);
                if (CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(row.entry))
                    t.localNames[row.name].insert(Lower(tmpl->Name));
            }
            return t;
        }();
        return tables;
    }
}

std::vector<uint32> const* CreatureNameAlias::Entries(std::string const& englishName)
{
    AliasTables const& t = Tables();
    auto itr = t.entries.find(Lower(englishName));
    return itr != t.entries.end() ? &itr->second : nullptr;
}

bool CreatureNameAlias::IsEntryNamed(uint32 entry, std::string const& englishName)
{
    std::vector<uint32> const* entries = Entries(englishName);
    return entries && std::find(entries->begin(), entries->end(), entry) != entries->end();
}

bool CreatureNameAlias::IsLocalNameOf(std::string const& localName, std::string const& englishName)
{
    AliasTables const& t = Tables();
    auto itr = t.localNames.find(Lower(englishName));
    return itr != t.localNames.end() && itr->second.count(Lower(localName));
}
