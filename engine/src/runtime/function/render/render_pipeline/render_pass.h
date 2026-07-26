// do@Redlive

#pragma once

#include "dopch.h"

#include "render_phase.h"

namespace dodoe {

	class RenderGraphBuilder;
	class RenderView;
	class RenderGraphImportRegistry;
	class GfxContext;
	class SharedRenderService;
	class RenderScene;

	template <typename... Ts>
	struct TypeList {};

	namespace detail {
		template <typename T, typename = void>
		struct HasProduces : std::false_type {};

		template <typename T>
		struct HasProduces<T, std::void_t<typename T::Produces>> : std::true_type {};

		template <typename T, typename = void>
		struct HasConsumes : std::false_type {};

		template <typename T>
		struct HasConsumes<T, std::void_t<typename T::Consumes>> : std::true_type {};
	}

	template <typename... Ts>
	DynamicArray<Size_t> MakeKeyHashes(TypeList<Ts...>) {
	    return { typeid(Ts).hash_code()... };
	}

	using NoProduces = TypeList<>;
	using NoConsumes = TypeList<>;

	struct RenderPassBuildContext {
	    const RenderView&        view;
	    const RenderGraphImportRegistry* graph_imports{nullptr};
	    GfxContext*              gfx_context{nullptr};
	    SharedRenderService*     shared_render_service{nullptr};
	    const RenderScene*       scene{nullptr};
	};

	class IRenderFeature;

	class IRenderPass {
	public:
	    virtual ~IRenderPass() = default;

	    void setOwningFeature(IRenderFeature* feature) { m_owning_feature = feature; }

	protected:
	    IRenderFeature* m_owning_feature{nullptr};

	    virtual RenderPhase getPhase() const = 0;

	    virtual DynamicArray<Size_t> getProducedKeys() const { return {}; }

	    virtual DynamicArray<Size_t> getConsumedKeys() const { return {}; }

	    virtual void build(RenderGraphBuilder& graph,
	                       const RenderPassBuildContext& context) = 0;
	};

} // namespace dodoe
