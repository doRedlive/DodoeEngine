//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_GL_RENDER_PROCESS_H
#define DODOE_GL_RENDER_PROCESS_H

#include "runtime/function/render/backend/render_process.h"
#include "runtime/function/render/backend/shader.h"

#include "runtime/function/render/camera/camera.h"

namespace dodoe {


    class GlRenderPass : public RenderPass {
    public:

    protected:
        void initialize(RenderPassCreateInfo create_info) override;
        void shutdown() override;
    };

    class GlRenderPipeline : public RenderPipeline {
    public:
        void attach() override;
        void detach() override;

    protected:
        void initialize(RenderPipelineCreateInfo create_info) override;
        void shutdown() override;

    private:
        Ref<Shader> shader_{nullptr};
        Camera* camera_{nullptr};
    };

} // dodoe

#endif//DODOE_GL_RENDER_PROCESS_H