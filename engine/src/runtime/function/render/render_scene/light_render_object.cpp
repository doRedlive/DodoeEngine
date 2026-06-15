// do@Redlive

#include "light_render_object.h"

namespace dodoe {

    RenderObjectDirtyFlags LightRenderObject::diff(const RenderObject& previous) const {
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return RenderObjectDirtyFlags::All;
        }
        const auto& prev = static_cast<const LightRenderObject&>(previous);
        if (m_color.r != prev.m_color.r || m_color.g != prev.m_color.g ||
            m_color.b != prev.m_color.b || m_color.a != prev.m_color.a ||
            m_intensity != prev.m_intensity) {
            return RenderObjectDirtyFlags::State;
        }
        return RenderObjectDirtyFlags::None;
    }

    RenderObjectDirtyFlags PointLightRenderObject::diff(const RenderObject& previous) const {
        const RenderObjectDirtyFlags base = LightRenderObject::diff(previous);
        if (base == RenderObjectDirtyFlags::All) return base;
        const auto& prev = static_cast<const PointLightRenderObject&>(previous);
        if (m_radius != prev.m_radius || m_range != prev.m_range) {
            return base | RenderObjectDirtyFlags::State;
        }
        return base;
    }

    RenderObjectDirtyFlags SpotLightRenderObject::diff(const RenderObject& previous) const {
        const RenderObjectDirtyFlags base = LightRenderObject::diff(previous);
        if (base == RenderObjectDirtyFlags::All) return base;
        const auto& prev = static_cast<const SpotLightRenderObject&>(previous);
        if (m_radius != prev.m_radius || m_range != prev.m_range ||
            m_inner_angle != prev.m_inner_angle || m_outer_angle != prev.m_outer_angle) {
            return base | RenderObjectDirtyFlags::State;
        }
        return base;
    }

    RenderObjectDirtyFlags SkyLightRenderObject::diff(const RenderObject& previous) const {
        const RenderObjectDirtyFlags base = LightRenderObject::diff(previous);
        if (base == RenderObjectDirtyFlags::All) return base;
        const auto& prev = static_cast<const SkyLightRenderObject&>(previous);
        if (m_cubemap != prev.m_cubemap) {
            return base | RenderObjectDirtyFlags::State;
        }
        return base;
    }

} // dodoe
