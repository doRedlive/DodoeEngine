// do@Redlive

#include "sprite_pass.h"

#include "../render_graph.h"
#include "../render_resource.h"
#include "../renderer_2d.h"
#include "../framework/descriptor_table_manager.h"
#include "../interface/rhi_context.h"

#include "runtime/core/utils/common.h"

namespace dodoe {
    namespace {
        constexpr UInt32 kVolatileConstantBufferVersions = 256;
    }

    SpritePass::SpritePass(RhiContext* rhi, DescriptorTableManager* descriptor_manager)
        : m_descriptor_table(descriptor_manager) {
        m_rhi = rhi;
    }

    void SpritePass::setup() {
        createFramebuffer();
        createBuffers();

        m_sampler = m_rhi->getDevice()->createSampler(rhi::SamplerDesc());

        MeshPipelineStateDesc desc;
        desc.vertex_shader_path = "engine/res/shaders/bin/sprite_pass.vert.spv";
        desc.pixel_shader_path  = "engine/res/shaders/bin/sprite_pass.frag.spv";
        desc.constant_buffer_size = sizeof(Matrix4f);
        desc.constant_buffer_max_versions = kVolatileConstantBufferVersions;
        desc.debug_name = "SpritePass";
        desc.descriptor_table = m_descriptor_table;
        desc.disable_caching = true;

        desc.vertex_attributes = {
            rhi::VertexAttributeDesc()
                .setName("a_Position").setFormat(rhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(QuadVertex, position))
                .setElementStride(sizeof(QuadVertex)),
            rhi::VertexAttributeDesc()
                .setName("a_UV").setFormat(rhi::Format::RG32_FLOAT)
                .setOffset(offsetof(QuadVertex, uv))
                .setElementStride(sizeof(QuadVertex)),
            rhi::VertexAttributeDesc()
                .setName("a_Color").setFormat(rhi::Format::RGBA32_FLOAT)
                .setOffset(offsetof(QuadVertex, color))
                .setElementStride(sizeof(QuadVertex)),
            rhi::VertexAttributeDesc()
                .setName("a_TexIndex").setFormat(rhi::Format::R32_UINT)
                .setOffset(offsetof(QuadVertex, texture_index))
                .setElementStride(sizeof(QuadVertex)),
        };

        desc.extra_binding_items = { rhi::BindingLayoutItem::Sampler(0) };
        desc.extra_binding_set_items = { rhi::BindingSetItem::Sampler(0, m_sampler) };

        rhi::DepthStencilState ds;
        ds.enableDepthTest().enableDepthWrite()
          .setDepthFunc(rhi::ComparisonFunc::LessOrEqual).disableStencil();

        rhi::BlendState bs;
        bs.targets[0]
            .enableBlend()
            .setSrcBlend(rhi::BlendFactor::SrcAlpha)
            .setDestBlend(rhi::BlendFactor::OneMinusSrcAlpha)
            .setBlendOp(rhi::BlendOp::Add)
            .setSrcBlendAlpha(rhi::BlendFactor::One)
            .setDestBlendAlpha(rhi::BlendFactor::OneMinusSrcAlpha)
            .setBlendOpAlpha(rhi::BlendOp::Add);

        rhi::RasterState rs;
        rs.setCullNone();

        desc.render_state
            .setDepthStencilState(ds)
            .setBlendState(bs)
            .setRasterState(rs);

        m_mesh_processor.initialize(m_rhi, desc);
        m_cmd_list = m_rhi->getDevice()->createCommandList();
    }

    void SpritePass::execute(size_t index) {
        (void)index;

        m_cmd_list->open();
        m_cmd_list->beginMarker("SpritePass");
        m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::RenderTarget);
        m_cmd_list->setTextureState(m_scene_depth_target, rhi::AllSubresources, rhi::ResourceStates::DepthWrite);
        m_cmd_list->commitBarriers();

        if (m_clear_targets) {
            m_cmd_list->clearTextureFloat(m_scene_color_target, rhi::AllSubresources, rhi::Color(0.19f, 0.19f, 0.19f, 1.0f));
            m_cmd_list->clearDepthStencilTexture(m_scene_depth_target, rhi::AllSubresources, true, 1.0f, false, 0);
        }

        const auto& batches = Renderer2D::GetQuadCpuBatches();
        if (batches.empty()) {
            m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->commitBarriers();
            m_cmd_list->endMarker();
            m_cmd_list->close();
            m_rhi->getDevice()->executeCommandList(m_cmd_list);
            return;
        }

        const auto& camera = g_RenderResource->getRenderScene().mainCamera();
        if (!camera || !camera->isValid()) {
            m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
            m_cmd_list->commitBarriers();
            m_cmd_list->endMarker();
            m_cmd_list->close();
            m_rhi->getDevice()->executeCommandList(m_cmd_list);
            return;
        }

        const Matrix4f view_projection = camera->getViewProjectionMatrix();
        m_cmd_list->writeBuffer(m_mesh_processor.getConstantBuffer(), &view_projection, sizeof(Matrix4f));

        m_mesh_processor.createGraphicsPipeline(m_framebuffer);

        const Vector2i viewport_extent(
            static_cast<Int32>(graph().getViewportExtent().x),
            static_cast<Int32>(graph().getViewportExtent().y));

        for (const auto& batch : batches) {
            if (batch.indices.empty() || batch.vertices.empty()) {
                continue;
            }

            const size_t vertex_byte_size = sizeof(QuadVertex) * batch.vertices.size();
            const size_t index_byte_size = sizeof(UInt32) * batch.indices.size();
            m_cmd_list->writeBuffer(m_vertex_buffer, batch.vertices.data(), vertex_byte_size);
            m_cmd_list->writeBuffer(m_index_buffer, batch.indices.data(), index_byte_size);

            MeshBatchElement element;
            element.index_count = static_cast<UInt32>(batch.indices.size());
            element.num_instances = 1;
            element.vertex_buffers[0] = m_vertex_buffer;
            element.index_buffer = m_index_buffer;

            MeshBatch mb;
            mb.elements.push_back(element);

            auto commands = m_mesh_processor.buildDrawCommands({ mb }, m_cmd_list);
            m_mesh_processor.submitDrawCommands(commands, m_framebuffer, viewport_extent, m_cmd_list);
        }

        m_cmd_list->setTextureState(m_scene_color_target, rhi::AllSubresources, rhi::ResourceStates::ShaderResource);
        m_cmd_list->commitBarriers();
        m_cmd_list->endMarker();
        m_cmd_list->close();
        m_rhi->getDevice()->executeCommandList(m_cmd_list);
    }

    void SpritePass::cleanup() {
    }

    void SpritePass::onViewportResize(const Vector2i& viewport_extent) {
        (void)viewport_extent;
        createFramebuffer();
        m_mesh_processor.invalidatePipeline();
    }

    void SpritePass::createFramebuffer() {
        m_scene_color_target = nullptr;
        m_scene_depth_target = nullptr;
        m_framebuffer = nullptr;

        m_scene_color_target = getTextureResource(kInputSceneColorResourceName);
        m_scene_depth_target = getTextureResource(kInputSceneDepthResourceName);

        auto framebuffer_desc = rhi::FramebufferDesc()
            .addColorAttachment(m_scene_color_target)
            .setDepthAttachment(m_scene_depth_target);
        m_framebuffer = m_rhi->getDevice()->createFramebuffer(framebuffer_desc);
    }

    void SpritePass::createBuffers() {
        auto vertex_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(QuadVertex) * Renderer2D::kMaxQuadCount * 4)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
            .setDebugName("SpritePass Vertex Buffer");
        m_vertex_buffer = m_rhi->getDevice()->createBuffer(vertex_buffer_desc);

        auto index_buffer_desc = rhi::BufferDesc()
            .setByteSize(sizeof(UInt32) * Renderer2D::kMaxQuadCount * 6)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
            .setDebugName("SpritePass Index Buffer");
        m_index_buffer = m_rhi->getDevice()->createBuffer(index_buffer_desc);
    }

} // dodoe
