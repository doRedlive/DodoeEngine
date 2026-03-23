//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_SHADER_H
#define DODOE_GL_SHADER_H

#include "dopch.h"

#include "runtime/function/render/backend/shader.h"

namespace dodoe {

	class GlShader : public Shader {
	public:
		void attach() override;
		void detach() override;

		void set_bool(const std::string& name, bool value) override;
		void set_int(const std::string& name, int value) override;
		void set_float(const std::string& name, float value) override;
		void set_vec2(const std::string& name, const Vector2f& value) override;
		void set_vec3(const std::string& name, const Vector3f& value) override;
		void set_vec4(const std::string& name, const Vector4f& value) override;
		void set_mat4(const std::string& name, const Matrix4f& value) override;

	protected:
		void initialize(ShaderCreateInfo create_info) override;
		void shutdown() override;

	private:
		int get_uniform_location(const std::string& name);

		uint renderer_id_{0};
		std::unordered_map<std::string, int> uniform_location_cache_{};
	};

} // dodoe

#endif//DODOE_GL_SHADER_H
