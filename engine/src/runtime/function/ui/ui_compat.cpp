#include "ui_compat.h"

#include "ui_imgui_utils.h"
#include "ui_preset_manager.h"

#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

namespace {

identifier hashOrNull(std::string_view value) {
    if (value.empty()) {
        return entt::null;
    }
    return entt::hashed_string{value.data(), value.size()}.value();
}

TextRenderer g_text_renderer{};
InputManagerFacade g_input_manager{};
ResourceManagerFacade g_resource_manager{};
AudioPlayerFacade g_audio_player{};
RendererFacade g_renderer{};

TextureManager* getTextureManager() {
    auto* render_system = Application::Self().context().render_system.get();
    return render_system ? render_system->getTextureManager() : nullptr;
}

} // namespace

Image::Image(std::string_view texture_path, Rect source_rect, bool flipped)
    : texture_path_(texture_path),
      texture_id_(hashOrNull(texture_path)),
      source_rect_(source_rect),
      flipped_(flipped) {}

Image::Image(std::string_view texture_path, identifier texture_id, Rect source_rect, bool flipped)
    : texture_path_(texture_path),
      texture_id_(texture_id),
      source_rect_(source_rect),
      flipped_(flipped) {}

Image::Image(identifier texture_id, Rect source_rect, bool flipped)
    : texture_id_(texture_id),
      source_rect_(source_rect),
      flipped_(flipped) {}

void Image::setTexture(std::string_view texture_path) {
    texture_path_ = std::string(texture_path);
    texture_id_ = hashOrNull(texture_path);
}

void Image::setTexture(identifier texture_id) {
    texture_id_ = texture_id;
}

void Image::setNineSliceMargins(const NineSliceMargins& margins) {
    nine_slice_margins_ = margins;
    nine_slice_dirty_ = true;
}

void Image::setNineSliceMargins(std::optional<NineSliceMargins> margins) {
    nine_slice_margins_ = std::move(margins);
    nine_slice_dirty_ = true;
}

bool TextRenderer::hasTextStyle(identifier) const {
    return true;
}

identifier TextRenderer::getDefaultUIStyleId() const {
    return entt::hashed_string{"default_ui"}.value();
}

std::string_view TextRenderer::getDefaultUIStyleKey() const {
    return "default_ui";
}

std::string_view TextRenderer::getTextStyleKey(identifier) const {
    return getDefaultUIStyleKey();
}

const TextStyle& TextRenderer::getTextStyle(identifier) const {
    static const TextStyle kDefaultStyle{};
    return kDefaultStyle;
}

std::uint64_t TextRenderer::getLayoutRevision() const {
    return 0;
}

Vector2f TextRenderer::getTextSize(std::string_view text,
                                   identifier,
                                   int font_size,
                                   std::string_view,
                                   const LayoutOptions* layout) const {
    const float scale = layout ? layout->glyph_scale : 1.0f;
    const float line_height = (std::max)(1.0f, static_cast<float>(font_size) * scale);
    const float char_width = (std::max)(1.0f, line_height * 0.5f);

    float current_width = 0.0f;
    float max_width = 0.0f;
    std::size_t line_count = 1;

    for (const char ch : text) {
        if (ch == '\n') {
            max_width = (std::max)(max_width, current_width);
            current_width = 0.0f;
            ++line_count;
            continue;
        }
        current_width += char_width;
    }

    max_width = (std::max)(max_width, current_width);
    return {max_width, line_height * static_cast<float>(line_count)};
}

void TextRenderer::drawText(std::string_view,
                            const Vector2f&,
                            const Color&,
                            float,
                            const Vector2f&,
                            std::optional<Color>) const {
    // The runtime text pipeline is not integrated yet in the current renderer.
}

Vector2f InputManagerFacade::getLogicalMousePosition() const {
    return Input::GetMousePosition();
}

UIPresetManager& ResourceManagerFacade::getUIPresetManager() const {
    return UIPresetManager::Self();
}

Vector2f ResourceManagerFacade::getTextureSize(identifier texture_id, std::string_view texture_path) const {
    auto* texture_manager = getTextureManager();
    if (!texture_manager) {
        return {0.0f, 0.0f};
    }

    Ref<Texture> texture = nullptr;
    if (texture_id != entt::null && !texture_path.empty()) {
        texture = Texture::Load(std::string(texture_path));
    } else if (texture_id != entt::null) {
        texture = texture_manager->findTexture(static_cast<InstanceID>(texture_id));
    } else if (!texture_path.empty()) {
        texture = texture_manager->loadTexture(std::string(texture_path));
    }

    if (!texture) {
        return {0.0f, 0.0f};
    }

    return {static_cast<float>(texture->getWidth()), static_cast<float>(texture->getHeight())};
}

void ResourceManagerFacade::loadSound(identifier, std::string_view) const {
    // Audio resources are not wired into the current runtime yet.
}

bool AudioPlayerFacade::playSound(identifier) const {
    return false;
}

void RendererFacade::drawUIFilledRect(const Rect& rect, const ColorOptions* params) const {
    const Color color = params ? params->start_color : Color::white();
    ui::drawFilledRect(rect, color);
}

InputManagerFacade& Context::getInputManager() const {
    return g_input_manager;
}

ResourceManagerFacade& Context::getResourceManager() const {
    return g_resource_manager;
}

TextRenderer& Context::getTextRenderer() const {
    return g_text_renderer;
}

AudioPlayerFacade& Context::getAudioPlayer() const {
    return g_audio_player;
}

RendererFacade& Context::getRenderer() const {
    return g_renderer;
}

} // namespace dodoe

