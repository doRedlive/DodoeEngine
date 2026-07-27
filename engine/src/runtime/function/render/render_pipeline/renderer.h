// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pass.h"
#include "pass_collector.h"
#include "render_feature/render_feature.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/thread/thread_pool.h"

namespace dodoe {

    class FrameStagingAllocator;
    class RenderGraphTransientPool;

	struct RendererCreateInfo {
	    Size_t worker_count{0};
	    GfxContext* gfx_context{nullptr};
	    SharedRenderService* shared_render_service{nullptr};
	};

	class BaseRenderer {
	protected:
	    GfxContext*           m_gfx_context{nullptr};
	    SharedRenderService*  m_shared_render_service{nullptr};
	    Scope<ThreadPool>     m_thread_pool{nullptr};
	    DynamicArray<Scope<IRenderFeature>> m_features{};
	    DynamicArray<Scope<IRenderPass>>    m_pass_storage{};
	    DynamicArray<IRenderPass*>          m_ordered_passes{};

	    void clearViewExtensions(RenderViewFamily& view_family) const;

	    void buildOrderedPasses(RenderViewFamily& view_family, RenderScene& scene,
	                            UInt32 swapchain_image_index, DrawCommandList& out_commands,
	                            FrameStagingAllocator* frame_staging_allocator,
	                            RenderGraphTransientPool* transient_resource_pool) const;

	    static void validateBlackboard(const DynamicArray<IRenderPass*>& sorted_passes);

	    void bakePasses();

	public:
	    virtual ~BaseRenderer() = default;

	    virtual void render(RenderViewFamily& view_family, RenderScene& scene,
	                        UInt32 swapchain_image_index, DrawCommandList& out_commands,
	                        FrameStagingAllocator* frame_staging_allocator,
	                        RenderGraphTransientPool* transient_resource_pool) = 0;

	    template <typename T, typename... Args>
	    T* addFeature(Args&&... args) {
	        auto feature = create_scope<T>(std::forward<Args>(args)...);
	        T* ptr = feature.get();
	        if (m_shared_render_service) {
	            ptr->initialize(*m_shared_render_service);
	        }
	        m_features.push_back(std::move(feature));
	        return ptr;
	    }

	    template <typename T>
	    T* getFeature() const {
	        for (const auto& feature : m_features) {
	            auto* casted = dynamic_cast<T*>(feature.get());
	            if (casted) return casted;
	        }
	        return nullptr;
	    }

	    void clearFeatures() {
	        m_features.clear();
	        m_ordered_passes.clear();
	        m_pass_storage.clear();
	    }

	    void onResize(UInt32 width, UInt32 height) {
	        for (const auto& feature : m_features) {
	            feature->onResize(width, height);
	        }
	    }

	    [[nodiscard]] GfxContext*          getGfx()           const { return m_gfx_context; }
	    [[nodiscard]] SharedRenderService* getSharedService() const { return m_shared_render_service; }
	    [[nodiscard]] ThreadPool*          getThreadPool()    const { return m_thread_pool.get(); }
	};

} // dodoe
