// Created by Redlive on 2026/4/15.

#include "render_scene.h"

namespace dodoe {

	void RenderScene::submitMainCameraPacket(const MainCameraDrawPacket& packet) {
		logic_main_camera_packets_.push_back(packet);
	}

	void RenderScene::swapLogicRenderContext() {
		render_main_camera_packets_.swap(logic_main_camera_packets_);
		logic_main_camera_packets_.clear();
	}

} // dodoe
