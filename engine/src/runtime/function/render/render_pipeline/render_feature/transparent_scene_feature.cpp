// do@Redlive

#include "runtime/function/render/render_pipeline/render_feature/transparent_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_transparent_pass.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    static GfxFramebufferInfo MakeTransparentFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA16_FLOAT)
            .setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    void TransparentSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(getLitProcessor() != nullptr, "TransparentSceneFeature lit processor is null");
        collector.addPass<TransparentPass>(getLitProcessor());
    }

    GfxShaderHandle TransparentSceneFeature::getPixelShader(const ShaderLibrary& shader_library) const {
        return shader_library.getForwardLitPixelShader();
    }

    GfxFramebufferInfo TransparentSceneFeature::getFramebufferInfo() const {
        return MakeTransparentFramebufferInfo();
    }

    void TransparentSceneFeature::modifyPipelineDesc(GfxGraphicsPipelineDesc& pipeline_desc) const {
        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.enableDepthTest().disableDepthWrite().setDepthFunc(GfxComparisonFunc::Less).disableStencil();
        GfxBlendState blend_state;
        GfxBlendState::RenderTarget blend_target;
        blend_target.enableBlend()
            .setSrcBlend(GfxBlendFactor::SrcAlpha)
            .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
            .setSrcBlendAlpha(GfxBlendFactor::One)
            .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
        blend_state.setRenderTarget(0, blend_target);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state).setBlendState(blend_state);
        pipeline_desc.setRenderState(render_state);
    }

} // namespace dodoe
