//
// Created by Redlive on 2026/3/18.
//

#include "gl_shader.h"

#include "glad/glad.h"

namespace {
	GLuint compile_gl_shader(const dodoe::ShaderModuleCreateInfo& module_info, GLenum shader_type) {
		DoAssert(module_info.code, "GlShader::initialize: shader module code must not be null.");
		// DoAssert(module_info.code_size > 0, "GlShader::initialize: shader module code size must be greater than zero.");
		// DoAssert(module_info.code_type == dodoe::ShaderCodeType::GLSL,
		// 	"GlShader::initialize: OpenGL backend only supports GLSL module code.");

		const GLchar* source = reinterpret_cast<const GLchar*>(module_info.code);
		const GLint source_length = static_cast<GLint>(module_info.code_size);
		GLuint shader = glCreateShader(shader_type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint status = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			GLint log_length = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
			std::string log;
			log.resize(static_cast<size_t>(std::max(log_length, 1)));
			glGetShaderInfoLog(shader, log_length, nullptr, log.data());
			glDeleteShader(shader);
			DoError("GlShader::initialize: shader compile failed: {}", log);
		}

		return shader;
	}
}

namespace dodoe {

	int GlShader::get_uniform_location(const std::string& name) {
		if (auto it = uniform_location_cache_.find(name); it != uniform_location_cache_.end()) {
			return it->second;
		}

		const GLint location = glGetUniformLocation(static_cast<GLuint>(renderer_id_), name.c_str());
		if (location == -1) {
			DoError("GlShader: uniform '{}' not found.", name);
		}

		uniform_location_cache_.insert_or_assign(name, static_cast<int>(location));
		return static_cast<int>(location);
	}

	void GlShader::attach() {
		DoAssert(renderer_id_ != 0, "GlShader::attach: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
	}

	void GlShader::detach() {
		glUseProgram(0);
	}

	void GlShader::set_bool(const std::string& name, bool value) {
		set_int(name, value ? 1 : 0);
	}

	void GlShader::set_int(const std::string& name, int value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_int: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniform1i(get_uniform_location(name), value);
	}

	void GlShader::set_float(const std::string& name, float value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_float: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniform1f(get_uniform_location(name), value);
	}

	void GlShader::set_vec2(const std::string& name, const Vector2f& value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_vec2: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniform2f(get_uniform_location(name), value.x, value.y);
	}

	void GlShader::set_vec3(const std::string& name, const Vector3f& value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_vec3: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniform3f(get_uniform_location(name), value.x, value.y, value.z);
	}

	void GlShader::set_vec4(const std::string& name, const Vector4f& value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_vec4: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniform4f(get_uniform_location(name), value.x, value.y, value.z, value.w);
	}

	void GlShader::set_mat4(const std::string& name, const Matrix4f& value) {
		DoAssert(renderer_id_ != 0, "GlShader::set_mat4: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
		glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, &value[0][0]);
	}

	void GlShader::initialize(ShaderCreateInfo create_info) {
		DoAssert(create_info.vert_module.stage == ShaderStage::Vertex,
			"GlShader::initialize: vert_module.stage must be ShaderStage::Vertex.");
		DoAssert(create_info.frag_module.stage == ShaderStage::Fragment,
			"GlShader::initialize: frag_module.stage must be ShaderStage::Fragment.");

		const GLuint vert_shader = compile_gl_shader(create_info.vert_module, GL_VERTEX_SHADER);
		const GLuint frag_shader = compile_gl_shader(create_info.frag_module, GL_FRAGMENT_SHADER);

		renderer_id_ = glCreateProgram();
		DoAssert(renderer_id_ != 0, "GlShader::initialize: failed to create shader program.");

		glAttachShader(static_cast<GLuint>(renderer_id_), vert_shader);
		glAttachShader(static_cast<GLuint>(renderer_id_), frag_shader);
		glLinkProgram(static_cast<GLuint>(renderer_id_));

		GLint link_status = GL_FALSE;
		glGetProgramiv(static_cast<GLuint>(renderer_id_), GL_LINK_STATUS, &link_status);
		if (link_status != GL_TRUE) {
			GLint log_length = 0;
			glGetProgramiv(static_cast<GLuint>(renderer_id_), GL_INFO_LOG_LENGTH, &log_length);
			std::string log;
			log.resize(static_cast<size_t>(std::max(log_length, 1)));
			glGetProgramInfoLog(static_cast<GLuint>(renderer_id_), log_length, nullptr, log.data());
			glDeleteShader(vert_shader);
			glDeleteShader(frag_shader);
			glDeleteProgram(static_cast<GLuint>(renderer_id_));
			renderer_id_ = 0;
			DoAssert(false, "GlShader::initialize: shader program link failed: {}", log);
		}

		glDetachShader(static_cast<GLuint>(renderer_id_), vert_shader);
		glDetachShader(static_cast<GLuint>(renderer_id_), frag_shader);
		glDeleteShader(vert_shader);
		glDeleteShader(frag_shader);

		uniform_location_cache_.clear();
	}

	void GlShader::shutdown() {
		if (renderer_id_ == 0) {
			return;
		}

		glDeleteProgram(static_cast<GLuint>(renderer_id_));
		renderer_id_ = 0;
		uniform_location_cache_.clear();
	}

} // dodoe
