// do@Redlive

#include "shared_render_service.h"

namespace dodoe {

    Bool SharedRenderService::initialize(const SharedRenderServiceCreateInfo& info) {
        m_gfx_context = info.gfx_context;

        DO_ASSERT(m_gfx_context != nullptr, "SharedRenderService requires gfx_context");
        const auto device = GDrawCommandList.getDevice();
        DO_ASSERT(device != nullptr, "SharedRenderService requires valid device");

        m_descriptor_table = DescriptorTableManager::Create({m_gfx_context});
        m_texture_manager = TextureManager::Create({m_gfx_context, m_descriptor_table.get()});

        m_deletion_queue = create_scope<DeferredDeletionQueue>();
        m_shader_library = create_scope<ShaderLibrary>();
        m_shader_library->initialize(*m_gfx_context);
        m_pipeline_state_cache = create_scope<PipelineStateCache>(device);
        GlobalSamplers::initialize(device);

        m_render_target_system = create_scope<RenderTargetSystem>();
        m_render_target_system->initialize(*m_gfx_context, m_deletion_queue.get());
        m_framebuffer_cache = create_scope<FramebufferCache>();
        m_framebuffer_cache->initialize(*m_gfx_context);
        m_binding_layout_cache = BindingLayoutCache::Create({m_gfx_context});
        m_binding_set_cache = BindingSetCache::Create({m_gfx_context});
        m_input_layout_cache = InputLayoutCache::Create({m_gfx_context});
        m_material_system = create_scope<MaterialSystem>();
        m_material_system->initialize(m_shader_library.get(), m_binding_layout_cache.get(), m_binding_set_cache.get(), m_texture_manager.get());

        return m_shader_library != nullptr
            && m_pipeline_state_cache != nullptr
            && m_render_target_system != nullptr
            && m_framebuffer_cache != nullptr;
    }

    void SharedRenderService::shutdown() {
        GlobalSamplers::reset();

        if (m_input_layout_cache) {
            InputLayoutCache::Destroy(m_input_layout_cache);
        }
        if (m_binding_set_cache) {
            BindingSetCache::Destroy(m_binding_set_cache);
        }
        if (m_material_system) {
            m_material_system->shutdown();
            m_material_system.reset();
        }
        if (m_binding_layout_cache) {
            BindingLayoutCache::Destroy(m_binding_layout_cache);
        }
        if (m_framebuffer_cache) {
            m_framebuffer_cache->reset();
            m_framebuffer_cache.reset();
        }
        if (m_render_target_system) {
            m_render_target_system->shutdown();
            m_render_target_system.reset();
        }
        if (m_pipeline_state_cache) {
            m_pipeline_state_cache->clear();
            m_pipeline_state_cache.reset();
        }
        if (m_shader_library) {
            m_shader_library->reset();
            m_shader_library.reset();
        }
        if (m_deletion_queue) {
            m_deletion_queue->clear();
            m_deletion_queue.reset();
        }
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        m_gfx_context = nullptr;
    }

} // dodoe
