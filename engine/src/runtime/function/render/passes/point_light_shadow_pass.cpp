// do@Redlive

#include "point_light_shadow_pass.h"

#include "../render_resource.h"
#include "../framework/scene_graph.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 128;
    }

    PointLightShadowPass::PointLightShadowPass(RhiContext* rhi) {
        m_rhi = rhi;
    }

    void PointLightShadowPass::setup() {
        MeshPipelineStateDesc desc;
        desc.vertex_shader_path = "engine/res/shaders/bin/point_light_shadow_pass.vert.spv";
        desc.geometry_shader_path = "engine/res/shaders/bin/point_light_shadow_pass.geom.spv";
        desc.pixel_shader_path  = "engine/res/shaders/bin/point_light_shadow_pass.frag.spv";
        desc.constant_buffer_size = sizeof(PointLightShadowPassConstants);
        desc.constant_buffer_max_versions = kVolatileConstantBufferVersions;
        desc.debug_name = "PointLightShadowPass";

        rhi::DepthStencilState ds;
        ds.enableDepthTest().enableDepthWrite()
          .setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
        rhi::RasterState rs;
        rs.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(8).setSlopeScaleDepthBias(2.0f);
        desc.render_state.setDepthStencilState(ds).setRasterState(rs);

        m_mesh_processor.initialize(m_rhi, desc);

        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void PointLightShadowPass::execute(size_t index) {
        (void)index;

        auto* render_scene = g_RenderResource ? &g_RenderResource->getRenderScene() : nullptr;
        auto scene_graph = render_scene ? render_scene->getSceneGraph() : nullptr;
        if (!scene_graph) {
            return;
        }

        std::vector<Ref<PointLight>> point_lights;
        std::vector<Vector3f> point_light_positions;
        point_lights.reserve(kMaxPointLightCount);
        point_light_positions.reserve(kMaxPointLightCount);
        for (const auto& light_leaf : scene_graph->getLights()) {
            if (const auto point_light = std::dynamic_pointer_cast<PointLight>(light_leaf)) {
                if (!point_light->getNode()) {
                    continue;
                }
                point_lights.push_back(point_light);
                point_light_positions.push_back(point_light->getNodePtr()->getTranslation());
                if (point_lights.size() >= kMaxPointLightCount) {
                    break;
                }
            }
        }

        const UInt32 point_light_count = static_cast<UInt32>(point_lights.size());
        if (point_light_count == 0) {
            return;
        }

        const UInt32 layer_count = point_light_count * 2;
        if (!m_shadow_target || m_active_layer_count != layer_count) {
            createShadowTarget(layer_count);
            createFramebuffer();
            m_mesh_processor.invalidatePipeline();
        }

        m_mesh_processor.createGraphicsPipeline(m_framebuffer);

        if (!m_shadow_target || !m_framebuffer) {
            return;
        }

        m_cmd_list->open();
        m_cmd_list->beginMarker("PointLightShadowPass");
        m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
        m_cmd_list->commitBarriers();
        m_cmd_list->clearDepthStencilTexture(m_shadow_target, rhi::AllSubresources, true, 1.0f, false, 0);

        render_scene->prepareBuffers(m_cmd_list);

        PointLightShadowPassConstants constants{};
        constants.point_light_count = point_light_count;
        for (UInt32 i = 0; i < point_light_count; ++i) {
            const auto& point_light = point_lights[i];
            const auto& translation = point_light_positions[i];
            constants.point_lights_position_and_radius[i] =
                Vector4f(translation.x, translation.y, translation.z, point_light->radius);
        }
        m_cmd_list->writeBuffer(m_mesh_processor.getConstantBuffer(), &constants, sizeof(constants));

        const auto& instances = render_scene->mainCameraInstances();
        auto batches = m_mesh_processor.buildMeshBatches(instances);
        auto commands = m_mesh_processor.buildDrawCommands(batches, m_cmd_list);
        m_mesh_processor.submitDrawCommands(
            commands, m_framebuffer,
            Vector2i(static_cast<Int32>(kShadowMapSize), static_cast<Int32>(kShadowMapSize)),
            m_cmd_list);

        m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->commitBarriers();
        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void PointLightShadowPass::cleanup() {
    }

    void PointLightShadowPass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        if (m_shadow_target) {
            createFramebuffer();
        }
        m_mesh_processor.invalidatePipeline();
    }

    void PointLightShadowPass::createShadowTarget(UInt32 layer_count) {
        m_shadow_target = nullptr;
        m_active_layer_count = layer_count;
        if (layer_count == 0) {
            return;
        }

        auto texture_desc = rhi::TextureDesc()
            .setDimension(rhi::TextureDimension::Texture2DArray)
            .setWidth(kShadowMapSize)
            .setHeight(kShadowMapSize)
            .setArraySize(layer_count)
            .setFormat(rhi::Format::D32)
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::DepthWrite)
            .setDebugName("PointLightShadowPass Depth Target");
        m_shadow_target = m_rhi->getDevice()->createTexture(texture_desc);
    }

    void PointLightShadowPass::createFramebuffer() {
        m_framebuffer = nullptr;
        if (!m_shadow_target) {
            return;
        }

        auto framebuffer_desc = rhi::FramebufferDesc().setDepthAttachment(m_shadow_target);
        m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
    }

} // dodoe
