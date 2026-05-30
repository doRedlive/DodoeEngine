// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../mesh_draw/mesh_pass_processor.h"

namespace dodoe {

    class PickPass : public RenderPass {
        inline static const String kPickColorName = "PickColor";
        inline static const String kPickDepthName = "PickDepth";

        struct PickPassConstants {
            Matrix4f view_projection{1.0f};
        };

        MeshPassProcessor m_mesh_processor;

        rhi::CommandListHandle m_cmd_list{};
        rhi::TextureHandle m_pick_target{};
        rhi::TextureHandle m_depth_target{};
        rhi::FramebufferHandle m_framebuffer{};

    public:
        explicit PickPass(RhiContext* rhi) { m_rhi = rhi; }
        ~PickPass() override = default;

        void setup() override;
        void execute(size_t index) override;
        void cleanup() override;
        void onViewportResize(const Vector2i& viewport_extent) override;

    private:
        void createFramebuffer();
    };

} // dodoe
