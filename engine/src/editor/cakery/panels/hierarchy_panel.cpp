// do@Redlive

#include "hierarchy_panel.h"

#include "../cakery_event.h"

#include "runtime/core/event/event_system.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/world/components.h"

#include <cstring>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {

	HierarchyPanel::HierarchyPanel(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
	}

	void HierarchyPanel::onWorkspaceActivated(const EditorPanelContext& context) {
		if (context.system_context.world) {
			m_context = context.system_context.world->getCurrentScene();
		}
	}

	void HierarchyPanel::onWorkspaceDeactivated(const EditorPanelContext& context) {
		(void)context;
		m_context = nullptr;
	}

	void HierarchyPanel::drawSearchBar() {
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##HierarchySearch", "Search...",
			m_search_buffer.data(), static_cast<Int32>(m_search_buffer.size()));
	}

	void HierarchyPanel::drawCreateButton() {
		if (ImGui::Button("Create")) {
			ImGui::OpenPopup("CreateEntityPopup");
		}

		if (ImGui::BeginPopup("CreateEntityPopup")) {
			if (ImGui::MenuItem("Create Empty")) {
				if (m_context) {
					m_context->createEntity("Entity");
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void HierarchyPanel::onDraw(const EditorPanelContext& context) {
		(void)context;
		ImGui::Begin("Hierarchy");

		// Search bar at top (Unity-style)
		drawSearchBar();
		ImGui::SameLine();
		drawCreateButton();

		ImGui::Spacing();

		if (!m_context) {
			ImGui::End();
			return;
		}

		ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		auto entities = m_context->getEntities();
		std::unordered_set<dodoe::ui32> visited;

		// Filter by search text
		const char* search_text = m_search_buffer.data();
		const Bool has_filter = search_text != nullptr && search_text[0] != '\0';

		for (auto entity : entities) {
			if (!entity || !m_context->registry().valid(entity)) {
				continue;
			}

			if (entity.hasComponent<dodoe::HierarchyComponent>()) {
				const auto& hierarchy = entity.getComponent<dodoe::HierarchyComponent>();
				if (hierarchy.parent && hierarchy.parent.valid()) {
					continue;
				}
			}

			if (has_filter) {
				const auto& name = entity.name();
				if (name.find(search_text) == String::npos) {
					continue;
				}
			}

			drawEntityNode(entity, visited);
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
			&& ImGui::IsWindowHovered()
			&& !ImGui::IsAnyItemHovered()) {
			dodoe::EventSystem::Enqueue<NonSelectEntityEvent>();
		}

		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			if (ImGui::MenuItem("Create Entity")) {
				m_context->createEntity("Entity");
			}
			ImGui::EndPopup();
		}

		ImGui::EndChild();
		ImGui::End();
	}

	void HierarchyPanel::drawEntityNode(dodoe::Entity entity, std::unordered_set<dodoe::ui32>& visited) {
		if (!entity || !m_context->registry().valid(entity)) {
			return;
		}

		const auto entity_id = static_cast<dodoe::ui32>(entity);
		if (!visited.insert(entity_id).second) {
			return;
		}

		const auto& name = entity.name();
		static char edit_name_buf[256]{};

		ImGui::PushID(static_cast<Int32>(entity_id));

		const Bool has_children = entity.hasComponent<dodoe::HierarchyComponent>()
			&& !entity.getComponent<dodoe::HierarchyComponent>().children.empty();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_OpenOnDoubleClick
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_FramePadding;
		if (!has_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		const Bool open = ImGui::TreeNodeEx("##entity_node", flags, "%s", name.c_str());

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			dodoe::EventSystem::Enqueue<SelectEntityEvent>(entity);
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			m_editing_handle = static_cast<dodoe::ui32>(entity);
			{
				const Size_t copy_len = (std::min)(name.size(), sizeof(edit_name_buf) - 1);
				std::memcpy(edit_name_buf, name.data(), copy_len);
				edit_name_buf[copy_len] = '\0';
			}
			ImGui::OpenPopup("EditEntityPopup");
		}

		Bool request_delete = false;
		if (ImGui::BeginPopupContextItem("EntityContext")) {
			if (ImGui::MenuItem("Delete")) {
				request_delete = true;
			}
			ImGui::EndPopup();
		}

		if (m_editing_handle == static_cast<dodoe::ui32>(entity) && ImGui::BeginPopup("EditEntityPopup")) {
			ImGui::SetKeyboardFocusHere(0);
			const Bool name_changed = ImGui::InputText("##edit_name", edit_name_buf, sizeof(edit_name_buf), ImGuiInputTextFlags_EnterReturnsTrue);
			if (name_changed || ImGui::IsItemDeactivatedAfterEdit()) {
				entity.getComponent<dodoe::IDComponent>().setName(edit_name_buf);
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_editing_handle = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		} else if (m_editing_handle == static_cast<dodoe::ui32>(entity) && !ImGui::IsPopupOpen("EditEntityPopup")) {
			m_editing_handle = 0;
		}

		if (request_delete) {
			if (auto& app = dodoe::Application::Self(); app.context().script_system) {
				if (auto* runtime = app.context().script_system->getMonoRuntime()) {
					runtime->removeEntityFromManagedWorld(static_cast<UInt64>(entity.uuid()));
				}
			}
			dodoe::EventSystem::Enqueue<NonSelectEntityEvent>();
			m_context->destroyEntity(entity);
		}

		if (open && has_children) {
			auto& hierarchy = entity.getComponent<dodoe::HierarchyComponent>();
			for (const auto& child : hierarchy.children) {
				drawEntityNode(child, visited);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

} // cakery
