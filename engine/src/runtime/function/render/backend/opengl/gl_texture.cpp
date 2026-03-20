//
// Created by Redlive on 2026/3/18.
//

#include "gl_texture.h"

#include "glad/glad.h"

namespace dodoe {

	void GlTexture::attach(uint slot) {
		glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
		glBindTexture(GL_TEXTURE_2D, id);
	}

	void GlTexture::initialize(TextureCreateInfo create_info) {
		GLuint texture_id = 0;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, create_info.width, create_info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, create_info.data);
        glBindTexture(GL_TEXTURE_2D, 0);

		id = texture_id;
		width = create_info.width;
		height = create_info.height;
	}

	void GlTexture::shutdown() {
		if (id != 0) {
			glDeleteTextures(1, &id);
			id = 0;
		}
	}

} // dodoe
