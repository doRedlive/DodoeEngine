#include "render_object.h"

namespace dodoe {

    RenderObjectDirtyFlags RenderObject::diff(const RenderObject& previous) const {
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return RenderObjectDirtyFlags::All;
        }
        return RenderObjectDirtyFlags::None;
    }

} // dodoe
