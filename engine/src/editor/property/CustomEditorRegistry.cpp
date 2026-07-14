// do@Redlive

#include "CustomEditorRegistry.h"

#include "framework/config/EditorConfig.h"

namespace cakery {

CustomEditorRegistry& CustomEditorRegistry::self()
{
    static CustomEditorRegistry instance;
    return instance;
}

void CustomEditorRegistry::registerByName(const std::string& editorName, Factory f)
{
    m_byName[editorName] = std::move(f);
}

void CustomEditorRegistry::mapComponent(const std::string& componentName, const std::string& editorName)
{
    m_comp2name[componentName] = editorName;
}

std::unique_ptr<CustomEditor> CustomEditorRegistry::create(const std::string& componentName) const
{
    auto it = m_comp2name.find(componentName);
    if (it != m_comp2name.end()) {
        auto factoryIt = m_byName.find(it->second);
        if (factoryIt != m_byName.end()) {
            return factoryIt->second();
        }
    }

    auto& inspectors = EditorConfig::self().inspectorsJson();
    if (inspectors.contains("customEditors") && inspectors["customEditors"].contains(componentName)) {
        std::string editorName = inspectors["customEditors"][componentName].get<std::string>();
        auto factoryIt = m_byName.find(editorName);
        if (factoryIt != m_byName.end()) {
            return factoryIt->second();
        }
    }

    return nullptr;
}

} // namespace cakery
