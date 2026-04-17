//
// Created by Redlive on 2026/3/19.
//

#include "texture_loader.h"

#include "runtime/core/utils/common.h"

#include <cstring>

#include "stb_image.h"

namespace dodoe {

    TextureBlob::TextureBlob(const std::string& path) {
        load(path);
    }

    TextureBlob::~TextureBlob() {
        if (isValid()) {
            free();
        }
    }

    void TextureBlob::load(const std::string& path) {
        stbi_set_flip_vertically_on_load(true);
        pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            DoError("Load texture {} error!", path);
        }
    }

    void TextureBlob::free() {
        if (pixels) stbi_image_free(pixels);
    }

} // dodoe
