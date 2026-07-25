// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/resource_registry.h"
#include "runtime/function/render/render_pipeline/pass_collector.h"

namespace dodoe {

	class SharedRenderService;

	class IRenderFeature {
	public:
	    virtual ~IRenderFeature() = default;

	    virtual void initialize(SharedRenderService& resources) {}
	    virtual void onResize(UInt32 width, UInt32 height) {}
	    virtual void shutdown() {}

	    virtual void exportResources(ResourceRegistry& registry,
	                                 const RenderView& view) {}

	    virtual void collectPasses(PassCollector& collector) = 0;
	};

} // namespace dodoe
