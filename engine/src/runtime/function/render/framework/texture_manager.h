// do@Redlive

#pragma once

#include "dopch.h"

#include "texture.h"

namespace dodoe {

    class RhiContext;

    struct TextureManagerCreateInfo {
        RhiContext* rhi{nullptr};
        DescriptorTableManager* descriptor_table{nullptr};
    };

    class TextureManager : public Managed<TextureManager, TextureManagerCreateInfo> {
        friend class Managed<TextureManager, TextureManagerCreateInfo>;

        RhiContext* m_rhi{nullptr};
        DescriptorTableManager* m_descriptor_table{nullptr};
        Ref<Texture> m_fallback{};
        UnorderedMap<InstanceID, Ref<Texture>> m_texture_cache{};

        Bool initialize(const TextureManagerCreateInfo& info);
        void shutdown();

        Ref<Texture> createTexture(const String& path);
        void createFallbackTexture();

    public:
        [[nodiscard]] Ref<Texture> loadTexture(const String& path);
        [[nodiscard]] Ref<Texture> findTexture(InstanceID id);
        [[nodiscard]] Ref<Texture> getFallback() const;
        void removeTexture(InstanceID id);
    };

} // dodoe
