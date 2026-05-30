#pragma once

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/input/input.h"

namespace dodoe {

class UIPresetManager;

struct NineSliceMargins {
    float left{0.0f};
    float top{0.0f};
    float right{0.0f};
    float bottom{0.0f};
};

class Image {
public:
    Image() = default;

    Image(std::string_view texture_path, Rect source_rect = {}, bool flipped = false);
    Image(std::string_view texture_path, identifier texture_id, Rect source_rect = {}, bool flipped = false);
    Image(identifier texture_id, Rect source_rect = {}, bool flipped = false);

    [[nodiscard]] std::string_view getTexturePath() const { return texture_path_; }
    [[nodiscard]] identifier getTextureId() const { return texture_id_; }
    [[nodiscard]] const Rect& getSourceRect() const { return source_rect_; }
    [[nodiscard]] bool isFlipped() const { return flipped_; }
    [[nodiscard]] bool hasNineSlice() const { return nine_slice_margins_.has_value(); }
    [[nodiscard]] const std::optional<NineSliceMargins>& getNineSliceMargins() const { return nine_slice_margins_; }

    void setTexture(std::string_view texture_path);
    void setTexture(identifier texture_id);
    void setSourceRect(const Rect& source_rect) { source_rect_ = source_rect; }
    void setFlipped(bool flipped) { flipped_ = flipped; }
    void setNineSliceMargins(const NineSliceMargins& margins);
    void setNineSliceMargins(std::optional<NineSliceMargins> margins);
    void markNineSliceDirty() const { nine_slice_dirty_ = true; }

private:
    std::string texture_path_{};
    identifier texture_id_{entt::null};
    Rect source_rect_{};
    bool flipped_{false};
    std::optional<NineSliceMargins> nine_slice_margins_{};
    mutable bool nine_slice_dirty_{true};
};

struct LayoutOptions {
    float glyph_scale{1.0f};
};

struct TextStyle {
    std::string key{"default_ui"};
    LayoutOptions layout{};
};

struct TextRenderOverrides {
    std::optional<Color> color{};
    std::optional<Color> shadow_color{};
    std::optional<Vector2f> shadow_offset{};
    std::optional<float> glyph_scale{};
    bool shadow_enabled{false};
};

struct ColorOptions {
    Color start_color{Color::white()};
};

class TextRenderer {
public:
    [[nodiscard]] bool hasTextStyle(identifier style_id) const;
    [[nodiscard]] identifier getDefaultUIStyleId() const;
    [[nodiscard]] std::string_view getDefaultUIStyleKey() const;
    [[nodiscard]] std::string_view getTextStyleKey(identifier style_id) const;
    [[nodiscard]] const TextStyle& getTextStyle(identifier style_id) const;
    [[nodiscard]] std::uint64_t getLayoutRevision() const;
    [[nodiscard]] Vector2f getTextSize(std::string_view text,
                                       identifier font_id,
                                       int font_size,
                                       std::string_view font_path,
                                       const LayoutOptions* layout = nullptr) const;

    void drawText(std::string_view text,
                  const Vector2f& position,
                  const Color& color,
                  float font_size,
                  const Vector2f& shadow_offset,
                  std::optional<Color> shadow_color = std::nullopt) const;
};

class InputManagerFacade {
public:
    class Signal {
    public:
        template<auto MemberFn, typename TObject>
        void connect(TObject*) {}

        template<auto MemberFn, typename TObject>
        void disconnect(TObject*) {}
    };

    [[nodiscard]] Vector2f getLogicalMousePosition() const;

    [[nodiscard]] Signal onAction(identifier, ActionState) const { return {}; }
};

class ResourceManagerFacade {
public:
    [[nodiscard]] UIPresetManager& getUIPresetManager() const;
    [[nodiscard]] Vector2f getTextureSize(identifier texture_id, std::string_view texture_path) const;
    void loadSound(identifier sound_id, std::string_view path) const;
};

class AudioPlayerFacade {
public:
    [[nodiscard]] bool playSound(identifier sound_id) const;
};

class RendererFacade {
public:
    void drawUIFilledRect(const Rect& rect, const ColorOptions* params) const;
};

class Context {
public:
    [[nodiscard]] InputManagerFacade& getInputManager() const;
    [[nodiscard]] ResourceManagerFacade& getResourceManager() const;
    [[nodiscard]] TextRenderer& getTextRenderer() const;
    [[nodiscard]] AudioPlayerFacade& getAudioPlayer() const;
    [[nodiscard]] RendererFacade& getRenderer() const;
};

} // namespace dodoe

namespace engine::core {
using Context = dodoe::Context;
}

namespace engine::render {
using Image = dodoe::Image;
using NineSliceMargins = dodoe::NineSliceMargins;
using TextRenderer = dodoe::TextRenderer;
}

namespace engine::utils {
using ColorOptions = dodoe::ColorOptions;
using LayoutOptions = dodoe::LayoutOptions;
using TextRenderOverrides = dodoe::TextRenderOverrides;
}

