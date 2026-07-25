// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"
#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"

namespace dodoe {

	class DeferredRenderer final : public BaseRenderer {
	    Scope<GpuCulling> m_gpu_culling{nullptr};

	    void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;
	    void executeGpuCulling(RenderViewFamily& view_family, RenderScene& scene, DrawCommandList& cmd_list) const;
	    void buildGpuDrivenDrawCommands(const RenderScene& scene, RenderViewFamily& view_family, DrawCommandList& cmd_list) const;

	public:
	    ~DeferredRenderer() override = default;

	    Bool initialize(const RendererCreateInfo& info);
	    void shutdown();

	    void render(RenderViewFamily& view_family, RenderScene& scene,
	                UInt32 swapchain_image_index, DrawCommandList& out_commands) override;
	};

} // namespace dodoe
