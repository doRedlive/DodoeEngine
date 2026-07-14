// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GpuCullingCreateInfo {
        GfxContext* gfx_context{nullptr};
        ShaderLibrary* shader_library{nullptr};
    };

    class GpuCulling : public Managed<GpuCulling, GpuCullingCreateInfo> {
        friend class Managed<GpuCulling, GpuCullingCreateInfo>;

        GfxContext* m_gfx{nullptr};
        ShaderLibrary* m_shader_library{nullptr};

        GfxComputePipelineHandle m_culling_pipeline{};
        GfxBindingLayoutHandle m_culling_binding_layout{};

        GfxBufferHandle m_visible_objects_buffer{};
        GfxBufferHandle m_visible_count_buffer{};
        GfxBufferHandle m_culling_params_buffer{};
        GfxBufferHandle m_indirect_args_buffer{};

        static constexpr UInt32 kMaxFramesInFlight = 2;
        UInt32 m_frame_index{0};
        Bool m_enabled{false};

    public:
        void setEnabled(Bool enabled) { m_enabled = enabled; }
        [[nodiscard]] Bool isEnabled() const { return m_enabled; }

        void executeCulling(DrawCommandList& cmd_list,
                           const GpuScenePassResources& scene_resources,
                           const Matrix4f& view_projection);

        [[nodiscard]] GfxBufferHandle getVisibleObjectsBuffer() const { return m_visible_objects_buffer; }
        [[nodiscard]] GfxBufferHandle getIndirectArgsBuffer() const { return m_indirect_args_buffer; }

    private:
        Bool initialize(const GpuDrivenRendererCreateInfo& info);
        void shutdown();
    };

} // dodoe