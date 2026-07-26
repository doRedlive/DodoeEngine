#include "ui_preset_manager.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

namespace dodoe {

UIPresetManager& UIPresetManager::Self() {
    static UIPresetManager instance;
    return instance;
}

namespace {

[[nodiscard]] float jsonToFloat(const nlohmann::json& value, float fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::number_float_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return static_cast<float>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return static_cast<float>(*v);
    }
    return fallback;
}

[[nodiscard]] int jsonToInt(const nlohmann::json& value, int fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return static_cast<int>(*v);
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        if (*v <= static_cast<nlohmann::json::number_unsigned_t>((std::numeric_limits<int>::max)())) {
            return static_cast<int>(*v);
        }
    }
    return fallback;
}

[[nodiscard]] bool jsonToBool(const nlohmann::json& value, bool fallback) {
    if (const auto* v = value.get_ptr<const nlohmann::json::boolean_t*>()) {
        return *v;
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_integer_t*>()) {
        return *v != 0;
    }
    if (const auto* v = value.get_ptr<const nlohmann::json::number_unsigned_t*>()) {
        return *v != 0;
    }
    return fallback;
}

[[nodiscard]] std::optional<String> jsonToString(const nlohmann::json& value) {
    if (const auto* v = value.get_ptr<const nlohmann::json::string_t*>()) {
        return String(*v);
    }
    return std::nullopt;
}

std::optional<Color> parseColor(const nlohmann::json& value) {
    if (value.is_array() && value.size() == 4) {
        return Color{
            jsonToFloat(value[0], 1.0f),
            jsonToFloat(value[1], 1.0f),
            jsonToFloat(value[2], 1.0f),
            jsonToFloat(value[3], 1.0f)
        };
    }
    if (value.is_object() && value.contains("r") && value.contains("g") && value.contains("b")) {
        const float a = value.contains("a") ? jsonToFloat(value["a"], 1.0f) : 1.0f;
        return Color{
            jsonToFloat(value["r"], 1.0f),
            jsonToFloat(value["g"], 1.0f),
            jsonToFloat(value["b"], 1.0f),
            a
        };
    }
    return std::nullopt;
}

std::optional<Vector2f> parseVec2(const nlohmann::json& value) {
    if (value.is_array() && value.size() == 2) {
        return Vector2f{jsonToFloat(value[0], 0.0f), jsonToFloat(value[1], 0.0f)};
    }
    return std::nullopt;
}

} // namespace

bool UIPresetManager::loadButtonPresets(std::string_view file_path) {
    std::ifstream file{String(file_path)};
    if (!file.is_open()) {
        DO_WARN("UIPresetManager: failed to open button preset file {}.", file_path);
        return false;
    }

    const String file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        DO_ERROR("UIPresetManager: failed to parse button preset JSON: {}.", file_path);
        return false;
    }

    if (!root.is_object()) {
        DO_WARN("UIPresetManager: preset file is invalid (root is not an object).");
        return false;
    }

    button_presets_.clear();
    button_preset_keys_.clear();

    std::size_t loaded_count = 0;
    for (const auto& [key, value] : root.items()) {
        if (!value.is_object()) {
            DO_WARN("UIPresetManager: preset '{}' is invalid (expected object).", key);
            continue;
        }

        UIButtonSkin skin{};
        if (value.contains("images")) {
            const auto& images = value["images"];
            if (images.is_object()) {
                if (images.contains("normal")) {
                    skin.normal_image = parseImageDefinition(images["normal"]);
                }
                if (images.contains("hover")) {
                    skin.hover_image = parseImageDefinition(images["hover"]);
                }
                if (images.contains("pressed")) {
                    skin.pressed_image = parseImageDefinition(images["pressed"]);
                }
                if (images.contains("disabled")) {
                    skin.disabled_image = parseImageDefinition(images["disabled"]);
                }
            }
        }

        if (!skin.normal_image) {
            DO_WARN("UIPresetManager: preset '{}' is missing a normal image and was skipped.", key);
            continue;
        }

        if (value.contains("nine_slice")) {
            skin.nine_slice_margins = parseNineSlice(value["nine_slice"]);
        }

        if (value.contains("label")) {
            skin.normal_label = parseLabelStyle(value["label"]);
        }

        if (value.contains("overrides")) {
            const auto& overrides = value["overrides"];
            if (overrides.is_object()) {
                if (overrides.contains("hover")) {
                    skin.hover_label = parseLabelOverrides(overrides["hover"]);
                }
                if (overrides.contains("pressed")) {
                    skin.pressed_label = parseLabelOverrides(overrides["pressed"]);
                }
                if (overrides.contains("disabled")) {
                    skin.disabled_label = parseLabelOverrides(overrides["disabled"]);
                }
            }
        }

        if (value.contains("sounds")) {
            const auto& sounds = value["sounds"];
            if (sounds.is_object()) {
                for (const auto& [sound_key, sound_value] : sounds.items()) {
                    const identifier event_id = entt::hashed_string{sound_key.c_str(), sound_key.size()};
                    if (event_id == entt::null) {
                        continue;
                    }

                    if (sound_value.is_null()) {
                        skin.sound_events.insert_or_assign(event_id, String{});
                    } else if (sound_value.is_string()) {
                        skin.sound_events.insert_or_assign(event_id, sound_value.get<String>());
                    } else {
                        DO_WARN("UIPresetManager: preset '{}' sounds.{} is invalid (expected string or null).", key, sound_key);
                    }
                }
            }
        }

        const identifier preset_id = entt::hashed_string{key.c_str(), key.size()};
        button_preset_keys_.insert_or_assign(preset_id, key);
        registerButtonPreset(preset_id, std::move(skin));
        ++loaded_count;
    }

    DO_INFO("UIPresetManager: loaded {} button presets.", loaded_count);
    return loaded_count > 0;
}

void UIPresetManager::clearButtonPresets() {
    button_presets_.clear();
    button_preset_keys_.clear();
}

bool UIPresetManager::loadImagePresets(std::string_view file_path) {
    std::ifstream file{String(file_path)};
    if (!file.is_open()) {
        DO_WARN("UIPresetManager: failed to open image preset file {}.", file_path);
        return false;
    }

    const String file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json root = nlohmann::json::parse(file_content, nullptr, false);
    if (root.is_discarded()) {
        DO_ERROR("UIPresetManager: failed to parse image preset JSON: {}.", file_path);
        return false;
    }

    if (!root.is_object()) {
        DO_WARN("UIPresetManager: image preset file is invalid (root is not an object).");
        return false;
    }

    image_presets_.clear();
    image_preset_keys_.clear();

    std::size_t loaded_count = 0;
    for (const auto& [key, value] : root.items()) {
        if (!value.is_object()) {
            DO_WARN("UIPresetManager: image preset '{}' is invalid (expected object).", key);
            continue;
        }

        auto image = parseImageDefinition(value);
        if (!image) {
            DO_WARN("UIPresetManager: image preset '{}' is missing image data and was skipped.", key);
            continue;
        }

        if (value.contains("nine_slice")) {
            if (auto margins = parseNineSlice(value["nine_slice"])) {
                image->setNineSliceMargins(*margins);
            }
        }

        const identifier preset_id = entt::hashed_string{key.c_str(), key.size()};
        image_preset_keys_.insert_or_assign(preset_id, key);
        registerImagePreset(preset_id, std::move(*image));
        ++loaded_count;
    }

    DO_INFO("UIPresetManager: loaded {} image presets.", loaded_count);
    return loaded_count > 0;
}

void UIPresetManager::clearImagePresets() {
    image_presets_.clear();
    image_preset_keys_.clear();
}

const UIButtonSkin* UIPresetManager::getButtonPreset(identifier preset_id) const {
    if (auto it = button_presets_.find(preset_id); it != button_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

const Image* UIPresetManager::getImagePreset(identifier preset_id) const {
    if (auto it = image_presets_.find(preset_id); it != image_presets_.end()) {
        return &it->second;
    }
    DO_WARN("UIPresetManager: image preset {} not found.", entt::to_integral(preset_id));
    return nullptr;
}

UIButtonSkin* UIPresetManager::getButtonPresetMutable(identifier preset_id) {
    if (auto it = button_presets_.find(preset_id); it != button_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

Image* UIPresetManager::getImagePresetMutable(identifier preset_id) {
    if (auto it = image_presets_.find(preset_id); it != image_presets_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<identifier> UIPresetManager::listButtonPresetIds() const {
    std::vector<identifier> ids;
    ids.reserve(button_presets_.size());
    for (const auto& [preset_id, _] : button_presets_) {
        ids.push_back(preset_id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::vector<identifier> UIPresetManager::listImagePresetIds() const {
    std::vector<identifier> ids;
    ids.reserve(image_presets_.size());
    for (const auto& [preset_id, _] : image_presets_) {
        ids.push_back(preset_id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::string_view UIPresetManager::getButtonPresetKey(identifier preset_id) const {
    if (auto it = button_preset_keys_.find(preset_id); it != button_preset_keys_.end()) {
        return it->second;
    }
    return {};
}

std::string_view UIPresetManager::getImagePresetKey(identifier preset_id) const {
    if (auto it = image_preset_keys_.find(preset_id); it != image_preset_keys_.end()) {
        return it->second;
    }
    return {};
}

bool UIPresetManager::registerButtonPreset(identifier preset_id, UIButtonSkin skin, bool overwrite) {
    if (preset_id == entt::null) {
        DO_WARN("UIPresetManager: ignored empty preset id registration.");
        return false;
    }

    if (!skin.normal_image) {
        DO_WARN("UIPresetManager: preset {} is missing a normal image.", preset_id);
        return false;
    }

    if (!overwrite && button_presets_.contains(preset_id)) {
        return false;
    }

    if (skin.nine_slice_margins) {
        if (skin.normal_image) {
            skin.normal_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.hover_image) {
            skin.hover_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.pressed_image) {
            skin.pressed_image->setNineSliceMargins(skin.nine_slice_margins);
        }
        if (skin.disabled_image) {
            skin.disabled_image->setNineSliceMargins(skin.nine_slice_margins);
        }
    }

    button_presets_.insert_or_assign(preset_id, std::move(skin));
    return true;
}

bool UIPresetManager::registerImagePreset(identifier preset_id, Image image, bool overwrite) {
    if (preset_id == entt::null) {
        DO_WARN("UIPresetManager: ignored empty image preset id registration.");
        return false;
    }

    if (image.getTextureId() == entt::null && image.getTexturePath().empty()) {
        DO_WARN("UIPresetManager: image preset {} is missing a valid texture.", preset_id);
        return false;
    }

    if (!overwrite && image_presets_.contains(preset_id)) {
        return false;
    }

    image_presets_.insert_or_assign(preset_id, std::move(image));
    return true;
}

std::optional<Image> UIPresetManager::parseImageDefinition(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    std::optional<String> texture_path{};
    if (auto it = json_value.find("path"); it != json_value.end()) {
        texture_path = jsonToString(*it);
    }

    identifier texture_id = entt::null;
    if (auto it = json_value.find("id"); it != json_value.end()) {
        if (auto id_string = jsonToString(*it); id_string) {
            texture_id = entt::hashed_string{id_string->c_str(), id_string->size()};
        }
    } else if (texture_path) {
        texture_id = entt::hashed_string{texture_path->c_str(), texture_path->size()};
    }

    Rect source_rect{};
    if (json_value.contains("source")) {
        const auto& src = json_value["source"];
        if (src.is_array() && src.size() == 4) {
            const float x = jsonToFloat(src[0], 0.0f);
            const float y = jsonToFloat(src[1], 0.0f);
            const float w = jsonToFloat(src[2], 0.0f);
            const float h = jsonToFloat(src[3], 0.0f);
            if (w > 0.0f && h > 0.0f) {
                source_rect = Rect{{x, y}, {w, h}};
            }
        } else if (src.is_object()) {
            const float x = (src.contains("x") ? jsonToFloat(src["x"], 0.0f) : 0.0f);
            const float y = (src.contains("y") ? jsonToFloat(src["y"], 0.0f) : 0.0f);
            const float w = (src.contains("w") ? jsonToFloat(src["w"], 0.0f) : 0.0f);
            const float h = (src.contains("h") ? jsonToFloat(src["h"], 0.0f) : 0.0f);
            if (w > 0.0f && h > 0.0f) {
                source_rect = Rect{{x, y}, {w, h}};
            }
        }
    }

    const bool flipped = json_value.contains("flipped") ? jsonToBool(json_value["flipped"], false) : false;

    if (texture_path) {
        if (texture_id != entt::null) {
            return Image(*texture_path, texture_id, source_rect, flipped);
        }
        return Image(*texture_path, source_rect, flipped);
    }

    if (texture_id != entt::null) {
        return Image(texture_id, source_rect, flipped);
    }

    return std::nullopt;
}

std::optional<NineSliceMargins> UIPresetManager::parseNineSlice(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }
    NineSliceMargins margins{};
    if (auto it = json_value.find("left"); it != json_value.end()) {
        margins.left = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("top"); it != json_value.end()) {
        margins.top = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("right"); it != json_value.end()) {
        margins.right = jsonToFloat(*it, 0.0f);
    }
    if (auto it = json_value.find("bottom"); it != json_value.end()) {
        margins.bottom = jsonToFloat(*it, 0.0f);
    }
    return margins;
}

std::optional<UIButtonLabelStyle> UIPresetManager::parseLabelStyle(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    UIButtonLabelStyle style{};
    if (auto it = json_value.find("text"); it != json_value.end()) {
        if (auto text = jsonToString(*it); text) {
            style.text = std::move(*text);
        }
    }
    if (auto it = json_value.find("font_path"); it != json_value.end()) {
        if (auto font_path = jsonToString(*it); font_path) {
            style.font_path = std::move(*font_path);
        }
    }
    if (auto it = json_value.find("font_size"); it != json_value.end()) {
        style.font_size = jsonToInt(*it, 16);
    }
    if (json_value.contains("color")) {
        if (auto color = parseColor(json_value["color"])) {
            style.color = *color;
        }
    }
    if (json_value.contains("offset")) {
        if (auto offset = parseVec2(json_value["offset"])) {
            style.offset = *offset;
        }
    }
    return style;
}

std::optional<UIButtonLabelOverrides> UIPresetManager::parseLabelOverrides(const nlohmann::json& json_value) {
    if (!json_value.is_object()) {
        return std::nullopt;
    }

    UIButtonLabelOverrides overrides{};
    bool has_override = false;
    if (json_value.contains("color")) {
        if (auto color = parseColor(json_value["color"])) {
            overrides.color = *color;
            has_override = true;
        }
    }
    if (json_value.contains("offset")) {
        if (auto offset = parseVec2(json_value["offset"])) {
            overrides.offset = *offset;
            has_override = true;
        }
    }
    if (!has_override) {
        return std::nullopt;
    }
    return overrides;
}

} // namespace dodoe

