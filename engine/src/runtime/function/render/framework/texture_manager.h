// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "descriptor_table_manager.h"

namespace dodoe {

    class RhiContext;

    struct Texture {
        identifier id;
        std::string path;
        int width, height;
        rhi::TextureHandle handle;
        DescriptorIndex descriptor_index;
    };

    struct TextureManagerCreateInfo {
        RhiContext* rhi{nullptr};
        DescriptorTableManager* descriptor_table{nullptr};
    };

    class TextureManager {
        RhiContext* rhi_{nullptr};
        DescriptorTableManager* descriptor_table_{nullptr};
        Ref<Texture> fallback_texture_{nullptr};
        std::unordered_map<identifier, Ref<Texture>> texture_umap_{};
    public:
        static Scope<TextureManager> create(const TextureManagerCreateInfo& info);
        static void destroy(Scope<TextureManager>& manager);
 
        [[nodiscard]] Ref<Texture> loadTexture(identifier id, const std::string& path);
        [[nodiscard]] Ref<Texture> loadTexture(const std::string& path);
        [[nodiscard]] Ref<Texture> loadTexture(identifier id);
        [[nodiscard]] Ref<Texture> loadFallbackTexture();

    private:
        bool initialize(const TextureManagerCreateInfo& info);
        void shutdown();

        Ref<Texture> createTexture(const std::string& path);
        void createFallbackTexture();
    };

} // dodoe