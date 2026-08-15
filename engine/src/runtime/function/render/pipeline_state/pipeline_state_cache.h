// do@Redlive

#pragma once

#include "dopch.h"

#include "pso_key.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    struct PipelineStateCacheCreateInfo {
        GfxDeviceHandle device{};
    };

    class PipelineStateCache : public Managed<PipelineStateCache, PipelineStateCacheCreateInfo> {
        friend class Managed<PipelineStateCache, PipelineStateCacheCreateInfo>;
        GfxDeviceHandle m_device{};
        mutable UnorderedMap<GraphicsPipelineCacheKey, GfxGraphicsPipelineHandle, GraphicsPipelineCacheKeyHash> m_graphics_pipelines{};

    public:
        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const MeshPassType pass_type,
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list) const;
        [[nodiscard]] GfxGraphicsPipelineHandle resolveGraphicsPipeline(
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list) const;
        void clear();

    private:
        Bool initialize(const PipelineStateCacheCreateInfo& info) { m_device = info.device; return m_device != nullptr; }
        void shutdown() { clear(); m_device = nullptr; }

        [[nodiscard]] static GraphicsPipelineCacheKey BuildGraphicsPipelineCacheKey(
            const MeshPassType pass_type,
            const GfxGraphicsPipelineDesc& pipeline_desc,
            const GfxFramebufferInfo& framebuffer_info);
    };

} // dodoe
