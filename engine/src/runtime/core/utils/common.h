// do@Redlive

#pragma once

#include "dopch.h"
#include "entt/entt.hpp"

namespace dodoe {

    inline uint32_t string2hash(const std::string& str) {
        return entt::hashed_string{ str.c_str() }.value();
    }

    inline void NameRemoveNamespace(std::string& name) {
        if (const auto pos = name.find_last_of(':'); pos != std::string::npos) {
            name = name.substr(pos + 1);
        }
    }

	inline DynamicArray<char> ReadShaderFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open()) {
			DO_ERROR("Open shader file {} failed!", path);
			return {};
		}

		const std::streamsize size = in.tellg();
		in.seekg(0, std::ios::beg);

		std::vector<char> content(static_cast<size_t>(size));
		in.read(content.data(), size);
		return content;
	}


    inline DynamicArray<float> RotateCubemapFaceCW(const float* src, Int32 w, Int32 h) {
        DynamicArray<float> dst(static_cast<Size_t>(w) * static_cast<Size_t>(h) * 4u);
        for (Int32 y = 0; y < h; ++y) {
            for (Int32 x = 0; x < w; ++x) {
                Int32 sx = y, sy = h - 1 - x;
                auto si = (static_cast<Size_t>(sy) * static_cast<Size_t>(w) + static_cast<Size_t>(sx)) * 4u;
                auto di = (static_cast<Size_t>(y) * static_cast<Size_t>(w) + static_cast<Size_t>(x)) * 4u;
                dst[di] = src[si]; dst[di + 1] = src[si + 1];
                dst[di + 2] = src[si + 2]; dst[di + 3] = src[si + 3];
            }
        }
        return dst;
    }

    inline DynamicArray<float> RotateCubemapFaceCCW(const float* src, Int32 w, Int32 h) {
        DynamicArray<float> dst(static_cast<Size_t>(w) * static_cast<Size_t>(h) * 4u);
        for (Int32 y = 0; y < h; ++y) {
            for (Int32 x = 0; x < w; ++x) {
                Int32 sx = w - 1 - y, sy = x;
                auto si = (static_cast<Size_t>(sy) * static_cast<Size_t>(w) + static_cast<Size_t>(sx)) * 4u;
                auto di = (static_cast<Size_t>(y) * static_cast<Size_t>(w) + static_cast<Size_t>(x)) * 4u;
                dst[di] = src[si]; dst[di + 1] = src[si + 1];
                dst[di + 2] = src[si + 2]; dst[di + 3] = src[si + 3];
            }
        }
        return dst;
    }

} // dodoe
