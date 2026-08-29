// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"

namespace dodoe {

    class ForwardLitSceneFeature final : public LitSceneFeature {
    public:
        void collectPasses(PassCollector& collector) override;

    protected:
        [[nodiscard]] GfxShaderHandle getPixelShader(const ShaderLibrary& shader_library) const override;
        [[nodiscard]] GfxFramebufferInfo getFramebufferInfo() const override;
        [[nodiscard]] bool usesPassBindingLayout() const override { return true; }
    };

} // namespace dodoe
