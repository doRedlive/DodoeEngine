// do@Redlive

#include "main_camera_pass.h"

#include "../render_graph.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 4096;

        struct MainCameraPassConstants {
            Matrix4f view_projection{1.0f};
            Vector4i draw_data{0};
            Vector4f material_data{0.0f, 1.0f, 1.0f, 0.0f};
        };

        TextureManager* GetTextureManager() {
            auto& app = Application::Self();
            auto* render_system = app.context().getRenderSystem();
            return render_system ? render_system->getTextureManager() : nullptr;
        }
    }

    MainCameraPass::MainCameraPass(RhiContext* rhi, DescriptorTableManager* descriptor_manager)
        : m_descriptor_table(descriptor_manager) {
        m_rhi = rhi;
    }

    void MainCameraPass::setup() {
        createFramebuffer();

        m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());

        MeshPipelineStateDesc desc;
        desc.vertex_shader_path = "engine/res/shaders/bin/main_camera_pass.vert.spv";
        desc.pixel_shader_path  = "engine/res/shaders/bin/main_camera_pass.frag.spv";
        desc.constant_buffer_size = sizeof(MainCameraPassConstants);
        desc.constant_buffer_max_versions = kVolatileConstantBufferVersions;
        desc.debug_name = "MainCameraPass";
        desc.extra_binding_items = { rhi::BindingLayoutItem::Sampler(0) };
        desc.extra_binding_set_items = { rhi::BindingSetItem::Sampler(0, m_sampler) };
        desc.descriptor_table = m_descriptor_table;

        rhi::DepthStencilState ds;
        ds.enableDepthTest().enableDepthWrite()
          .setDepthFunc(rhi::ComparisonFunc::Less).disableStencil();
        desc.render_state.setDepthStencilState(ds);

        m_mesh_processor.initialize(m_rhi, desc);

        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void MainCameraPass::cleanup() {
    }

    void MainCameraPass::execute(size_t index) {
        (void)index;

        auto& scene = g_RenderResource->getRenderScene();
        const auto& camera = scene.mainCamera();
        const auto& instances = scene.mainCameraInstances();

        m_cmd_list->open();
        m_cmd_list->beginMarker("MainCameraPass");
        m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
        m_cmd_list->commitBarriers();
        m_cmd_list->clearTextureFloat(m_albedo_target, rhi::AllSubresources, rhi::Color(0.08f, 0.09f, 0.11f, 1.0f));
        m_cmd_list->clearTextureFloat(m_normal_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
        m_cmd_list->clearTextureFloat(m_position_target, rhi::AllSubresources, rhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
        m_cmd_list->clearTextureFloat(m_material_target, rhi::AllSubresources, rhi::Color(0.0f, 1.0f, 1.0f, 1.0f));
        m_cmd_list->clearDepthStencilTexture(m_depth_target, rhi::AllSubresources, true, 1.0f, false, 0);

        if (!camera || !camera->isValid() || instances.empty()) {
            m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->commitBarriers();
            m_cmd_list->endMarker();
            m_cmd_list->close();
            m_rhi->getDevice()->executeCommandList(m_cmd_list);
            return;
        }

        scene.prepareBuffers(m_cmd_list);

        m_cached_view_projection = camera->getViewProjectionMatrix();

        m_mesh_processor.createGraphicsPipeline(m_framebuffer);

        auto batches = m_mesh_processor.buildMeshBatches(instances);
        auto commands = m_mesh_processor.buildDrawCommands(batches, m_cmd_list,
            [this](rhi::CommandListHandle cmd_list, const MeshBatch& batch, rhi::BufferHandle cb) {
                MainCameraPassConstants constants{};
                constants.view_projection = m_cached_view_projection;

                if (batch.material) {
                    constants.draw_data.x = static_cast<Int32>(resolveTextureIndex(batch.material));
                    constants.draw_data.y = static_cast<Int32>(resolveMetallicRoughnessTextureIndex(batch.material));
                    constants.draw_data.z = batch.material->metallic_roughness_texture.isValid() ? 1 : 0;
                    constants.material_data.x = glm::clamp(batch.material->metallic, 0.0f, 1.0f);
                    constants.material_data.y = glm::clamp(batch.material->roughness, 0.04f, 1.0f);
                }

                cmd_list->writeBuffer(cb, &constants, sizeof(constants));
            });
        m_mesh_processor.submitDrawCommands(commands, m_framebuffer, graph().getViewportExtent(), m_cmd_list);

        m_cmd_list->setTextureState(m_albedo_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(m_normal_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(m_position_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->setTextureState(m_material_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->commitBarriers();

        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void MainCameraPass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        createFramebuffer();
        m_mesh_processor.invalidatePipeline();
    }

    void MainCameraPass::createFramebuffer() {
        m_albedo_target = nullptr;
        m_normal_target = nullptr;
        m_position_target = nullptr;
        m_material_target = nullptr;
        m_depth_target = nullptr;
        m_framebuffer = nullptr;

        m_albedo_target = getTextureResource(kSceneAlbedoName);
        m_normal_target = getTextureResource(kSceneNormalName);
        m_position_target = getTextureResource(kScenePositionName);
        m_material_target = getTextureResource(kSceneMaterialName);
        m_depth_target = getTextureResource(kSceneDepthName);
        if (!m_albedo_target || !m_normal_target || !m_position_target || !m_material_target || !m_depth_target) {
            return;
        }

        auto framebuffer_desc = rhi::FramebufferDesc()
            .addColorAttachment(m_albedo_target)
            .addColorAttachment(m_normal_target)
            .addColorAttachment(m_position_target)
            .addColorAttachment(m_material_target)
            .setDepthAttachment(m_depth_target);
        m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
    }

    UInt32 MainCameraPass::resolveTextureIndex(const Ref<Material>& material) const {
        auto* texture_manager = GetTextureManager();

        auto fallback_texture = texture_manager->getFallback();
        UInt32 texture_index = 0;
        if (fallback_texture && fallback_texture->getDescriptorIndex() >= 0) {
            texture_index = static_cast<UInt32>(fallback_texture->getDescriptorIndex());
        }

        if (material && material->base_color_texture.isValid()) {
            auto texture = texture_manager->findTexture(static_cast<InstanceID>(material->base_color_texture.getID()));
            if (texture && texture->getDescriptorIndex() >= 0) {
                texture_index = static_cast<UInt32>(texture->getDescriptorIndex());
            }
        }

        return texture_index;
    }

    UInt32 MainCameraPass::resolveMetallicRoughnessTextureIndex(const Ref<Material>& material) const {
        auto* texture_manager = GetTextureManager();
        if (!texture_manager || !material || !material->metallic_roughness_texture.isValid()) {
            return 0;
        }

        auto texture = texture_manager->findTexture(static_cast<InstanceID>(material->metallic_roughness_texture.getID()));
        if (texture && texture->getDescriptorIndex() >= 0) {
            return static_cast<UInt32>(texture->getDescriptorIndex());
        }
        return 0;
    }

} // dodoe
