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

	protected:
		void initialize(ShaderCreateInfo create_info) override;
		void shutdown() override;

	private:
		uint renderer_id_{0};
	};

} // dodoe

#endif//DODOE_GL_SHADER_H
