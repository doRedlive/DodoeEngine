// do@Redlive

#include "renderer.h"

#include "render_graph_import_registry.h"

#include "../render_graph/render_graph_builder.h"
#include "runtime/function/render/render_view/render_view.h"

namespace dodoe {

	void BaseRenderer::clearViewExtensions(RenderViewFamily& view_family) const {
	    for (auto& view : view_family.getViews()) {
	        view.resetExtensions();
	    }
	}

	void BaseRenderer::validateBlackboard(const DynamicArray<IRenderPass*>& sorted_passes) {
	    UnorderedMap<Size_t, Size_t> producers{};

	    for (Size_t i = 0; i < sorted_passes.size(); i++) {
	        const auto* pass = sorted_passes[i];
	        if (!pass) continue;

	        for (const auto& key_hash : pass->getProducedKeys()) {
	            DO_ASSERT(producers.find(key_hash) == producers.end(),
	                      "Blackboard key produced by multiple passes");
	            producers[key_hash] = i;
	        }
	    }

	    for (Size_t i = 0; i < sorted_passes.size(); i++) {
	        const auto* pass = sorted_passes[i];
	        if (!pass) continue;

	        for (const auto& key_hash : pass->getConsumedKeys()) {
	            DO_ASSERT(producers.find(key_hash) != producers.end(),
	                      "Blackboard key consumed but never produced");
	            DO_ASSERT(producers[key_hash] <= i,
	                      "Blackboard key consumed before it is produced");
	        }
	    }
	}

	void BaseRenderer::bakePasses() {
	    PassCollector collector;

	    for (const auto& feature : m_features) {
	        collector.setCurrentFeature(feature.get());
	        feature->collectPasses(collector);
	    }

	    m_pass_storage.clear();
	    m_ordered_passes.clear();
	    m_pass_storage.reserve(collector.getPasses().size());
	    m_ordered_passes.reserve(collector.getPasses().size());

	    for (auto& pass : collector.getPasses()) {
	        m_ordered_passes.push_back(pass.get());
	        m_pass_storage.push_back(std::move(pass));
	    }

	    std::sort(m_ordered_passes.begin(), m_ordered_passes.end(),
	        [](const IRenderPass* a, const IRenderPass* b) {
	            return static_cast<UInt8>(a->getPhase())
	                 < static_cast<UInt8>(b->getPhase());
	        });

	    validateBlackboard(m_ordered_passes);
	}

	void BaseRenderer::buildOrderedPasses(RenderViewFamily& view_family,
	                                      RenderScene& scene,
	                                      const UInt32 swapchain_image_index,
	                                      DrawCommandList& out_commands,
	                                      FrameStagingAllocator* frame_staging_allocator,
	                                      RenderGraphTransientPool* transient_resource_pool) const {
	    DO_ASSERT(transient_resource_pool != nullptr,
	              "BaseRenderer requires a transient resource pool");
	    DynamicArray<RenderGraphBuilder> graphs;
	    graphs.reserve(view_family.getSize());

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        const auto& view = view_family.getView(view_index);

	        RenderGraphImportRegistry graph_imports;
	        for (const auto& feature : m_features) {
	            feature->registerGraphImports(graph_imports, view);
	        }
	        graph_imports.freeze();

	        RenderGraphBuilder graph{};
	        const RenderPassBuildContext build_ctx{
	            view, &graph_imports, m_gfx_context, m_shared_render_service, &scene
	        };
	        for (auto* pass : m_ordered_passes) {
	            pass->build(graph, build_ctx);
	        }
	        graph.compile();
	        graphs.push_back(std::move(graph));
	    }

	    for (Size_t view_index = 0; view_index < view_family.getSize(); view_index++) {
	        RenderGraphExecuteContext context{};
	        context.view_family           = &view_family;
	        context.scene                 = &scene;
	        context.view                  = &view_family.getView(view_index);
	        context.view_index            = view_index;
	        context.gfx_context           = m_gfx_context;
	        context.shared_render_service = m_shared_render_service;
	        context.frame_staging_allocator = frame_staging_allocator;
	        context.transient_resource_pool = transient_resource_pool;
	        context.swapchain_image_index  = swapchain_image_index;
	        graphs[view_index].execute(*m_thread_pool, context, out_commands);
	    }
	}

} // dodoe
