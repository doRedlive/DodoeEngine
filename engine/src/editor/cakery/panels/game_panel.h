// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

#include "runtime/function/render/interface/rhi.h"

namespace cakery {

	class GamePanel : public EditorPanel {
	private:
		dodoe::Vector2f m_game_content_size{1.0f, 1.0f};

	public:
		explicit GamePanel(EditorPanelDescriptor descriptor);
		~GamePanel() override;

		void onWorkspaceActivated(const EditorPanelContext& context) override;
		void onWorkspaceDeactivated(const EditorPanelContext& context) override;
		void onUpdate(const EditorPanelContext& context, float delta_time) override;
		void onDraw(const EditorPanelContext& context) override;

		[[nodiscard]] const dodoe::Vector2f& getGameContentSize() const { return m_game_content_size; }
	};

} // cakery
