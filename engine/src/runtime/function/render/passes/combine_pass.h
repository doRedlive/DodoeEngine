// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../interface/rhi_context.h"
#include "../render_pass.h"

namespace dodoe {

    class CombinePass : public RenderPass {
        inline static const std::string kInputSceneColorResourceName = "MainCameraColor";
        inline static const std::string kInputImGuiColorResourceName = "ImGuiColor";

        rhi::ShaderHandle m_vertex_shader{};
        rhi::ShaderHandle m_pixel_shader{};
        rhi::TextureHandle m_scene_target{};
        rhi::TextureHandle m_imgui_target{};
        rhi::SamplerHandle m_sampler{};
        rhi::CommandListHandle m_cmd_list{};
        rhi::BindingLayoutHandle m_binding_layout{};
        rhi::BindingSetHandle m_binding_set{};
        rhi::GraphicsPipelineHandle m_graphics_pipeline{};
        std::vector<rhi::FramebufferHandle> m_framebuffers{};
    public:
        explicit CombinePass(RhiContext* rhi) { m_rhi = rhi; }
        ~CombinePass() override = default;

        void setup() override;
        void execute(size_t index) override;
        void cleanup() override;
        void onViewportResize(const Vector2i& viewport_extent) override;
        void onWindowResize(const Vector2i& window_extent) override;

    private:
        void refreshInputResources();
        void createShaders();
        void createFramebuffers();
        void createGraphicsPipeline();
        void createBindingLayout();
        void createBindingSet();
        void createSampler();
    };

} // dodoe
