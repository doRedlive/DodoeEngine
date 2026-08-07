#include "common/precompiled.h"

#include "language_types/enum_def.h"

EnumDef::EnumDef(const Cursor& cursor) : m_name(cursor.getSpelling())
{
    Utils::replaceAll(m_name, "dodoe::", "");
    Utils::replaceAll(m_name, " ", "");

    for (const auto& child : cursor.getChildren())
    {
        if (child.getKind() != CXCursor_EnumConstantDecl)
        {
            continue;
        }
        long long value = clang_getEnumConstantDeclValue(child.getHandle());
        m_values.emplace_back(child.getSpelling(), static_cast<int>(value));
    }
}

std::unordered_map<std::string, std::shared_ptr<EnumDef>> g_enum_table;
