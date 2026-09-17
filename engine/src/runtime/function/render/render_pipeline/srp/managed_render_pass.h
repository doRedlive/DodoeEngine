// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/render_phase.h"

namespace dodoe {

    class ManagedRenderPass : public IRenderPass {
        Int32 m_feature_id{0};
        RenderPhase m_phase{RenderPhase::PostProcess};

    public:
        ManagedRenderPass(const Int32 feature_id, const RenderPhase phase)
            : m_feature_id(feature_id), m_phase(phase) {}

        [[nodiscard]] RenderPhase getPhase() const override { return m_phase; }
        void build(RenderGraphBuilder& graph, const RenderPassBuildContext& context) override;
    };

} // dodoe
