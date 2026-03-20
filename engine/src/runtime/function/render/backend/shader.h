//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_SHADER_H
#define DODOE_SHADER_H

#include "dopch.h"

namespace dodoe {
	enum class ShaderStage {
		Vertex,
		Fragment,
	};

	enum class ShaderCodeType {
		GLSL,
		SPIRV,
	};

	struct ShaderModuleCreateInfo {
		ShaderStage stage{ShaderStage::Vertex};
		ShaderCodeType code_type{ShaderCodeType::GLSL};
		const void* code{nullptr};
		ui32 code_size{0};
		const char* entry{"main"};

		ShaderModuleCreateInfo() = default;
		ShaderModuleCreateInfo(ShaderStage in_stage, const void* in_code) : stage(in_stage), code(in_code) { }
	};

	struct ShaderCreateInfo {
		ShaderModuleCreateInfo vert_module{};
		ShaderModuleCreateInfo frag_module{};
		void* native_device{nullptr};

		ShaderCreateInfo(const std::string& vert_source, const std::string& frag_source) :
			vert_module(ShaderStage::Vertex, vert_source.c_str()), frag_module(ShaderStage::Fragment, frag_source.c_str()) {
		}
	};

	class Shader {
	public:
		virtual ~Shader() = default;

		static Ref<Shader> create(ShaderCreateInfo create_info);
		void destroy();
		virtual void attach() = 0;
		virtual void detach() = 0;

	protected:
		virtual void initialize(ShaderCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_SHADER_H
