// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/render_frame/frame_staging_allocator.h"

namespace dodoe {

    class LitMeshProcessor;
    class RenderScene;
    class DrawCommandList;
    class BindingLayoutCache;

    inline constexpr UInt64 kLitPassConstantBufferSize = 272;

    struct LitPassConstantBuffer {
        Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f directional_color_intensity{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f directional_direction_flags{0.0f, 0.0f, 0.0f, 1.0f};
        Matrix4f dir_light_view_projection{1.0f};
        Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
        Vector4f point_light_colors[4]{};
        Vector4f point_light_positions[4]{};
        Vector4f light_count_flags{0.0f, 0.0f, 0.0f, 0.0f};
    };

    void BuildLitPassConstantBuffer(LitPassConstantBuffer& pass_cb,
                                    const RenderScene& scene,
                                    const Vector3f& camera_position);

    GfxBindingLayoutHandle MakeLitPassBindingLayout(BindingLayoutCache& binding_layout_cache);

    GfxBindingSetHandle CreateLitPassBindingSet(DrawCommandList& command_list,
                                                const FrameStagingAllocator::Allocation& allocation,
                                                const GfxTextureHandle& shadow_handle,
                                                const GfxTextureHandle& skybox_texture,
                                                const GfxBindingLayoutHandle& binding_layout);

    class OpaquePass : public IRenderPass {
    public:
        using Produces = TypeList<SceneHdrKey, SceneTexturesKey>;
        using Consumes = TypeList<>;

        explicit OpaquePass(const LitMeshProcessor* processor = nullptr)
            : m_mesh_processor(processor) {}

        RenderPhase getPhase() const override { return RenderPhase::Opaque; }

        DynamicArray<Size_t> getProducedKeys() const override {
            return MakeKeyHashes(Produces{});
        }

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;

    private:
        const LitMeshProcessor* m_mesh_processor;
    };

} // namespace dodoe
