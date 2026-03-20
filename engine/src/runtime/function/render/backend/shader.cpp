//
// Created by Redlive on 2026/3/17.
//

#include "shader.h"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_shader.h"
#include "vulkan/vk_shader.h"

namespace dodoe {

	namespace {
		void shader_deleter(Shader* shader) {
			if (!shader) {
				return;
			}

			shader->destroy();
			delete shader;
		}
	}

	Ref<Shader> Shader::create(ShaderCreateInfo create_info) {
		Ref<Shader> shader {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			shader = Ref<Shader>(new GlShader(), shader_deleter);
			break;
		case RenderApiType::Vulkan:
			shader = Ref<Shader>(new VkShader(), shader_deleter);
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "Shader::create: Invalid render api type.");
			break;
		}

		DoAssert(shader, "Shader::create: Create shader failure.");
		shader->initialize(create_info);
		return shader;
	}

	void Shader::destroy() {
		shutdown();
	}

} // dodoe
