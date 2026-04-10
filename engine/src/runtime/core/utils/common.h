//
// Created by GreenMuffin on 2026/1/22.
//

#ifndef DODOE_COMMON_H
#define DODOE_COMMON_H

#include "dopch.h"
#include "entt/entt.hpp"

namespace dodoe {

    inline uint32_t String2Hash(const std::string& str) {
        return entt::hashed_string{ str.c_str() }.value();
    }

    inline void name_remove_namespace(std::string& name) {
        if (const auto pos = name.find_last_of(':'); pos != std::string::npos) {
            name = name.substr(pos + 1);
        }
    }

	inline std::vector<char> ReadShaderFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open()) {
			DoError("Open shader file {} failed!", path);
			return {};
		}

		const std::streamsize size = in.tellg();
		in.seekg(0, std::ios::beg);

		std::vector<char> content(static_cast<size_t>(size));
		in.read(content.data(), size);
		return content;
	}


} // dodoe

#endif //DODOE_COMMON_H