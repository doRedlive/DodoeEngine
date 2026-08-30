// do@Redlive

#include "texture_blob.h"

#include "runtime/core/utils/common.h"

#include <cstring>

#include "stb_image.h"

namespace dodoe {

    TextureBlob::TextureBlob(const String& path) {
        load(path);
    }

    TextureBlob::~TextureBlob() {
        if (isValid()) {
            free();
        }
    }

    void TextureBlob::load(const String& path, bool flip_vertical) {
        DO_PROFILE_SCOPE_CATEGORY("TextureBlob::load", "asset");
        stbi_set_flip_vertically_on_load(flip_vertical);
        is_hdr = stbi_is_hdr(path.c_str()) != 0;
        if (is_hdr) {
            pixels = stbi_loadf(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        } else {
            pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        }

		if (!flip_vertical) {
			stbi_set_flip_vertically_on_load(true);
		}

        if (!pixels) {
            DO_ERROR("Load texture {} error!", path);
        } else {
            // DO_DEBUG("TextureBlob: loaded '{}' ({}x{}, channels={}, hdr={})",
            //     path, width, height, channels, is_hdr);
        }
    }

    void TextureBlob::free() {
        if (pixels) stbi_image_free(pixels);
        pixels = nullptr;
    }

} // dodoe
