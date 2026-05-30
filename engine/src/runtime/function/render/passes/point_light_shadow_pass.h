// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../mesh_draw/mesh_pass_processor.h"

namespace dodoe {

    class PointLightShadowPass : public RenderPass {
        static constexpr UInt32 kMaxPointLightCount = 32;
        static constexpr UInt32 kShadowMapSize = 1024;
        static constexpr UInt32 kShadowLayerCount = kMaxPointLightCount * 2;
        static constexpr UInt32 kMaxShadowGeomVertices = kMaxPointLightCount * 6;

        struct PointLightShadowPassConstants {
            UInt32 point_light_count{0};
            UInt32 padding0{0};
            UInt32 padding1{0};
            UInt32 padding2{0};
            Vector4f point_lights_position_and_radius[kMaxPointLightCount]{};
        };

        MeshPassProcessor m_mesh_processor;

        rhi::CommandListHandle m_cmd_list{};
        rhi::TextureHandle m_shadow_target{};
        rhi::FramebufferHandle m_framebuffer{};
        UInt32 m_active_layer_count{0};

    public:
        explicit PointLightShadowPass(RhiContext* rhi);
        ~PointLightShadowPass() override = default;

        void setup() override;
        void execute(size_t index) override;
        void cleanup() override;
        void onViewportResize(const Vector2i& viewport_extent) override;

    private:
        void createShadowTarget(UInt32 layer_count);
        void createFramebuffer();
    };

} // dodoe
