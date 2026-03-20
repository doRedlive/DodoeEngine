//
// Created by GreenMuffin on 2026/2/22.
//

#include "viewport_panel.h"

#include "imgui/imgui.h"

using namespace dodoe;

namespace cakery {

	ViewportPanel::ViewportPanel() {
		FrameBufferSpecification fb_spec;
		fb_spec.attachment_specification = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::RED_INTEGER, FrameBufferTextureFormat::Depth };
		fb_spec.width = 1280;
		fb_spec.height = 720;

		frame_buffer_ = FrameBuffer::create(fb_spec);
	}

	void ViewportPanel::on_update() {
		if (auto spec = frame_buffer_->specification();
			size_.x > 0.0f && size_.y > 0.0f && (spec.width != size_.x || spec.height != size_.y)) {
			frame_buffer_->resize(static_cast<uint32_t>(size_.x), static_cast<uint32_t>(size_.y));
		}
		frame_buffer_->attach();
		frame_buffer_->clear_attachment(1, -1);
	}

	void ViewportPanel::on_ui_render() {
		ImGui::Begin("Viewport");

		auto viewport_panel_size = ImGui::GetContentRegionAvail();
		size_ = { viewport_panel_size.x, viewport_panel_size.y };

		uint64_t texture_id = frame_buffer_->color_attachment_renderer_id();
		ImGui::Image(texture_id, ImVec2{ size_.x, size_.y }, ImVec2{ 0, 1 }, ImVec2{ 1,0 });

		ImGui::End();
	}

} // cakery