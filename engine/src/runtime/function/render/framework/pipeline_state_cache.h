// do@Redlive

#pragma once

#include "dopch.h"

#include "../framework/pso_key.h"

namespace dodoe {

    class PipelineStateCache {
        GfxDeviceHandle m_device{};
        UnorderedMap<GraphicsPipelineCacheKey, GfxGraphicsPipelineHandle, GraphicsPipelineCacheKeyHash> m_graphics_pipelines{};

    public:
        explicit PipelineStateCache(const GfxDeviceHandle& device) : m_device(device) { }
        ~PipelineStateCache() = default;

        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const MeshPassType pass_type,
            const GraphicsPipelineDesc& pipeline_desc,
            const nvrhi::FramebufferInfo& framebuffer_info);
        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const GraphicsPipelineDesc& pipeline_desc,
            const nvrhi::FramebufferInfo& framebuffer_info);
        void clear();

    private:
        [[nodiscard]] static GraphicsPipelineCacheKey BuildGraphicsPipelineCacheKey(
            const MeshPassType pass_type,
            const GraphicsPipelineDesc& pipeline_desc,
            const nvrhi::FramebufferInfo& framebuffer_info);
    };

} // dodoe
