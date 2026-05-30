#pragma once

#include "dopch.h"
#include "ui_button.h"
#include <nlohmann/json_fwd.hpp>

namespace dodoe {

class UIPresetManager final {
    std::unordered_map<identifier, UIButtonSkin> button_presets_{};
    std::unordered_map<identifier, Image> image_presets_{};
    std::unordered_map<identifier, std::string> button_preset_keys_{};
    std::unordered_map<identifier, std::string> image_preset_keys_{};

    static std::optional<Image> parseImageDefinition(const nlohmann::json& json_value);
    static std::optional<NineSliceMargins> parseNineSlice(const nlohmann::json& json_value);
    static std::optional<UIButtonLabelStyle> parseLabelStyle(const nlohmann::json& json_value);
    static std::optional<UIButtonLabelOverrides> parseLabelOverrides(const nlohmann::json& json_value);

public:
    static UIPresetManager& Self();

    UIPresetManager() = default;

    bool loadButtonPresets(std::string_view file_path);
    bool loadImagePresets(std::string_view file_path);
    void clearButtonPresets();
    void clearImagePresets();

    [[nodiscard]] const UIButtonSkin* getButtonPreset(identifier preset_id) const;
    [[nodiscard]] const Image* getImagePreset(identifier preset_id) const;
    [[nodiscard]] UIButtonSkin* getButtonPresetMutable(identifier preset_id);
    [[nodiscard]] Image* getImagePresetMutable(identifier preset_id);
    [[nodiscard]] std::vector<identifier> listButtonPresetIds() const;
    [[nodiscard]] std::vector<identifier> listImagePresetIds() const;
    [[nodiscard]] std::string_view getButtonPresetKey(identifier preset_id) const;
    [[nodiscard]] std::string_view getImagePresetKey(identifier preset_id) const;
    bool registerButtonPreset(identifier preset_id, UIButtonSkin skin, bool overwrite = true);
    bool registerImagePreset(identifier preset_id, Image image, bool overwrite = true);
};

} // namespace dodoe

