//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_TEXTURE_H
#define DODOE_GL_TEXTURE_H

#include "dopch.h"

#include "runtime/function/render/backend/texture.h"

namespace dodoe {

	class GlTexture : public Texture {
	public:
		void attach(uint slot = 0) override;
	protected:
		void initialize(TextureCreateInfo create_info) override;
		void shutdown() override;
	};

} // dodoe

#endif//DODOE_GL_TEXTURE_H
