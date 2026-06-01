// do@Redlive

#pragma once

#include "dopch.h"

#include <memory>

#include "dockspace_layout_builder.h"
#include "editor_panel.h"

namespace cakery {

	class EditorPanelManager {
	private:
		DynamicArray<Scope<EditorPanel>> m_panels{};
		UnorderedMap<String, EditorPanel*> m_panels_by_id{};
		DockspaceLayoutBuilder m_layout_builder{};
		ImGuiID m_dockspace_id{0};
		Bool m_initialized{false};
		Bool m_workspace_active{false};
		Bool m_layout_dirty{true};
	public:
		void registerPanel(Scope<EditorPanel> panel);

		void initialize(const EditorPanelContext& context);
		void shutdown(const EditorPanelContext& context);

		void setWorkspaceActive(Bool active, const EditorPanelContext& context);
		void update(EditorPanelStage stage, const EditorPanelContext& context, float delta_time);
		void draw(EditorPanelStage stage, const EditorPanelContext& context);

		void drawViewMenuItems();
		void requestLayoutReset();

		[[nodiscard]] EditorPanel* findPanel(const String& id) const;

	private:
		Bool stageMatches(EditorPanelStage panel_stage, EditorPanelStage active_stage) const;
		Bool panelRequiresRuntime(const EditorPanelDescriptor& descriptor, const EditorPanelContext& context) const;
		Bool shouldProcessPanel(const EditorPanel& panel, const EditorPanelContext& context, EditorPanelStage stage) const;
		void sortPanels();
		void applyDefaultLayout(EditorPanelStage stage);
};

} // cakery
