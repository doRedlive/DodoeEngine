// do@Redlive

#pragma once

#include "dopch.h"

#include "texture.h"
#include "runtime/function/graphics/draw_command_list.h"

#include <mutex>

namespace dodoe {

    class GfxContext;

    struct TextureManagerCreateInfo {
        GfxContext* gfx{nullptr};
        DescriptorTableManager* descriptor_table{nullptr};
    };

    class TextureManager : public Managed<TextureManager, TextureManagerCreateInfo> {
        friend class Managed<TextureManager, TextureManagerCreateInfo>;

        GfxContext* m_gfx{nullptr};
        DescriptorTableManager* m_descriptor_table{nullptr};
        Ref<Texture> m_fallback{};
        UnorderedMap<InstanceID, Ref<Texture>> m_texture_cache{};
        std::mutex m_mutex{};

        Bool initialize(const TextureManagerCreateInfo& info);
        void shutdown();

        Ref<Texture> createTexture(const String& path);
        void createFallbackTexture();

    public:
        [[nodiscard]] Ref<Texture> loadTexture(const String& path);
        [[nodiscard]] Ref<Texture> findTexture(InstanceID id);
        [[nodiscard]] Ref<Texture> getFallback() const;
        [[nodiscard]] DescriptorTableManager* getDescriptorTable() const { return m_descriptor_table; }
        void removeTexture(InstanceID id);

        void flushPendingCommands();
    };

} // dodoe
