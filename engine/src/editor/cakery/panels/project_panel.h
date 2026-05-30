// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/resource_type.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "cakery/framework/editor_panel.h"

namespace fs = std::filesystem;

namespace cakery {
	class ProjectPanel : public EditorPanel {
		fs::path cur_directory_;
		fs::path base_directory_;
        fs::path last_base_directory_;
		dodoe::TextureRes directory_icon_;
		dodoe::TextureRes file_icon_;
		dodoe::Ref<dodoe::Texture> directory_icon_texture_{nullptr};
		dodoe::Ref<dodoe::Texture> file_icon_texture_{nullptr};
	public:
		explicit ProjectPanel(EditorPanelDescriptor descriptor);
		~ProjectPanel() override;
		void onWorkspaceDeactivated(const EditorPanelContext& context) override;
		void onDraw(const EditorPanelContext& context) override;
	private:
        void updateBaseDirectory();
		void initializeIconTextures();
		[[nodiscard]] dodoe::Ref<dodoe::Texture> getIconTexture(bool is_directory) const;
	};

} // cakery
