// do@Redlive

#include "ui_preset_manager.h"

namespace dodoe {

    void UIPresetManager::registerButtonPreset(const ButtonPreset& preset) {
        m_button_presets[preset.id] = preset;
    }

    const ButtonPreset* UIPresetManager::findButtonPreset(identifier id) const {
        auto it = m_button_presets.find(id);
        return it != m_button_presets.end() ? &it->second : nullptr;
    }

    void UIPresetManager::clear() {
        m_button_presets.clear();
    }

} // namespace dodoe
