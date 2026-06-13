// do@Redlive

#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"

#include "entt/entt.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace dodoe {

    template <typename T>
    using Scope = std::unique_ptr<T>;
    template <typename T, typename... Args>
    constexpr Scope<T> create_scope(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    using Ref = std::shared_ptr<T>;
    template <typename T, typename... Args>
    constexpr Ref<T> create_ref(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    using Weak = std::weak_ptr<T>;

    using uint     = unsigned int;
    using uchar    = unsigned char;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using ui8  = uint8_t;
    using ui16 = uint16_t;
    using ui32 = uint32_t;
    using ui64 = uint64_t;

    using Int = int;
    using UInt = unsigned int;
    using Float = float;
    using Byte = char;
    using Char = char;
    using UByte = unsigned char;
    using UChar = unsigned char;
    using Bool = bool;
    using Size_t = size_t;

    using Int8 = int8_t;
    using Int16 = int16_t;
    using Int32 = int32_t;
    using Int64 = int64_t;
    using UInt8  = uint8_t;
    using UInt16 = uint16_t;
    using UInt32 = uint32_t;
    using UInt64 = uint64_t;

    using String = std::string;
    using StringView = std::string_view;

    template <typename T>
    using DynamicArray = std::vector<T>;

    template <typename TKey, typename TValue>
    using UnorderedMap = std::unordered_map<TKey, TValue>;

    template <typename TKey, typename TValue>
    using Dictionary = UnorderedMap<TKey, TValue> ;


    using Vector2f = glm::vec2;
    using Vector2i = glm::ivec2;
    using Vector3f = glm::vec3;
    using Vector3i = glm::ivec3;
    using Vector4f = glm::vec4;
    using Vector4i = glm::ivec4;
    using Matrix3f = glm::mat3;
    using Matrix4f = glm::mat4;

    using identifier = entt::id_type;
    using Identifier = entt::id_type;

    using InstanceID = Int32;

} // dodoe
