// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/framework/texture_manager.h"
#include "cakery/framework/editor_panel.h"

#include <array>

namespace fs = std::filesystem;

namespace cakery {

	class ProjectPanel : public EditorPanel {
	private:
		fs::path m_cur_directory;
		fs::path m_base_directory;
		fs::path m_last_base_directory;
		String m_directory_icon_path{};
		String m_file_icon_path{};
		dodoe::Ref<dodoe::Texture> m_directory_icon_texture{nullptr};
		dodoe::Ref<dodoe::Texture> m_file_icon_texture{nullptr};

	public:
		explicit ProjectPanel(EditorPanelDescriptor descriptor);
		~ProjectPanel() override;
		void onWorkspaceDeactivated(const EditorPanelContext& context) override;
		void onDraw(const EditorPanelContext& context) override;

	private:
		void updateBaseDirectory();
		void initializeIconTextures();
		[[nodiscard]] dodoe::Ref<dodoe::Texture> getIconTexture(Bool is_directory) const;
	};

} // cakery
