// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ShaderLibrary;
    class SharedRenderService;

    struct BaselinePassContext {
        GfxDeviceHandle device{};
        cutie::CommandListHandle command_list{};
        const ShaderLibrary* shader_library{nullptr};
        SharedRenderService* shared_render_service{nullptr};
        cutie::SamplerHandle sampler{};
    };

    // Deferred render targets shared across passes (UE-style GBuffer pipeline).
    struct BaselineRenderTargets {
        // GBuffer
        GfxTextureHandle gbuffer_albedo{};
        GfxTextureHandle gbuffer_normal{};
        GfxTextureHandle gbuffer_position{};
        GfxTextureHandle gbuffer_material{};
        GfxTextureHandle gbuffer_depth{};
        GfxFramebufferHandle gbuffer_framebuffer{};
        // HDR scene color (lighting output / sprite input)
        GfxTextureHandle scene_color{};
        GfxFramebufferHandle lighting_framebuffer{};
        GfxFramebufferHandle sprite_framebuffer{};
        // LDR post-process ping-pong
        GfxTextureHandle tone_map_color{};
        GfxTextureHandle fxaa_color{};
        GfxFramebufferHandle tone_map_framebuffer{};
        GfxFramebufferHandle fxaa_framebuffer{};
        GfxTextureHandle pick_id{};
        GfxFramebufferHandle pick_framebuffer{};
    };

    class BaselineRenderPass {
    public:
        virtual ~BaselineRenderPass() = default;

        virtual Bool initialize(const BaselinePassContext& context) = 0;
        virtual void shutdown() = 0;
    };

} // namespace dodoe
