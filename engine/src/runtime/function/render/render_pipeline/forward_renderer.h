// do@Redlive

#pragma once

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"

namespace dodoe {

	class ForwardRenderer final : public BaseRenderer {

	    void initViews(const RenderScene& scene, RenderViewFamily& view_family) const;

	public:
	    ~ForwardRenderer() override = default;

	    Bool initialize(const RendererCreateInfo& info);
	    void shutdown() override;

	    void render(RenderViewFamily& view_family, RenderScene& scene,
	                UInt32 swapchain_image_index, DrawCommandList& out_commands,
	                FrameStagingAllocator* frame_staging_allocator,
	                RenderGraphTransientPool* transient_resource_pool) override;
	};

} // namespace dodoe

