// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pass.h"

namespace dodoe {

	class PassCollector {
	public:
	    template <typename TPass, typename... Args>
	    TPass* addPass(Args&&... args) {
	        auto pass = create_scope<TPass>(std::forward<Args>(args)...);
	        if (m_current_feature) {
	            pass->setOwningFeature(m_current_feature);
	        }
	        TPass* ptr = pass.get();
	        m_passes.push_back(std::move(pass));
	        return ptr;
	    }

	    DynamicArray<Scope<IRenderPass>>& getPasses() { return m_passes; }
	    const DynamicArray<Scope<IRenderPass>>& getPasses() const { return m_passes; }

	    void setCurrentFeature(IRenderFeature* f) { m_current_feature = f; }

	private:
	    DynamicArray<Scope<IRenderPass>> m_passes{};
	    IRenderFeature* m_current_feature{nullptr};
	};

} // namespace dodoe
