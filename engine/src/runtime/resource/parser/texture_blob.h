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

#ifdef DODOE_PERF_ENABLED
        struct MemoryStats {
            Size_t blob_count{0};
            UInt64 pixel_bytes{0};
            UInt64 peak_pixel_bytes{0};
        };
        static MemoryStats QueryMemoryStats();
#endif
    };

} // dodoe
