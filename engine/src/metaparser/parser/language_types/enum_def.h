#pragma once

#include "cursor/cursor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct EnumDef
{
    explicit EnumDef(const Cursor& cursor);

    std::string m_name;
    std::vector<std::pair<std::string, int>> m_values;
};

extern std::unordered_map<std::string, std::shared_ptr<EnumDef>> g_enum_table;
