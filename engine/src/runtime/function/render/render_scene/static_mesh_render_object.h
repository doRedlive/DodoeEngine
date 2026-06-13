#pragma once

#include "render_object.h"

namespace dodoe {

    class StaticMeshRenderObject final : public RenderObject {
    public:
        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::StaticMesh; }
    };

} // dodoe
