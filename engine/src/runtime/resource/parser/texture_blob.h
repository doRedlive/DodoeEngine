// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    struct TextureBlob {
        int width{0}, height{0};
        int channels{0};
        bool is_hdr{false};
        void* pixels{nullptr};

        TextureBlob() = default;
        explicit TextureBlob(const String& paht);
        ~TextureBlob();

        void load(const String& path, bool flip_vertical = true);
        void free();

        bool isValid() const { return pixels != nullptr; }
    };

} // dodoe
