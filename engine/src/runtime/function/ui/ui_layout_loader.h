// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_element.h"
#include "runtime/core/utils/json.h"

namespace dodoe {

    class UIPresetManager;

    class UILayoutLoader {
    public:
        [[nodiscard]] static Scope<UIElement> LoadFromJson(const Json& json);
        [[nodiscard]] static Scope<UIElement> LoadFromJson(const Json& json, UIPresetManager* preset_manager);
        [[nodiscard]] static Scope<UIElement> LoadFromFile(const String& filePath);
        [[nodiscard]] static Scope<UIElement> LoadFromFile(const String& filePath, UIPresetManager* preset_manager);

    private:
        [[nodiscard]] static Scope<UIElement> ParseElement(const Json& node, UIPresetManager* preset_manager);
        [[nodiscard]] static Scope<UIElement> CreateElementByType(const String& type);
    };

} // namespace dodoe
