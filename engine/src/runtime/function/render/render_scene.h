// Created by Redlive on 2026/4/15.

#pragma once

#include "dopch.h"

namespace dodoe {

	struct MainCameraDrawPacket {
		identifier entity_id{0};
		identifier model_id{0};
		Matrix4f model_matrix{1.0f};
		Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
	};

	class RenderScene {
	public:
		void submitMainCameraPacket(const MainCameraDrawPacket& packet);
		void swapLogicRenderContext();
		[[nodiscard]] const std::vector<MainCameraDrawPacket>& mainCameraPackets() const { return render_main_camera_packets_; }

	private:
		std::vector<MainCameraDrawPacket> logic_main_camera_packets_{};
		std::vector<MainCameraDrawPacket> render_main_camera_packets_{};
	};

} // dodoe
