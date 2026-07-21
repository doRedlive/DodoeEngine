// do@Redlive

#pragma once

#include "dopch.h"

#include "render_pipeline_base.h"

namespace dodoe {

    struct RenderPipelineInstance {
        RenderPipelineBase* pipeline{nullptr};
        std::function<void(RenderPipelineBase*)> destroy{};

        [[nodiscard]] Bool isValid() const { return pipeline != nullptr; }
        explicit operator Bool() const { return isValid(); }

        void reset() {
            if (pipeline && destroy) {
                destroy(pipeline);
            }
            pipeline = nullptr;
            destroy = {};
        }
    };

    using RenderPipelineFactory = std::function<RenderPipelineInstance()>;

    class RenderPipelineRegistry {
        UnorderedMap<String, RenderPipelineFactory> m_factories{};

    public:
        void registerFactory(const String& name, RenderPipelineFactory factory);

        [[nodiscard]] RenderPipelineInstance resolve(const RenderPipelineDefinition& definition) const;
    };

} // namespace dodoe
