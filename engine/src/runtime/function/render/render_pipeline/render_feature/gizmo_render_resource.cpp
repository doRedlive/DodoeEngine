// do@Redlive

#include "gizmo_render_resource.h"

#ifdef DODOE_EDITOR_ENABLED

namespace dodoe {

    void GizmoRenderResource::reset() {
        m_binding_layout = nullptr;
    }

    GfxBindingLayoutHandle GizmoRenderResource::getOrCreateBindingLayout(DrawCommandList& command_list) {
        if (m_binding_layout) return m_binding_layout;

        m_binding_layout = command_list.createBindingLayout(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Vertex)
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));
        return m_binding_layout;
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
