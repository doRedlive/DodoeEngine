// Created by Redlive on 2026/4/6.

#pragma once

#include "dopch.h"

#include "interface/rhi.h"

namespace dodoe {

	struct RenderPassCreateInfo {
		rhi::DeviceHandle device;
	};

	class RenderPass {
	protected:
		std::string name_{};
		rhi::DeviceHandle device_{};
	public:
		explicit RenderPass(const RenderPassCreateInfo& info) : device_(info.device) {}

		virtual ~RenderPass() = default;

		virtual void execute() = 0;
		virtual void setup() {}
		virtual void cleanup() {}

	protected:
		void setName(const std::string& name);
		void addWriteResource();
		void addReadResource();
	};

} // dodoe
