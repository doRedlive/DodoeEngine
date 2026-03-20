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

	void GlShader::attach() {
		DoAssert(renderer_id_ != 0, "GlShader::attach: shader program is not initialized.");
		glUseProgram(static_cast<GLuint>(renderer_id_));
	}

	void GlShader::detach() {
		glUseProgram(0);
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
	}

	void GlShader::shutdown() {
		if (renderer_id_ == 0) {
			return;
		}

		glDeleteProgram(static_cast<GLuint>(renderer_id_));
		renderer_id_ = 0;
	}

} // dodoe
