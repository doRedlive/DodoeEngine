// do@Redlive

#pragma once

#include "dopch.h"

#include "texture.h"
#include "runtime/function/render/render_frame/frame_staging_allocator.h"
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
        GfxDeviceHandle m_device{};
        ObjHandle<Texture2D> m_fallback{};
        UnorderedMap<InstanceID, ObjHandle<Texture2D>> m_texture2d_cache{};
        UnorderedMap<InstanceID, ObjHandle<TextureCubemap>> m_cubemap_cache{};
        std::mutex m_mutex{};

        Bool initialize(const TextureManagerCreateInfo& info);
        void shutdown();

        Texture2D* createTexture(const String& path, DrawCommandList& cmd_list, FrameStagingAllocator* staging = nullptr);
        void createFallbackTexture();

    public:
        [[nodiscard]] Texture2D* loadTexture(const String& path, DrawCommandList& cmd_list, FrameStagingAllocator* staging = nullptr);
        [[nodiscard]] Texture2D* loadTexture(const String& path);
        [[nodiscard]] TextureCubemap* loadCubemapTexture(const DynamicArray<String>& face_paths, DrawCommandList& cmd_list, FrameStagingAllocator* staging = nullptr);
        [[nodiscard]] TextureCubemap* loadCubemapTexture(const DynamicArray<String>& face_paths);
        [[nodiscard]] Texture* findTexture(InstanceID id);
        [[nodiscard]] Texture2D* findTexture2D(InstanceID id);
        [[nodiscard]] Texture2D* getFallback() const;
        [[nodiscard]] DescriptorTableManager* getDescriptorTable() const { return m_descriptor_table; }
        [[nodiscard]] const UnorderedMap<InstanceID, ObjHandle<Texture2D>>& getTexture2DCache() const { return m_texture2d_cache; }
        void removeTexture(InstanceID id);
    };

} // dodoe
