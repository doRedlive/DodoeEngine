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
        Scope<Texture2D> m_fallback{};
        Scope<TextureCubemap> m_fallback_cubemap{};
        UnorderedMap<InstanceID, Scope<Texture2D>> m_texture2d_cache{};
        UnorderedMap<InstanceID, Scope<TextureCubemap>> m_cubemap_cache{};
        UnorderedMap<String, InstanceID> m_cubemap_by_path{};
        std::mutex m_mutex{};
        DynamicArray<InstanceID> m_slot_lut{};

        Bool initialize(const TextureManagerCreateInfo& info);
        void shutdown();

        void createFallbackTexture();

    public:
        Texture2D* createTexture(const String& path, const ObjectID& ref, DrawCommandList& cmd_list, FrameStagingAllocator* staging = nullptr);
        [[nodiscard]] TextureCubemap* loadCubemapTexture(const DynamicArray<String>& face_paths, DrawCommandList& cmd_list, FrameStagingAllocator* staging = nullptr);
        [[nodiscard]] TextureCubemap* loadCubemapTexture(const DynamicArray<String>& face_paths);
        [[nodiscard]] Texture* findTexture(InstanceID id);
        [[nodiscard]] Texture2D* findTexture2D(InstanceID id);
        [[nodiscard]] Texture2D* getFallback() const;
        [[nodiscard]] TextureCubemap* getFallbackCubemap() const;
        [[nodiscard]] DescriptorTableManager* getDescriptorTable() const { return m_descriptor_table; }
        [[nodiscard]] const UnorderedMap<InstanceID, Scope<Texture2D>>& getTexture2DCache() const { return m_texture2d_cache; }
        void removeTexture(InstanceID id);
        [[nodiscard]] Texture2D* resolveSlot(UInt32 slot) const;
    };

} // dodoe
