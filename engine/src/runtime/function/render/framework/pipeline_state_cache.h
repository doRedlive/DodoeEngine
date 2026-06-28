// do@Redlive

#pragma once

#include "dopch.h"

#include "../framework/pso_key.h"

namespace dodoe {

    class PipelineStateCache {
        GfxDeviceHandle m_device{};
        mutable UnorderedMap<GraphicsPipelineCacheKey, GfxGraphicsPipelineHandle, GraphicsPipelineCacheKeyHash> m_graphics_pipelines{};

    public:
        explicit PipelineStateCache(const GfxDeviceHandle& device) : m_device(device) { }
        ~PipelineStateCache() = default;

        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const MeshPassType pass_type,
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info) const;
        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info) const;
        void clear();

    private:
        [[nodiscard]] static GraphicsPipelineCacheKey BuildGraphicsPipelineCacheKey(
            const MeshPassType pass_type,
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info);
    };

} // dodoe
