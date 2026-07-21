// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pipeline_definition.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/shared_render_service.h"

namespace dodoe {

    struct RendererCreateInfo;

    class RenderPipelineBase {
    public:
        virtual ~RenderPipelineBase() = default;

        virtual Bool initialize(const RenderPipelineDefinition& definition,
                                const RendererCreateInfo& info) = 0;

        virtual void onResize(UInt32 width, UInt32 height) = 0;

        virtual void render(RenderViewFamily& view_family,
                            RenderScene& scene,
                            UInt32 swapchain_image_index,
                            DrawCommandList& out_commands) = 0;

        virtual void shutdown() = 0;
    };

} // namespace dodoe
