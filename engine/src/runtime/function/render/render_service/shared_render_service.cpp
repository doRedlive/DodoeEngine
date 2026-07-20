// do@Redlive

#include "shared_render_service.h"

namespace dodoe {

    Bool SharedRenderService::initialize(const SharedRenderServiceCreateInfo& info) {
        m_gfx_context = info.gfx_context;
        m_descriptor_table = info.descriptor_table;
        m_texture_manager = info.texture_manager;

        DO_ASSERT(m_gfx_context != nullptr, "SharedRenderService requires gfx_context");
        const auto device = GDrawCommandList.getDevice();
        DO_ASSERT(device != nullptr, "SharedRenderService requires valid device");

        m_shader_library = create_scope<ShaderLibrary>();
        m_shader_library->initialize(*m_gfx_context);
        m_pipeline_state_cache = create_scope<PipelineStateCache>(device);
        GlobalSamplers::initialize(device);

        return m_shader_library != nullptr && m_pipeline_state_cache != nullptr;
    }

    void SharedRenderService::shutdown() {
        GlobalSamplers::reset();
        if (m_pipeline_state_cache) {
            m_pipeline_state_cache->clear();
            m_pipeline_state_cache.reset();
        }
        if (m_shader_library) {
            m_shader_library->reset();
            m_shader_library.reset();
        }
        m_texture_manager = nullptr;
        m_descriptor_table = nullptr;
        m_gfx_context = nullptr;
    }

} // dodoe
