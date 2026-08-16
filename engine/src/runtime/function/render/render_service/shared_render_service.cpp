// do@Redlive

#include "shared_render_service.h"

namespace dodoe {

    Bool SharedRenderService::initialize(const SharedRenderServiceCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("SharedRenderService::initialize", "startup");
        m_gfx_context = info.gfx_context;

        DO_ASSERT(m_gfx_context != nullptr, "SharedRenderService requires gfx_context");
        const auto device = GDrawCommandList.getDevice();
        DO_ASSERT(device != nullptr, "SharedRenderService requires valid device");

        m_descriptor_table = DescriptorTableManager::Create({m_gfx_context});
        m_texture_manager = TextureManager::Create({m_gfx_context, m_descriptor_table.get()});

        m_deletion_queue = create_scope<DeferredDeletionQueue>();
        m_shader_library = ShaderLibrary::Create({m_gfx_context});
        m_pipeline_state_cache = PipelineStateCache::Create({device});
        GlobalSamplers::initialize(device);

        m_render_target_system = RenderTargetSystem::Create({m_gfx_context, m_deletion_queue.get()});
        m_framebuffer_cache = FramebufferCache::Create({m_gfx_context});
        m_binding_layout_cache = BindingLayoutCache::Create({m_gfx_context});
        m_binding_set_cache = BindingSetCache::Create({m_gfx_context});
        m_input_layout_cache = InputLayoutCache::Create({m_gfx_context});
        m_material_system = MaterialSystem::Create({m_shader_library.get(), m_binding_layout_cache.get(), m_binding_set_cache.get(), m_texture_manager.get()});

        const Bool initialized = m_descriptor_table != nullptr
            && m_texture_manager != nullptr
            && m_deletion_queue != nullptr
            && m_shader_library != nullptr
            && m_pipeline_state_cache != nullptr
            && m_render_target_system != nullptr
            && m_framebuffer_cache != nullptr
            && m_binding_layout_cache != nullptr
            && m_binding_set_cache != nullptr
            && m_input_layout_cache != nullptr
            && m_material_system != nullptr;
        if (!initialized) {
            shutdown();
        }
        return initialized;
    }

    void SharedRenderService::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("SharedRenderService::shutdown", "shutdown");
        GlobalSamplers::reset();

        if (m_input_layout_cache) {
            InputLayoutCache::Destroy(m_input_layout_cache);
        }
        if (m_binding_set_cache) {
            BindingSetCache::Destroy(m_binding_set_cache);
        }
        if (m_material_system) {
            MaterialSystem::Destroy(m_material_system);
        }
        if (m_binding_layout_cache) {
            BindingLayoutCache::Destroy(m_binding_layout_cache);
        }
        if (m_framebuffer_cache) {
            FramebufferCache::Destroy(m_framebuffer_cache);
        }
        if (m_render_target_system) {
            RenderTargetSystem::Destroy(m_render_target_system);
        }
        if (m_pipeline_state_cache) {
            PipelineStateCache::Destroy(m_pipeline_state_cache);
        }
        if (m_shader_library) {
            ShaderLibrary::Destroy(m_shader_library);
        }
        if (m_deletion_queue) {
            m_deletion_queue->clear();
            m_deletion_queue.reset();
        }
        TextureManager::Destroy(m_texture_manager);
        DescriptorTableManager::Destroy(m_descriptor_table);
        m_gfx_context = nullptr;
    }

    GfxTextureHandle SharedRenderService::resolveTextureBySlot(const UInt32 slot) const {
        if (!m_texture_manager) return {};
        auto* tex = m_texture_manager->resolveSlot(slot);
        return tex ? tex->getGpuHandle() : GfxTextureHandle{};
    }

} // dodoe
