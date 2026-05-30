// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

#include "runtime/function/render/interface/rhi.h"
#include "runtime/function/render/interface/rhi_context.h"

namespace cakery {

    class ViewportPanel : public EditorPanel {
	dodoe::Vector2f viewport_content_size_{1.0f, 1.0f};
    public:
        explicit ViewportPanel(EditorPanelDescriptor descriptor);
        ~ViewportPanel() override;

        void onWorkspaceActivated(const EditorPanelContext& context) override;
        void onWorkspaceDeactivated(const EditorPanelContext& context) override;
        void onUpdate(const EditorPanelContext& context, float delta_time) override;
        void onDraw(const EditorPanelContext& context) override;

	[[nodiscard]] const dodoe::Vector2f& viewportContentSize() const { return viewport_content_size_; }
    };

} // cakery
