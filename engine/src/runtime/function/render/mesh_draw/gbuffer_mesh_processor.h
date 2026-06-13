// do@Redlive

#pragma once

#include "dopch.h"

#include "../framework/descriptor_table_manager.h"
#include "../framework/texture_manager.h"
#include "../framework/primitive_scene_info.h"
#include "../rendering_pipeline/render_view.h"
#include "view_mesh_draw_context.h"

namespace dodoe {

    class GBufferMeshProcessor {
        DescriptorTableManager* m_descriptor_table{nullptr};
        TextureManager* m_texture_manager{nullptr};
        GfxSamplerHandle m_sampler{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_binding_set{};
        GfxBufferHandle m_constant_buffer{};

    public:
        GBufferMeshProcessor() = default;
        GBufferMeshProcessor(DescriptorTableManager* descriptor_table, TextureManager* texture_manager)
            : m_descriptor_table(descriptor_table), m_texture_manager(texture_manager) { }

        void initialize(GfxContext& gfx_context, DescriptorTableManager* descriptor_table, TextureManager* texture_manager);
        void reset();

        void setDescriptorTable(DescriptorTableManager* descriptor_table) { m_descriptor_table = descriptor_table; }
        void setTextureManager(TextureManager* texture_manager) { m_texture_manager = texture_manager; }
        [[nodiscard]] const GfxBindingLayoutHandle& getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getConstantBuffer() const { return m_constant_buffer; }

        void buildCommands(
            const ViewMeshDrawContext& view_context,
            const RenderView& view,
            ViewMeshDrawContext& out_view_context) const;
    };

} // dodoe
