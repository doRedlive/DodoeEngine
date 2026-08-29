// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_test_pass.h"

namespace dodoe {

	class TestFeature final : public IRenderFeature {
	public:
	    ~TestFeature() override = default;

	    void collectPasses(PassCollector& collector) override;
	};

} // namespace dodoe
