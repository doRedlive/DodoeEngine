#pragma once

#include "primitive_render_object.h"

namespace dodoe {

    class StaticMeshRenderObject final : public PrimitiveRenderObject {
    public:
        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::StaticMesh; }
    };

} // dodoe
