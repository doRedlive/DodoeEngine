// do@Redlive

#include "pipeline_state_cache.h"

namespace dodoe {
    GfxGraphicsPipelineHandle PipelineStateCache::resolveGraphicsPipeline(
        const MeshPassType pass_type,
        const GfxGraphicsPipelineDesc& pipeline_desc,
        const GfxFramebufferInfo& framebuffer_info,
        DrawCommandList& command_list) const
    {
        DO_ASSERT(m_device != nullptr, "PipelineStateCache device is null");

        const auto cache_key = BuildGraphicsPipelineCacheKey(pass_type, pipeline_desc, framebuffer_info);
        const auto cache_it = m_graphics_pipelines.find(cache_key);
        if (cache_it != m_graphics_pipelines.end()) {
            return cache_it->second;
        }

        DO_DEBUG("PipelineStateCache: Enqueuing graphics pipeline for pass_type={}, VS={}, PS={}, binding_layouts={}",
            static_cast<int>(pass_type),
            pipeline_desc.VS != nullptr,
            pipeline_desc.PS != nullptr,
            pipeline_desc.bindingLayouts.size());

        auto handle = command_list.createGraphicsPipeline(pipeline_desc, framebuffer_info);
        m_graphics_pipelines.emplace(cache_key, handle);
        return handle;
    }

    GfxGraphicsPipelineHandle PipelineStateCache::resolveGraphicsPipeline(
        const GfxGraphicsPipelineDesc& pipeline_desc,
        const GfxFramebufferInfo& framebuffer_info,
        DrawCommandList& command_list) const
    {
        return resolveGraphicsPipeline(MeshPassType::Count, pipeline_desc, framebuffer_info, command_list);
    }

    void PipelineStateCache::clear() {
        m_graphics_pipelines.clear();
    }

    GraphicsPipelineCacheKey PipelineStateCache::BuildGraphicsPipelineCacheKey(
        const MeshPassType pass_type,
        const GfxGraphicsPipelineDesc& pipeline_desc,
        const GfxFramebufferInfo& framebuffer_info)
    {
        GraphicsPipelineCacheKey key{};
        key.pass_type = pass_type;
        key.primitive_type = pipeline_desc.primType;
        key.patch_control_points = pipeline_desc.patchControlPoints;
        key.input_layout = pipeline_desc.inputLayout.Get();
        key.vertex_shader = pipeline_desc.VS.Get();
        key.hull_shader = pipeline_desc.HS.Get();
        key.domain_shader = pipeline_desc.DS.Get();
        key.geometry_shader = pipeline_desc.GS.Get();
        key.pixel_shader = pipeline_desc.PS.Get();
        key.binding_layouts.reserve(pipeline_desc.bindingLayouts.size());
        for (const auto& binding_layout : pipeline_desc.bindingLayouts) {
            key.binding_layouts.push_back(binding_layout.Get());
        }
        key.render_state = pipeline_desc.renderState;
        key.shading_rate_state = pipeline_desc.shadingRateState;
        key.framebuffer_info = framebuffer_info;
        return key;
    }

} // dodoe
