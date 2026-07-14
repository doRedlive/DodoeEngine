// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/framework/shared_render_service.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"

namespace dodoe {

    class GfxContext;
    class ShaderLibrary;
    class PipelineStateCache;
    class TextureManager;
    class RenderScene;
    class GBufferMeshProcessor;
    class DirectionalShadowMeshProcessor;
    class LocalVertexFactory;

    enum class PassType : UInt32 {
        Sprite,
        GBuffer,
        Shadow,
        Transparent,
        CullingInput,
        Count
    };

    struct RenderPassContext {
        GfxContext* gfx_context{nullptr};
        SharedRenderService* shared_render_service{nullptr};
        const RenderScene* scene{nullptr};
        LocalVertexFactory* local_vertex_factory{nullptr};
        const IMeshPassProcessor* mesh_processors[static_cast<size_t>(MeshPassType::Count)]{};

        [[nodiscard]] GfxContext* getGfxContext() const { return gfx_context; }
        [[nodiscard]] SharedRenderService* getSharedRenderService() const { return shared_render_service; }
        [[nodiscard]] const ShaderLibrary* getShaderLibrary() const { return shared_render_service ? shared_render_service->getShaderLibrary() : nullptr; }
        [[nodiscard]] PipelineStateCache* getPipelineStateCache() const { return shared_render_service ? shared_render_service->getPipelineStateCache() : nullptr; }
        [[nodiscard]] TextureManager* getTextureManager() const { return shared_render_service ? shared_render_service->getTextureManager() : nullptr; }
        [[nodiscard]] const RenderScene* getScene() const { return scene; }

        [[nodiscard]] Bool isValid() const {
            return gfx_context != nullptr &&
                shared_render_service != nullptr &&
                getShaderLibrary() != nullptr &&
                getPipelineStateCache() != nullptr &&
                scene != nullptr;
        }

        template <MeshPassType Type>
        [[nodiscard]] const auto& getMeshProcessor() const {
            return *static_cast<const typename std::conditional_t<
                Type == MeshPassType::GBuffer,
                GBufferMeshProcessor,
                DirectionalShadowMeshProcessor>*>(mesh_processors[static_cast<size_t>(Type)]);
        }
    };

} // dodoe
