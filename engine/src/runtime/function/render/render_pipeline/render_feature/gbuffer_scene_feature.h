// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"
#include "runtime/function/render/render_service/render_target_handle.h"

namespace dodoe {

    class RenderView;

    class GBufferSceneFeature final : public LitSceneFeature {
        Scope<RenderTargetHandle> m_gbuffer{nullptr};

    public:
        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;

        void registerGraphImports(RenderGraphImportRegistry& imports,
                                  const RenderView& view) override;

        void collectPasses(PassCollector& collector) override;

        [[nodiscard]] RenderTargetHandle* getGBuffer() const { return m_gbuffer.get(); }

    protected:
        [[nodiscard]] GfxShaderHandle getPixelShader(const ShaderLibrary& shader_library) const override;
        [[nodiscard]] GfxFramebufferInfo getFramebufferInfo() const override;
    };

} // namespace dodoe
