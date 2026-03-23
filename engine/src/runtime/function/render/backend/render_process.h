//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_RENDER_PROCESS_H
#define DODOE_RENDER_PROCESS_H

#include "dopch.h"

#include "shader.h"
#include "runtime/function/render/camera/camera.h"

namespace dodoe {

	struct RenderPassCreateInfo {

	};

	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		static Scope<RenderPass> create(RenderPassCreateInfo create_info);
		static void destroy(Scope<RenderPass>& render_pass);

	protected:
		virtual void initialize(RenderPassCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

	struct RenderPipelineCreateInfo {
		Ref<Shader> shder;
		Camera* camera{nullptr};
	};

	class RenderPipeline {
	public:
		virtual ~RenderPipeline() = default;

		static Scope<RenderPipeline> create(RenderPipelineCreateInfo create_info);
		static void destroy(Scope<RenderPipeline>& render_pipeline);

		virtual void attach() = 0;
		virtual void detach() = 0;

	protected:
		virtual void initialize(RenderPipelineCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_RENDER_PROCESS_H
