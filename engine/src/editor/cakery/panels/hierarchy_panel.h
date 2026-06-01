// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"

#include <array>
#include <unordered_set>

namespace cakery {

	class HierarchyPanel : public EditorPanel {
	private:
		dodoe::Scene* m_context{nullptr};
		dodoe::ui32 m_editing_handle{0};
		std::array<char, 128> m_search_buffer{};

	public:
		explicit HierarchyPanel(EditorPanelDescriptor descriptor);
		void onWorkspaceActivated(const EditorPanelContext& context) override;
		void onWorkspaceDeactivated(const EditorPanelContext& context) override;
		void onDraw(const EditorPanelContext& context) override;

	private:
		void drawEntityNode(dodoe::Entity entity, std::unordered_set<dodoe::ui32>& visited);
		void drawSearchBar();
		void drawCreateButton();
	};

} // cakery
