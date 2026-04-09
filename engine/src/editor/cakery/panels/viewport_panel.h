//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef CAKERY_VIEWPORT_PANEL_H
#define CAKERY_VIEWPORT_PANEL_H

#include "dopch.h"

#include "runtime/function/render/backend/frame_buffer.h"

namespace cakery {
	class ViewportPanel {
	public:
		ViewportPanel();
		~ViewportPanel() = default;

		void on_render();
		void on_update();

	private:
		dodoe::Ref<dodoe::FrameBuffer> frame_buffer_;
		dodoe::Vector2f size_{ 0.0f, 0.0f };
	};
} // cakery

#endif//CAKERY_VIEWPORT_PANEL_H
