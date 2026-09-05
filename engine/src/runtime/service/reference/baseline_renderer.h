// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "baseline_pass.h"
#include "passes/baseline_gbuffer_pass.h"
#include "passes/baseline_lighting_pass.h"
#include "passes/baseline_sky_pass.h"
#include "passes/baseline_sprite_pass.h"
#include "passes/baseline_imgui_pass.h"
#include "passes/baseline_post_process_pass.h"
#include "passes/baseline_present_pass.h"

namespace dodoe {

    class RenderViewFamily;
    class RenderScene;

    struct BaselineRendererCreateInfo {
        GfxDeviceHandle device{};
        const ShaderLibrary* shader_library{nullptr};
        SharedRenderService* shared_render_service{nullptr};
    };

    class BaselineRenderer final : public Managed<BaselineRenderer, BaselineRendererCreateInfo> {
        friend class Managed<BaselineRenderer, BaselineRendererCreateInfo>;

        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        cutie::SamplerHandle m_sampler{};

        BaselineRenderTargets m_rt{};
        Vector2i m_scene_rt_extent{0, 0};

        Scope<BaselineGBufferPass> m_gbuffer_pass{};
        Scope<BaselineLightingPass> m_lighting_pass{};
        Scope<BaselineSkyPass> m_sky_pass{};
        Scope<BaselineSpritePass> m_sprite_pass{};
        Scope<BaselinePostProcessPass> m_post_process_pass{};
#ifdef DODOE_DEBUG_ENABLED
        Scope<BaselineImGuiPass> m_imgui_pass{};
#endif
        Scope<BaselinePresentPass> m_present_pass{};

        UInt64 m_frame_counter{0};

    public:
        void render(GfxContext& gfx, UInt32 swapchain_image_index, RenderViewFamily& view_family, RenderScene& scene);

    private:
        Bool initialize(const BaselineRendererCreateInfo& info);
        void shutdown();
        Bool ensureRenderTarget(const Vector2i& extent);
    };

} // namespace dodoe
