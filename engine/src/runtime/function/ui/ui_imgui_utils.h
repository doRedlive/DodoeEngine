#pragma once

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/function/render/framework/texture_manager.h"

namespace engine::render {
    class Image;
}

namespace dodoe::ui {

[[nodiscard]] inline Ref<Texture> resolveTexture(const engine::render::Image& image) {
    auto* render_system = Application::Self().context().render_system.get();
    auto* texture_manager = render_system ? render_system->getTextureManager() : nullptr;
    if (!texture_manager) {
        return nullptr;
    }

    Ref<Texture> texture = nullptr;
    if (image.getTextureId() != entt::null && !image.getTexturePath().empty()) {
        texture = texture_manager->loadTexture(image.getTextureId(), std::string(image.getTexturePath()));
    } else if (image.getTextureId() != entt::null) {
        texture = texture_manager->loadTexture(image.getTextureId());
    } else if (!image.getTexturePath().empty()) {
        texture = texture_manager->loadTexture(std::string(image.getTexturePath()));
    }

    return texture;
}

    [[nodiscard]] inline Vector4f resolveUvRect(const engine::render::Image& image) {
        Vector4f uv{0.0f, 0.0f, 1.0f, 1.0f};
        const auto texture = resolveTexture(image);
        if (!texture) {
            return uv;
        }

        const auto source_rect = image.getSourceRect();
        if (source_rect.size.x <= 0.0f || source_rect.size.y <= 0.0f) {
            return uv;
        }

        if (texture->width <= 0 || texture->height <= 0) {
            return uv;
        }

        uv = {
            source_rect.pos.x / static_cast<float>(texture->width),
            source_rect.pos.y / static_cast<float>(texture->height),
            (source_rect.pos.x + source_rect.size.x) / static_cast<float>(texture->width),
            (source_rect.pos.y + source_rect.size.y) / static_cast<float>(texture->height)
        };

        if (image.isFlipped()) {
            std::swap(uv.x, uv.z);
        }

        return uv;
    }

    inline void drawFilledRect(const Rect& rect,
                               const Color& color,
                               float /*rounding*/ = 0.0f) {
        Renderer2d::drawSprite(0, rect.pos, rect.size, Vector3f{0.0f, 0.0f, 0.0f}, color);
    }

inline void drawImage(const engine::render::Image& image,
                      const Vector2f& position,
                      const Vector2f& size,
                      const Color& tint = Color::white()) {
        auto texture = resolveTexture(image);
        if (!texture) {
            return;
        }

        Renderer2d::drawSprite(
            texture->id,
            position,
            size,
            Vector3f{0.0f, 0.0f, 0.0f},
            resolveUvRect(image),
            tint
        );
}

inline void drawImageForeground(const engine::render::Image& image,
                                const Vector2f& position,
                                const Vector2f& size,
                                const Color& tint = Color::white()) {
    drawImage(image, position, size, tint);
}

} // namespace dodoe::ui