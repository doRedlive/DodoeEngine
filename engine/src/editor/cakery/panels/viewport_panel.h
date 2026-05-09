// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/function/render/interface/rhi.h"
#include "runtime/function/render/interface/rhi_context.h"

namespace cakery {
	class ViewportPanel {
		dodoe::Vector2f viewport_content_size_{1.0f, 1.0f};
	public:
        ~ViewportPanel();
        ViewportPanel() = default;
		void initialize();
        [[nodiscard]] const dodoe::Vector2f& viewportContentSize() const { return viewport_content_size_; }
		void update();
		void draw();
        void cleanup();
	};

} // cakery
