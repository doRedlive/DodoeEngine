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
    }

    void GlRenderPipeline::shutdown() {

    }

    void GlRenderPipeline::attach() {
        shader_->attach();
    }

    void GlRenderPipeline::detach() {
        shader_->detach();
    }

} // dodoe