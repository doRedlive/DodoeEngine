// do@Redlive

#pragma once

#include "dopch.h"

#include "editor_panel.h"

struct ImGuiViewport;

typedef unsigned int ImGuiID;

namespace cakery {

	class EditorPanel;

	class DockspaceLayoutBuilder {
	public:
		void buildDefaultLayout(ImGuiID dockspace_id,
			const ImGuiViewport* viewport,
			const DynamicArray<const EditorPanel*>& panels,
			EditorPanelStage active_stage) const;
	};

} // cakery
