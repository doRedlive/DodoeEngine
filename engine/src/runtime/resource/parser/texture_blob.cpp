// do@Redlive

#include "texture_blob.h"

#include "runtime/core/utils/common.h"

#include <cstring>

#include "stb_image.h"

namespace dodoe {

#ifdef DODOE_PERF_ENABLED
    namespace {
        std::atomic<Size_t> s_texture_blob_count{0};
        std::atomic<UInt64> s_texture_pixel_bytes{0};
        std::atomic<UInt64> s_texture_pixel_peak{0};

        UInt64 ComputePixelBytes(const int width, const int height, const int channels, const bool is_hdr) {
            return static_cast<UInt64>(width) * static_cast<UInt64>(height) * static_cast<UInt64>(channels)
                * (is_hdr ? sizeof(float) : sizeof(UInt8));
        }

        void RecordPeak(std::atomic<UInt64>& peak, const UInt64 current) {
            UInt64 expected = peak.load(std::memory_order_relaxed);
            while (current > expected &&
                   !peak.compare_exchange_weak(expected, current, std::memory_order_relaxed, std::memory_order_relaxed)) {
            }
        }
    }
#endif

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
        if (pixels) {
            free();
        }
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
#ifdef DODOE_PERF_ENABLED
            s_texture_blob_count.fetch_add(1, std::memory_order_relaxed);
            const UInt64 pixel_bytes_now = s_texture_pixel_bytes.fetch_add(ComputePixelBytes(width, height, channels, is_hdr), std::memory_order_relaxed);
            RecordPeak(s_texture_pixel_peak, pixel_bytes_now + ComputePixelBytes(width, height, channels, is_hdr));
#endif
        }
    }

    void TextureBlob::free() {
        if (pixels) {
#ifdef DODOE_PERF_ENABLED
            s_texture_blob_count.fetch_sub(1, std::memory_order_relaxed);
            s_texture_pixel_bytes.fetch_sub(ComputePixelBytes(width, height, channels, is_hdr), std::memory_order_relaxed);
#endif
            stbi_image_free(pixels);
        }
        pixels = nullptr;
    }

#ifdef DODOE_PERF_ENABLED
    TextureBlob::MemoryStats TextureBlob::QueryMemoryStats() {
        MemoryStats stats;
        stats.blob_count = s_texture_blob_count.load(std::memory_order_relaxed);
        stats.pixel_bytes = s_texture_pixel_bytes.load(std::memory_order_relaxed);
        stats.peak_pixel_bytes = s_texture_pixel_peak.load(std::memory_order_relaxed);
        return stats;
    }
#endif

} // dodoe
