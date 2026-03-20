//
// Created by Redlive on 2026/3/17.
//

#include "texture.h"

#include "runtime/function/render/render_api.h"

#include "opengl/gl_texture.h"

namespace dodoe {
	
	namespace {
		void texture_deleter(Texture* texture) {
			if (!texture) {
				return;
			}

			texture->destroy();
			delete texture;
		}
	}

	Ref<Texture> Texture::create(TextureCreateInfo create_info) {
		Ref<Texture> texture {};

		switch (RenderApi::api_type()) {
		case RenderApiType::OpenGL:
			texture = Ref<Texture>(new GlTexture(), texture_deleter);
			break;
		case RenderApiType::Vulkan:
			DoAssert(false, "Texture::create: Vulkan backend texture is not implemented yet.");
			break;
		case RenderApiType::None:
		default:
			DoAssert(false, "Texture::create: Invalid render api type.");
			break;
		}

		DoAssert(texture, "Texture::create: Create texture failure.");
		texture->initialize(create_info);
		return texture;
	}

	void Texture::destroy() {
		shutdown();
	}

} // dodoe
