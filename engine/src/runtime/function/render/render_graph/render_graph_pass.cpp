// do@Redlive

#include "render_graph_pass.h"

#include "runtime/function/render/render_service/shared_render_service.h"

namespace dodoe {

    const ShaderLibrary* RenderGraphPassContext::getShaderLibrary() const {
        const auto* service = getSharedRenderService();
        return service ? service->getShaderLibrary() : nullptr;
    }

    PipelineStateCache* RenderGraphPassContext::getPipelineStateCache() const {
        const auto* service = getSharedRenderService();
        return service ? service->getPipelineStateCache() : nullptr;
    }

    TextureManager* RenderGraphPassContext::getTextureManager() const {
        const auto* service = getSharedRenderService();
        return service ? service->getTextureManager() : nullptr;
    }

} // namespace dodoe
