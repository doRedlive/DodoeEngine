// do@Redlive

#include "runtime/function/render/render_pipeline/render_feature/forward_lit_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    static GfxFramebufferInfo MakeSceneColorFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA16_FLOAT)
            .setDepthFormat(GfxFormat::D32);
        const UInt32 sample_count = RenderSettings::GetMsaaSampleCount();
        if (sample_count > 1) {
            framebuffer_info.setSampleCount(sample_count);
        }
        return framebuffer_info;
    }

    void ForwardLitSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(getMeshProcessor() != nullptr, "ForwardLitSceneFeature mesh processor is null");
        collector.addPass<OpaquePass>(getMeshProcessor());
    }

    GfxShaderHandle ForwardLitSceneFeature::getPixelShader(const ShaderLibrary& shader_library) const {
        return shader_library.getForwardLitPixelShader();
    }

    GfxFramebufferInfo ForwardLitSceneFeature::getFramebufferInfo() const {
        return MakeSceneColorFramebufferInfo();
    }

} // namespace dodoe
