#pragma once

#include "dopch.h"
#include "ui_compat.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/framework/texture_manager.h"

namespace dodoe::ui {

[[nodiscard]] inline Ref<Texture> resolveTexture(const Image& image) {
    auto* texture_manager = GetRenderSystem()->getTextureManager();
    if (!texture_manager) {
        return nullptr;
    }

    Ref<Texture> texture = nullptr;
    if (image.getTextureId() != entt::null && !image.getTexturePath().empty()) {
        texture = Texture::Load(std::string(image.getTexturePath()));
    } else if (image.getTextureId() != entt::null) {
        texture = texture_manager->findTexture(static_cast<InstanceID>(image.getTextureId()));
    } else if (!image.getTexturePath().empty()) {
        texture = texture_manager->loadTexture(std::string(image.getTexturePath()));
    }

    return texture;
}

    [[nodiscard]] inline Vector4f resolveUvRect(const Image& image) {
        Vector4f uv{0.0f, 0.0f, 1.0f, 1.0f};
        const auto texture = resolveTexture(image);
        if (!texture) {
            return uv;
        }

        const auto source_rect = image.getSourceRect();
        if (source_rect.size.x <= 0.0f || source_rect.size.y <= 0.0f) {
            return uv;
        }

        if (texture->getWidth() <= 0 || texture->getHeight() <= 0) {
            return uv;
        }

        uv = {
            source_rect.pos.x / static_cast<float>(texture->getWidth()),
            source_rect.pos.y / static_cast<float>(texture->getHeight()),
            (source_rect.pos.x + source_rect.size.x) / static_cast<float>(texture->getWidth()),
            (source_rect.pos.y + source_rect.size.y) / static_cast<float>(texture->getHeight())
        };

        if (image.isFlipped()) {
            std::swap(uv.x, uv.z);
        }

        return uv;
    }

    inline void drawFilledRect(const Rect& rect,
                               const Color& color,
                               Float /*rounding*/ = 0.0f) {
        (void)rect; (void)color;
    }

    inline void drawImage(const Image& image,
                          const Vector2f& position,
                          const Vector2f& size,
                          const Color& tint = Color::white()) {
        (void)image; (void)position; (void)size; (void)tint;
    }

inline void drawImageForeground(const Image& image,
                                const Vector2f& position,
                                const Vector2f& size,
                                const Color& tint = Color::white()) {
    drawImage(image, position, size, tint);
}

} // namespace dodoe::ui

