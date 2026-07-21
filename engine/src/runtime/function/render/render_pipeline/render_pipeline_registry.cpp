// do@Redlive

#include "render_pipeline_registry.h"

namespace dodoe {

    void RenderPipelineRegistry::registerFactory(const String& name, RenderPipelineFactory factory) {
        DO_ASSERT(!name.empty(), "RenderPipelineRegistry requires a non-empty pipeline name");
        DO_ASSERT(static_cast<Bool>(factory), "RenderPipelineRegistry requires a valid factory");
        m_factories[name] = std::move(factory);
    }

    RenderPipelineInstance RenderPipelineRegistry::resolve(const RenderPipelineDefinition& definition) const {
        DO_ASSERT(!definition.pipeline_type.empty(), "RenderPipelineRegistry requires a valid pipeline type");

        const auto it = m_factories.find(definition.pipeline_type);
        if (it == m_factories.end()) {
            return {};
        }
        return it->second();
    }

} // namespace dodoe
