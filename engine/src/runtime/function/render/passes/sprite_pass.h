// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "../render_resource.h"
#include "../mesh_draw/mesh_pass_processor.h"

namespace dodoe {

    class DescriptorTableManager;

    class SpritePass : public RenderPass {
        inline static const std::string kInputSceneColorResourceName = "MainCameraColor";
        inline static const std::string kInputSceneDepthResourceName = "MainCameraDepth";

        DescriptorTableManager* m_descriptor_table{nullptr};
        MeshPassProcessor m_mesh_processor;

        rhi::TextureHandle m_scene_color_target{};
        rhi::TextureHandle m_scene_depth_target{};
        rhi::FramebufferHandle m_framebuffer{};
        rhi::BufferHandle m_vertex_buffer{};
        rhi::BufferHandle m_index_buffer{};
        rhi::SamplerHandle m_sampler{};
        rhi::CommandListHandle m_cmd_list{};

    public:
        SpritePass(RhiContext* rhi, DescriptorTableManager* descriptor_table);

        void setup() override;
        void execute(size_t index) override;
        void cleanup() override;
        void onViewportResize(const Vector2i& viewport_extent) override;

    private:
        void createFramebuffer();
        void createBuffers();
    };

} // dodoe
