// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class RenderView;
    class RenderViewFamily;
    class RenderScene;
    struct MaterialInstance;
    class GpuCulling;

    struct alignas(16) GpuSceneInstanceData {
        Vector4f color_tint;       // loc3 RGBA32_FLOAT @0
        Vector4f params;           // loc4 RGBA32_FLOAT @16
        Vector4i transform_index;  // loc5 R32_UINT @32 (x = GpuScene object index)
        Vector4i draw_data;        // loc6 RGBA32_SINT @48 (x/y/z = texture indices)
    };

    class BaselineGBufferPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BindingLayoutHandle m_global_binding_layout{};
        cutie::BindingLayoutHandle m_view_binding_layout{};
        cutie::BindingLayoutHandle m_material_binding_layout{};
        cutie::BindingLayoutHandle m_primitive_binding_layout{};
        cutie::BindingLayoutHandle m_bindless_binding_layout{};
        cutie::BufferHandle m_global_cb{};
        cutie::BufferHandle m_view_cb{};
        cutie::BufferHandle m_primitive_cb{};
        cutie::BufferHandle m_instance_buffer{};
        UInt32 m_instance_capacity{0};
        cutie::BindingSetHandle m_global_binding_set{};
        cutie::BindingSetHandle m_view_binding_set{};
        cutie::BindingSetHandle m_primitive_binding_set{};
        GfxBindingSetHandle m_material_binding_set{};
        GfxBindingSetHandle m_bindless_binding_set{};
        Bool m_material_warning_logged{false};
        Matrix4f m_prev_unjittered_view_projection{1.0f};
        Vector2f m_prev_jitter_uv{0.0f, 0.0f};
        Bool m_has_prev_frame{false};

        cutie::GraphicsPipelineHandle m_pipeline_gpu{};
        cutie::InputLayoutHandle m_input_layout_gpu{};
        cutie::BindingLayoutHandle m_view_gpu_binding_layout{};
        cutie::BindingSetHandle m_view_gpu_binding_set{};
        cutie::IBuffer* m_view_gpu_transforms_rhi{nullptr};
        GfxBufferHandle m_gpu_instance_buffer{};
        UInt32 m_gpu_instance_capacity{0};
        GfxBufferHandle m_candidate_args_buffer{};
        GfxBufferHandle m_arg_to_object_buffer{};
        GfxBufferHandle m_final_args_buffer{};
        UInt32 m_batch_capacity{0};
        GpuCulling* m_gpu_culling{nullptr};
        DrawCommandList m_pre_pass_command_list{};
        Bool m_gpu_scene_warning_logged{false};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void setupView(RenderView& view, RenderViewFamily& view_family);
        void render(RenderView& view, RenderScene& scene,
                    const GfxViewportState& viewport_state, cutie::IFramebuffer* framebuffer);

        [[nodiscard]] cutie::IBuffer* getInstanceBuffer() const { return m_instance_buffer.Get(); }

        void setGpuCulling(GpuCulling* culling) { m_gpu_culling = culling; }
        void resetMotionHistory() { m_has_prev_frame = false; }

    private:
        void ensureInstanceCapacity(UInt32 instance_count);
        void ensureGpuScenePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void ensureGpuInstanceCapacity(UInt32 instance_count);
        void ensureBatchBuffers(UInt32 batch_count);
        void renderCpuScene(RenderView& view, RenderScene& scene,
                            const GfxViewportState& viewport_state, cutie::IFramebuffer* framebuffer);
        void renderGpuScene(RenderView& view, RenderScene& scene,
                            const GfxViewportState& viewport_state, cutie::IFramebuffer* framebuffer,
                            Bool indirect_path);
    };

} // namespace dodoe
