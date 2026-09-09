// do@Redlive

#include "runtime/function/render/render_pipeline/render_feature/transparent_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_transparent_pass.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    static GfxFramebufferInfo MakeTransparentFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA16_FLOAT);
        const UInt32 sample_count = RenderSettings::GetMsaaSampleCount();
        if (sample_count > 1) {
            framebuffer_info.setSampleCount(sample_count);
        }
        return framebuffer_info;
    }

    void TransparentSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(getMeshProcessor() != nullptr, "TransparentSceneFeature mesh processor is null");
        collector.addPass<TransparentPass>(getMeshProcessor());
    }

    GfxShaderHandle TransparentSceneFeature::getPixelShader(const ShaderLibrary& shader_library) const {
        return shader_library.getForwardLitPixelShader();
    }

    GfxFramebufferInfo TransparentSceneFeature::getFramebufferInfo() const {
        return MakeTransparentFramebufferInfo();
    }

} // namespace dodoe
