//
// Created by GreenMuffin on 2025/11/27.
//

#ifndef DODOE_UTIL_H
#define DODOE_UTIL_H

#include "dopch.h"


namespace dodoe {
    struct Color {
        float r{1.0f};
        float g{1.0f};
        float b{1.0f};
        float a{1.0f};

        Vector4f to_vec4() const {
            return {r, g, b, a};
        }

        static Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
        static Color Red()   { return {1.0f, 0.0f, 0.0f, 1.0f}; }
        static Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
        static Color Blue()  { return {0.0f, 0.0f, 1.0f, 1.0f}; }
        static Color Gray()  { return {0.5f, 0.5f, 0.5f, 1.0f}; }
    };

    inline char* read_bytes(const std::string& file_path, uint32_t* out_size) {
		std::ifstream stream(file_path, std::ios::binary | std::ios::ate);

		if (!stream) {
			return nullptr;
		}

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t size = end - stream.tellg();

		if (size == 0) {
			return nullptr;
		}

		char* buffer = new char[size];
		stream.read((char*)buffer, size);
		stream.close();

		*out_size = size;
		return buffer;
	}

    
} // dodoe

#endif //DODOE_UTIL_H