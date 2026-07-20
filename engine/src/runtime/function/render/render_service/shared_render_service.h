// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    struct SharedRenderServiceCreateInfo {
        GfxContext* gfx_context{nullptr};
        DescriptorTableManager* descriptor_table{nullptr};
        TextureManager* texture_manager{nullptr};
    };

    class SharedRenderService : public Managed<SharedRenderService, SharedRenderServiceCreateInfo> {
        friend class Managed<SharedRenderService, SharedRenderServiceCreateInfo>;

        GfxContext* m_gfx_context{nullptr};
        DescriptorTableManager* m_descriptor_table{nullptr};
        TextureManager* m_texture_manager{nullptr};
        Scope<ShaderLibrary> m_shader_library{nullptr};
        Scope<PipelineStateCache> m_pipeline_state_cache{nullptr};

    public:
        SharedRenderService() = default;
        ~SharedRenderService() = default;

        [[nodiscard]] GfxContext* getGfxContext() const { return m_gfx_context; }
        [[nodiscard]] DescriptorTableManager* getDescriptorTable() const { return m_descriptor_table; }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager; }
        [[nodiscard]] ShaderLibrary* getShaderLibrary() const { return m_shader_library.get(); }
        [[nodiscard]] PipelineStateCache* getPipelineStateCache() const { return m_pipeline_state_cache.get(); }

    private:
        Bool initialize(const SharedRenderServiceCreateInfo& info);
        void shutdown();
    };

} // dodoe
