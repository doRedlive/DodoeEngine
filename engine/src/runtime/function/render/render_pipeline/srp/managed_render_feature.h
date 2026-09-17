// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_feature/render_feature.h"
#include "runtime/function/render/render_pipeline/render_phase.h"

namespace dodoe {

    class ManagedRenderFeature : public IRenderFeature {
        Int32 m_managed_id{0};
        RenderPhase m_phase{RenderPhase::PostProcess};

    public:
        ManagedRenderFeature(const Int32 managed_id, const RenderPhase phase)
            : m_managed_id(managed_id), m_phase(phase) {}

        void initialize(SharedRenderService& resources) override;
        void onResize(UInt32 width, UInt32 height) override;
        void shutdown() override;
        void collectPasses(PassCollector& collector) override;
    };

} // dodoe
