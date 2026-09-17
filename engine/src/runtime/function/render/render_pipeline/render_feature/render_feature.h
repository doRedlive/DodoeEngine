// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"
#include "runtime/function/render/render_pipeline/pass_collector.h"
#include "runtime/function/render/mesh_draw/mesh_pass_type.h"

namespace dodoe {

	class SharedRenderService;
	class RenderGraphPassContext;
	class DrawCommandList;

	class IMeshPhaseProvider {
	public:
	    virtual ~IMeshPhaseProvider() = default;
	    [[nodiscard]] virtual MeshPassType getProvidedMeshPass() const = 0;
	    virtual void drawPhase(RenderGraphPassContext& context, DrawCommandList& command_list) = 0;
	};

	class IRenderFeature {
	public:
	    virtual ~IRenderFeature() = default;

	    virtual void initialize(SharedRenderService& resources) {}
	    virtual void onResize(UInt32 width, UInt32 height) {}
	    virtual void shutdown() {}

	    virtual void registerGraphImports(RenderGraphImportRegistry& imports,
	                                      const RenderView& view) {}

	    virtual void collectPasses(PassCollector& collector) = 0;

	    [[nodiscard]] virtual IMeshPhaseProvider* asMeshPhaseProvider() { return nullptr; }
	};

} // namespace dodoe
