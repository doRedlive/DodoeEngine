//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_TEXTURE_H
#define DODOE_TEXTURE_H

#include "dopch.h"

namespace dodoe {

	struct TextureCreateInfo {
		int width, height;
		uchar* data;
	};

	class Texture {
	public:
		uint id{0};
		uint width{0}, height{0};

		virtual ~Texture() = default;

		static Ref<Texture> create(TextureCreateInfo create_info);
		void destroy();

		virtual void attach(uint slot = 0) = 0;

	protected:
		virtual void initialize(TextureCreateInfo create_info) = 0;
		virtual void shutdown() = 0;
	};

} // dodoe

#endif//DODOE_TEXTURE_H
