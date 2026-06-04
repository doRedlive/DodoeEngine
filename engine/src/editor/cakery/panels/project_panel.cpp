// do@Redlive

#include "project_panel.h"

#include "runtime/function/render/framework/texture.h"
#include "runtime/core/project/project.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {

	namespace {
		dodoe::TextureManager* GetTextureManager() {
			auto& app = dodoe::Application::Self();
			auto* render_system = app.context().render_system.get();
			return render_system->getTextureManager();
		}
	}

	ProjectPanel::ProjectPanel(EditorPanelDescriptor descriptor)
		: EditorPanel(std::move(descriptor)) {
		updateBaseDirectory();
		m_cur_directory = m_base_directory;
		m_last_base_directory = m_base_directory;

		m_directory_icon_path = "engine/res/pictures/ContentBrowser/DirectoryIcon.png";
		m_file_icon_path = "engine/res/pictures/ContentBrowser/FileIcon.png";

		initializeIconTextures();
	}

	ProjectPanel::~ProjectPanel() {
		m_directory_icon_texture = nullptr;
		m_file_icon_texture = nullptr;
	}

	void ProjectPanel::onWorkspaceDeactivated(const EditorPanelContext& context) {
		(void)context;
		m_directory_icon_texture = nullptr;
		m_file_icon_texture = nullptr;
	}

	void ProjectPanel::onDraw(const EditorPanelContext& context) {
		(void)context;
		updateBaseDirectory();
		if (m_base_directory != m_last_base_directory) {
			m_cur_directory = m_base_directory;
			m_last_base_directory = m_base_directory;
		}

		ImGui::Begin("Project");

		std::error_code ec;
		for (auto& directory_entry : fs::directory_iterator(m_cur_directory, ec)) {
			const auto& path = directory_entry.path();
			const String file_name = path.filename().string();
			const Bool is_directory = directory_entry.is_directory();

			ImGui::PushID(file_name.c_str());

			if (!m_directory_icon_texture || !m_file_icon_texture) {
				initializeIconTextures();
			}

			constexpr float kItemHeight = 40.0f;
			const Bool selected = ImGui::Selectable("##project_item", false,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
				ImVec2(0, kItemHeight));

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (is_directory) {
					m_cur_directory /= path.filename();
				}
			}

			ImGui::SameLine();
			auto icon_tex = getIconTexture(is_directory);
			if (icon_tex && icon_tex->getGpuHandle()) {
				constexpr float kIconSize = 28.0f;
				const float icon_offset_y = (kItemHeight - kIconSize) * 0.5f;
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + icon_offset_y);
				const ImTextureRef icon_ref(reinterpret_cast<ImTextureID>(icon_tex->getGpuHandle().Get()));
				ImGui::Image(icon_ref, ImVec2(kIconSize, kIconSize), ImVec2(0, 1), ImVec2(1, 0));
			}
			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (kItemHeight - ImGui::GetTextLineHeight()) * 0.5f);
			ImGui::TextUnformatted(file_name.c_str());

			if (selected) {
				(void)is_directory;
			}

			ImGui::PopID();
		}

		if (m_cur_directory != m_base_directory) {
			ImGui::Separator();
			if (ImGui::Button("<- Back")) {
				m_cur_directory = m_cur_directory.parent_path();
			}
		}

		ImGui::End();
	}

	void ProjectPanel::updateBaseDirectory() {
		fs::path desired = FileSystem::EngineResPath;
		if (const auto active_project = Project::ActiveProject()) {
			desired = Project::ProjectDirectory() / active_project->config().asset_directory;
		}
		desired = desired.lexically_normal();

		m_base_directory = desired;
		if (!fs::exists(m_base_directory) || !fs::is_directory(m_base_directory)) {
			m_base_directory = FileSystem::EngineResPath;
		}
	}

	void ProjectPanel::initializeIconTextures() {
		if (m_directory_icon_texture && m_file_icon_texture) {
			return;
		}

		m_directory_icon_texture = dodoe::Texture::Load(m_directory_icon_path);
		m_file_icon_texture = dodoe::Texture::Load(m_file_icon_path);
	}

	Ref<Texture> ProjectPanel::getIconTexture(const Bool is_directory) const {
		auto texture = is_directory ? m_directory_icon_texture : m_file_icon_texture;
		if (texture && texture->getGpuHandle()) {
			return texture;
		}
		return nullptr;
	}

} // cakery
