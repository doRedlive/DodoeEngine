// 
// Created by Redlive on 2026/3/19.
//

#include "gl_render_process.h"

namespace dodoe {

    void GlRenderPass::initialize(RenderPassCreateInfo create_info) {

    }

    void GlRenderPass::shutdown() {

    }

    void GlRenderPipeline::initialize(RenderPipelineCreateInfo create_info) {
        DoAssert(create_info.shder, "Pipeline initialize failed! Shader is null!");
        shader_ = create_info.shder;
        camera_ = create_info.camera;
    }

    void GlRenderPipeline::shutdown() {

    }

    void GlRenderPipeline::attach() {
        shader_->attach();
        shader_->set_mat4("u_ViewProj", camera_->view_projection_matrix());
    }

    void GlRenderPipeline::detach() {
        shader_->detach();
    }

} // dodoe