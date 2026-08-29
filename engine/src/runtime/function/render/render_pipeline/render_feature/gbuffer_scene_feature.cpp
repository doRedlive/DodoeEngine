// do@Redlive

#include "gbuffer_scene_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_gbuffer_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    static RenderTargetDesc BuildGBufferDesc() {
        RenderTargetDesc desc{};
        desc.name = "GBuffer";
        desc.scale_policy = RenderTargetScalePolicy::Relative;
        desc.scale_x = 1.0f;
        desc.scale_y = 1.0f;

        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferAlbedo", GfxColor(0.08f, 0.09f, 0.11f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA16_FLOAT, "GBufferNormal", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA32_FLOAT, "GBufferPosition", GfxColor(0.0f, 0.0f, 0.0f, 1.0f)
        });
        desc.color_attachments.push_back({
            GfxFormat::RGBA8_UNORM, "GBufferMaterial", GfxColor(0.0f, 1.0f, 1.0f, 1.0f)
        });

        desc.has_depth = true;
        desc.depth_format = GfxFormat::D32;
        desc.depth_debug_name = "GBufferDepth";
        desc.clear_depth = 1.0f;

        return desc;
    }

    static GfxFramebufferInfo MakeGBufferFramebufferInfo() {
        GfxFramebufferInfo framebuffer_info{};
        framebuffer_info
            .addColorFormat(GfxFormat::RGBA8_UNORM)
            .addColorFormat(GfxFormat::RGBA16_FLOAT)
            .addColorFormat(GfxFormat::RGBA32_FLOAT)
            .addColorFormat(GfxFormat::RGBA8_UNORM)
            .setDepthFormat(GfxFormat::D32);
        return framebuffer_info;
    }

    void GBufferSceneFeature::initialize(SharedRenderService& resources) {
        LitSceneFeature::initialize(resources);

        auto* gfx = resources.getGfxContext();
        auto* deletion_queue = resources.getRenderTargetSystem()
            ? resources.getRenderTargetSystem()->getDeletionQueue()
            : nullptr;

        m_gbuffer = create_scope<RenderTargetHandle>();
        m_gbuffer->initialize(BuildGBufferDesc(), *gfx, deletion_queue);
    }

    void GBufferSceneFeature::onResize(const UInt32 width, const UInt32 height) {
        auto* gfx = getSharedRenderService() ? getSharedRenderService()->getGfxContext() : nullptr;
        DO_ASSERT(gfx != nullptr, "GBufferSceneFeature onResize requires valid GfxContext");

        if (m_gbuffer) {
            m_gbuffer->resolve(width, height, *gfx, 0);
        }
    }

    void GBufferSceneFeature::shutdown() {
        if (m_gbuffer) {
            m_gbuffer->shutdown();
            m_gbuffer.reset();
        }
        LitSceneFeature::shutdown();
    }

    void GBufferSceneFeature::registerGraphImports(RenderGraphImportRegistry& imports,
                                                   const RenderView& view) {
        (void)view;
        if (m_gbuffer) {
            imports.publish<GBufferRenderTargetKey>(m_gbuffer.get());
        }
    }

    void GBufferSceneFeature::collectPasses(PassCollector& collector) {
        DO_ASSERT(getLitProcessor() != nullptr, "GBufferSceneFeature lit processor is null");
        collector.addPass<GBufferPass>(getLitProcessor());
    }

    GfxShaderHandle GBufferSceneFeature::getPixelShader(const ShaderLibrary& shader_library) const {
        return shader_library.getGBufferPixelShader();
    }

    GfxFramebufferInfo GBufferSceneFeature::getFramebufferInfo() const {
        return MakeGBufferFramebufferInfo();
    }

} // namespace dodoe
