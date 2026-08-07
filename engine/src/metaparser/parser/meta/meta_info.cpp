#include "common/precompiled.h"

#include "parser/parser.h"

#include "meta_info.h"

MetaInfo::MetaInfo(const Cursor& cursor)
{
    for (auto& child : cursor.getChildren())
    {

        if (child.getKind() != CXCursor_AnnotateAttr)
            continue;

        for (auto& prop : extractProperties(child))
            m_properties[prop.first] = prop.second;
    }
}

std::string MetaInfo::getProperty(const std::string& key) const
{
    auto search = m_properties.find(key);

    // use an empty string by default
    return search == m_properties.end() ? "" : search->second;
}

bool MetaInfo::getFlag(const std::string& key) const { return m_properties.find(key) != m_properties.end(); }

std::vector<MetaInfo::Property> MetaInfo::extractProperties(const Cursor& cursor) const
{
    std::vector<Property> ret_list;

    auto propertyList = cursor.getDisplayName();

    static const std::string white_space_string = " \t\r\n";

    auto splitTopLevel = [](const std::string& input) -> std::vector<std::string> {
        std::vector<std::string> out;
        int                      depth  = 0;
        size_t                   start  = 0;
        for (size_t i = 0; i < input.size(); ++i)
        {
            char c = input[i];
            if (c == '(' || c == '[' || c == '{')
                ++depth;
            else if (c == ')' || c == ']' || c == '}')
                --depth;
            else if (c == ',' && depth == 0)
            {
                out.push_back(input.substr(start, i - start));
                start = i + 1;
            }
        }
        out.push_back(input.substr(start));
        return out;
    };

    for (auto& property_item : splitTopLevel(propertyList))
    {
        std::string item = Utils::trim(property_item, white_space_string);
        if (item.empty())
        {
            continue;
        }

        std::string key, value;
        size_t      open_paren = item.find('(');
        size_t      colon      = item.find(':');

        if (open_paren != std::string::npos && item.back() == ')')
        {
            key   = Utils::trim(item.substr(0, open_paren), white_space_string);
            value = Utils::trim(item.substr(open_paren + 1, item.size() - open_paren - 2), white_space_string);
        }
        else if (colon != std::string::npos)
        {
            key   = Utils::trim(item.substr(0, colon), white_space_string);
            value = Utils::trim(item.substr(colon + 1), white_space_string);
        }
        else
        {
            key   = item;
            value = "";
        }

        if (key.empty())
        {
            continue;
        }

        Utils::replaceAll(value, "\"", "");

        ret_list.emplace_back(key, value);
    }
    return ret_list;
}
