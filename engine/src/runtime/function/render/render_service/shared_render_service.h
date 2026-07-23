// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/container/deferred_deletion.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/render_service/render_target_system.h"
#include "runtime/function/render/render_service/framebuffer_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/binding_set_cache.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/material/material_system.h"
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
        Scope<DeferredDeletionQueue> m_deletion_queue{nullptr};
        Scope<ShaderLibrary> m_shader_library{nullptr};
        Scope<PipelineStateCache> m_pipeline_state_cache{nullptr};
        Scope<RenderTargetSystem> m_render_target_system{nullptr};
        Scope<FramebufferCache> m_framebuffer_cache{nullptr};
        Scope<BindingLayoutCache> m_binding_layout_cache{nullptr};
        Scope<BindingSetCache> m_binding_set_cache{nullptr};
        Scope<InputLayoutCache> m_input_layout_cache{nullptr};
        Scope<MaterialSystem> m_material_system{nullptr};

    public:
        SharedRenderService() = default;
        ~SharedRenderService() = default;

        [[nodiscard]] GfxContext* getGfxContext() const { return m_gfx_context; }
        [[nodiscard]] DescriptorTableManager* getDescriptorTable() const { return m_descriptor_table; }
        [[nodiscard]] TextureManager* getTextureManager() const { return m_texture_manager; }
        [[nodiscard]] DeferredDeletionQueue* getDeletionQueue() const { return m_deletion_queue.get(); }
        [[nodiscard]] ShaderLibrary* getShaderLibrary() const { return m_shader_library.get(); }
        [[nodiscard]] PipelineStateCache* getPipelineStateCache() const { return m_pipeline_state_cache.get(); }
        [[nodiscard]] RenderTargetSystem* getRenderTargetSystem() const { return m_render_target_system.get(); }
        [[nodiscard]] FramebufferCache* getFramebufferCache() const { return m_framebuffer_cache.get(); }
        [[nodiscard]] BindingLayoutCache* getBindingLayoutCache() const { return m_binding_layout_cache.get(); }
        [[nodiscard]] BindingSetCache* getBindingSetCache() const { return m_binding_set_cache.get(); }
        [[nodiscard]] InputLayoutCache* getInputLayoutCache() const { return m_input_layout_cache.get(); }
        [[nodiscard]] MaterialSystem* getMaterialSystem() const { return m_material_system.get(); }

    private:
        Bool initialize(const SharedRenderServiceCreateInfo& info);
        void shutdown();
    };

} // dodoe
