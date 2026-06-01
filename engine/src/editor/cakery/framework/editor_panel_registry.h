// do@Redlive

#pragma once

#include "dopch.h"

#include "editor_panel.h"

namespace cakery {

	class EditorPanel;
	class EditorPanelManager;

	using EditorPanelFactory = std::function<dodoe::Scope<EditorPanel>()>;

	class EditorPanelRegistry {
	private:
		struct PanelEntry {
			EditorPanelDescriptor descriptor{};
			EditorPanelFactory factory{};
		};

		DynamicArray<String> m_order{};
		UnorderedMap<String, PanelEntry> m_entries{};

	public:
		static EditorPanelRegistry& Self();

		Bool registerPanel(EditorPanelDescriptor descriptor, EditorPanelFactory factory);
		void instantiatePanels(EditorPanelManager& manager) const;
	};

} // cakery
