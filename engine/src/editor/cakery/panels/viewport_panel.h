// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

#include "runtime/function/render/interface/rhi.h"
#include "runtime/function/render/interface/rhi_context.h"
#include "runtime/function/render/framework/camera.h"

namespace cakery {

	class ViewportPanel : public EditorPanel {
	private:
		dodoe::Vector2f m_viewport_content_size{1.0f, 1.0f};

		// Game camera state — independent world position and logical size.
		// The game camera rectangle acts as a "viewfinder" anchored in world space.
		dodoe::Vector2f m_game_camera_position{0.0f, 0.0f};
		dodoe::Vector2f m_game_camera_size{640.0f, 360.0f};

		void drawGameCameraRect(ImDrawList*           draw_list,
		                        const ImVec2&         vp_min,
		                        const ImVec2&         vp_max,
		                        const dodoe::Camera&  camera) const;

		void drawZoomBar(ImDrawList*          draw_list,
		                 const ImVec2&        vp_min,
		                 const ImVec2&        vp_max,
		                 dodoe::Camera&       camera);

	public:
		explicit ViewportPanel(EditorPanelDescriptor descriptor);
		~ViewportPanel() override;

		void onWorkspaceActivated(const EditorPanelContext& context) override;
		void onWorkspaceDeactivated(const EditorPanelContext& context) override;
		void onUpdate(const EditorPanelContext& context, float delta_time) override;
		void onDraw(const EditorPanelContext& context) override;

		[[nodiscard]] const dodoe::Vector2f& getViewportContentSize() const { return m_viewport_content_size; }
	};

} // cakery
