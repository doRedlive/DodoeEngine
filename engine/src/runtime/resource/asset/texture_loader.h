//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_TEXTURE_LOADER_H
#define DODOE_TEXTURE_LOADER_H

#include "dopch.h"

#include "../resource_type.h"

namespace dodoe {

    struct TextureBlob {
        int width{0}, height{0};
        int channels{0};
        uchar* pixels{nullptr};

        TextureBlob() = default;
        explicit TextureBlob(const std::string& paht);
        ~TextureBlob();

        void load(const std::string& path);
        void free();

        bool isValid() const { return pixels != nullptr; }
    };

} // dodoe

#endif //DODOE_TEXTURE_LOADER_H
