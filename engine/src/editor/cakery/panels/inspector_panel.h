// do@Redlive

#pragma once

#include "dopch.h"

#include <unordered_set>

#include "cakery/cakery_event.h"
#include "cakery/framework/editor_panel.h"

#include "runtime/function/world/entity.h"

namespace dodoe {
	class MonoComponentInstance;
	class ScriptClass;
}

namespace cakery {

	class InspectorPanel : public EditorPanel {
	private:
		UnorderedMap<String, std::function<void(String, void*)>> m_component_ui_drawer{};
		String m_current_component_name{};
		dodoe::Entity m_selected_entity;
		char m_search_buffer[256]{};

	public:
		explicit InspectorPanel(EditorPanelDescriptor descriptor);
		~InspectorPanel() override;
		void onDraw(const EditorPanelContext& context) override;

	private:
		void initializeComponentDrawers();
		void drawComponents();
		void drawComponentGroup(const String& comp_name);
		void drawNode(const String& comp_name);
		void drawButtons(dodoe::Entity entity);
		void drawMonoComponents();
		void drawMonoNode(const String& comp_name, dodoe::MonoComponentInstance& component_instance, dodoe::ScriptClass& script_class);
		void drawPropertyLabel(const String& label);
		void markCurrentComponentDirty();

		void onSelectEntity(const SelectEntityEvent& event);
		void onNonSelectEntity();
	};

} // cakery
