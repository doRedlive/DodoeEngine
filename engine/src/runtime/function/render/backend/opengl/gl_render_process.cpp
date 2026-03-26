// 
// Created by Redlive on 2026/3/19.
//

#include "gl_render_process.h"

#include "runtime/core/application.h"
#include "runtime/core/system_context.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    void GlRenderPass::initialize(RenderPassCreateInfo create_info) {

    }

    void GlRenderPass::shutdown() {

    }

    void GlRenderPipeline::initialize(RenderPipelineCreateInfo create_info) {
        DoAssert(create_info.shder, "Pipeline initialize failed! Shader is null!");
        shader_ = create_info.shder;
    }

    void GlRenderPipeline::shutdown() {

    }

    void GlRenderPipeline::attach() {
        auto* render_system = Application::self().context().render_system.get();
        DoAssert(render_system, "GlRenderPipeline::attach: render_system is null.");
        auto& camera = render_system->camera();

        shader_->attach();
        shader_->set_mat4("u_ViewProj", camera.view_projection_matrix());
        for (int i = 0; i < 16; ++i) {
            shader_->set_int("u_Textures[" + std::to_string(i) + "]", i);
        }
    }

    void GlRenderPipeline::detach() {
        shader_->detach();
    }

} // dodoe
