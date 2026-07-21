// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

    class GBufferPass : public IRenderPass {
        RenderGraphTextureHandle m_albedo{};
        RenderGraphTextureHandle m_normal{};
        RenderGraphTextureHandle m_position{};
        RenderGraphTextureHandle m_material{};
        RenderGraphTextureHandle m_depth{};
        RenderGraphBufferHandle m_primitive_scene_buffer{};
        RenderGraphBufferHandle m_constant_buffer{};

        const class GBufferMeshProcessor* m_mesh_processor{nullptr};

    public:
        GBufferPass() = default;

        [[nodiscard]] const String& getName() const override;
        [[nodiscard]] RenderGraphPassFlags getFlags() const override;

        void setup(RenderGraphPassBuilder& builder,
                   const RenderPassContext& context,
                   const RenderView& view) override;

        void execute(const RenderGraphPassContext& context,
                     DrawCommandList& cmd) override;
    };

} // namespace dodoe
