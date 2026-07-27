// do@Redlive

#pragma once

#include "dopch.h"

#include "ui_types.h"

namespace dodoe {

    class Texture2D;

    struct ButtonPreset {
        identifier id{entt::null};
        String name{};
        Texture2D* normal_texture{nullptr};
        Texture2D* hovered_texture{nullptr};
        Texture2D* pressed_texture{nullptr};
        Color normal_color{1, 1, 1, 1};
        Color hovered_color{1, 1, 1, 1};
        Color pressed_color{1, 1, 1, 1};
    };

    class UIPresetManager {
    private:
        UnorderedMap<identifier, ButtonPreset> m_button_presets{};

    public:
        void registerButtonPreset(const ButtonPreset& preset);
        [[nodiscard]] const ButtonPreset* findButtonPreset(identifier id) const;
        void clear();
    };

} // namespace dodoe
