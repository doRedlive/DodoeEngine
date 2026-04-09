// do->Redlive

#pragma once

#include "interface/rhi.h"

#include "runtime/resource/resource_type.h"

namespace dodoe {

    class TextureManager {
        rhi::DeviceHandle device_{};
        std::unordered_map<identifier, rhi::TextureHandle> texture_umap_{};
        rhi::TextureHandle fallback_texture_{};
    public:
        static TextureManager& self();

        void initialize(rhi::DeviceHandle device);
        void shutdown();

        rhi::TextureHandle createTexture(const TextureRes& res);
        [[nodiscard]] rhi::TextureHandle getTexture(identifier texture_id, const TextureRes& res);
        [[nodiscard]] rhi::TextureHandle getTexture(const TextureRes& res);
        [[nodiscard]] rhi::TextureHandle getFallbackTexture();
    };

} // dodoe