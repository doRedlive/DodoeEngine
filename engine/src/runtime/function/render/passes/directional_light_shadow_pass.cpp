// do@Redlive

#include "directional_light_shadow_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"
#include "../framework/scene_graph.h"

#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 128;

        Vector3f BuildDirectionalLightDirection(const Ref<SceneGraphNode>& light_node) {
            constexpr Vector3f kFallbackDirection = Vector3f(0.3f, 0.8f, 0.5f);
            if (!light_node) {
                return glm::normalize(kFallbackDirection);
            }

            const auto light_matrix = light_node->getGlobalMatrix();
            Vector3f direction = Vector3f(light_matrix * Vector4f(0.0f, 0.0f, -1.0f, 0.0f));
            if (glm::length(direction) < 0.0001f) {
                return glm::normalize(kFallbackDirection);
            }
            return glm::normalize(direction);
        }

        Matrix4f BuildDirectionalLightViewProjection(const Vector3f& light_dir) {
            const Vector3f light_eye = Vector3f(0.0f, 0.0f, 0.0f) - light_dir * 30.0f;
            const Matrix4f view = glm::lookAt(light_eye, Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
            const Matrix4f proj = glm::orthoRH_ZO(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 80.0f);
            return proj * view;
        }

    }

    DirectionalLightShadowPass::DirectionalLightShadowPass(RhiContext* rhi) {
        m_rhi = rhi;
    }

    void DirectionalLightShadowPass::setup() {
        createFramebuffer();

        MeshPipelineStateDesc desc;
        desc.vertex_shader_path = "engine/res/shaders/bin/directional_light_shadow_pass.vert.spv";
        desc.pixel_shader_path  = "engine/res/shaders/bin/directional_light_shadow_pass.frag.spv";
        desc.constant_buffer_size = sizeof(DirectionalLightShadowPassConstants);
        desc.constant_buffer_max_versions = kVolatileConstantBufferVersions;
        desc.debug_name = "DirectionalLightShadowPass";

        rhi::DepthStencilState ds;
        ds.enableDepthTest().enableDepthWrite()
          .setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
        rhi::RasterState rs;
        rs.setCullBack().setDepthBiasClamp(0.0f).setDepthBias(6).setSlopeScaleDepthBias(1.5f);
        desc.render_state.setDepthStencilState(ds).setRasterState(rs);

        m_mesh_processor.initialize(m_rhi, desc);

        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void DirectionalLightShadowPass::execute(size_t index) {
        (void)index;

        auto* render_scene = g_RenderResource ? &g_RenderResource->getRenderScene() : nullptr;
        auto scene_graph = render_scene ? render_scene->getSceneGraph() : nullptr;
        if (!scene_graph) {
            return;
        }

        Ref<DirectionalLight> directional_light;
        Ref<SceneGraphNode> directional_light_node;
        for (const auto& light_leaf : scene_graph->getLights()) {
            directional_light = std::dynamic_pointer_cast<DirectionalLight>(light_leaf);
            if (directional_light && directional_light->getNode()) {
                directional_light_node = directional_light->getNodePtr();
                break;
            }
        }
        if (!directional_light || !directional_light_node) {
            return;
        }

        const Vector3f light_dir = BuildDirectionalLightDirection(directional_light_node);
        const Matrix4f light_view_projection = BuildDirectionalLightViewProjection(light_dir);

        m_cmd_list->open();
        m_cmd_list->beginMarker("DirectionalLightShadowPass");
        m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
        m_cmd_list->commitBarriers();
        m_cmd_list->clearDepthStencilTexture(m_shadow_target, rhi::AllSubresources, true, 1.0f, false, 0);

        render_scene->prepareBuffers(m_cmd_list);

        DirectionalLightShadowPassConstants constants{};
        constants.light_view_projection = light_view_projection;
        m_cmd_list->writeBuffer(m_mesh_processor.getConstantBuffer(), &constants, sizeof(constants));

        m_mesh_processor.createGraphicsPipeline(m_framebuffer);

        const auto& instances = render_scene->mainCameraInstances();
        auto batches = m_mesh_processor.buildMeshBatches(instances);
        auto commands = m_mesh_processor.buildDrawCommands(batches, m_cmd_list);
        m_mesh_processor.submitDrawCommands(commands, m_framebuffer, graph().getViewportExtent(), m_cmd_list);

        m_cmd_list->setTextureState(m_shadow_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->commitBarriers();
        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void DirectionalLightShadowPass::cleanup() {
    }

    void DirectionalLightShadowPass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        createFramebuffer();
        m_mesh_processor.invalidatePipeline();
    }

    void DirectionalLightShadowPass::createFramebuffer() {
        m_shadow_target = getTextureResource(kSceneShadowMapName);
        m_framebuffer = nullptr;
        if (!m_shadow_target) {
            return;
        }

        auto framebuffer_desc = rhi::FramebufferDesc().setDepthAttachment(m_shadow_target);
        m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
    }

} // dodoe
