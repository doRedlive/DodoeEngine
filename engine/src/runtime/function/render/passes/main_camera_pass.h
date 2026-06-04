// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../render_pass.h"
#include "../render_resource.h"
#include "../framework/descriptor_table_manager.h"
#include "../mesh_draw/mesh_pass_processor.h"

#include "runtime/function/render/framework/material.h"

namespace dodoe {

    class MainCameraPass : public RenderPass {
        inline static const String kSceneAlbedoName = "MainCameraAlbedo";
        inline static const String kSceneNormalName = "MainCameraNormal";
        inline static const String kScenePositionName = "MainCameraPosition";
        inline static const String kSceneMaterialName = "MainCameraMaterial";
        inline static const String kSceneDepthName = "MainCameraDepth";

        DescriptorTableManager* m_descriptor_table{nullptr};

        MeshPassProcessor m_mesh_processor;

        rhi::SamplerHandle m_sampler{};
        rhi::CommandListHandle m_cmd_list{};

        rhi::TextureHandle m_albedo_target{};
        rhi::TextureHandle m_normal_target{};
        rhi::TextureHandle m_position_target{};
        rhi::TextureHandle m_material_target{};
        rhi::TextureHandle m_depth_target{};
        rhi::FramebufferHandle m_framebuffer{};

        Matrix4f m_cached_view_projection{1.0f};

    public:
        MainCameraPass(RhiContext* rhi, DescriptorTableManager* descriptor_manager);
        ~MainCameraPass() override = default;

        void setup() override;
        void execute(size_t index) override;
        void cleanup() override;
        void onViewportResize(const Vector2i& viewport_extent) override;

    private:
        void createFramebuffer();
        UInt32 resolveTextureIndex(const Ref<Material>& material) const;
        UInt32 resolveMetallicRoughnessTextureIndex(const Ref<Material>& material) const;
    };

} // dodoe
