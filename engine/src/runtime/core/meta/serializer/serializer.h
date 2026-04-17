//
// Created by Redlive on 2026/3/20.
//

#ifndef DODOE_SERIALIZER_H
#define DODOE_SERIALIZER_H

#include "dopch.h"

#include "runtime/core/meta/json.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/utils/util.h"

namespace dodoe {
    template <typename...>
    inline constexpr bool always_false = false;

    class Serializer {
    public:
        template <typename T>
        static Json write_pointer(T* instance) {
            return Json{ { "$typeName", "*" }, { "$context", Serializer::write(*instance) } };
        }

        template <typename T>
        static T*& read_pointer(const Json& json_context, T*& instance) {
            DO_ASSERT(instance == nullptr);
            DO_ASSERT(json_context.is_object(), "Serializer::read_pointer expects object");
            DO_ASSERT(json_context.contains("$typeName"), "Serializer::read_pointer missing $typeName");
            DO_ASSERT(json_context.contains("$context"), "Serializer::read_pointer missing $context");

            std::string type_name = json_context.at("$typeName").get<std::string>();
            DO_ASSERT(!type_name.empty(), "Serializer::read_pointer empty $typeName");

            if ('*' == type_name[0]) {
                instance = new T;
                read(json_context.at("$context"), *instance);
            }
            else {
                auto reflection_instance = TypeMeta::new_from_name_and_json(type_name, json_context.at("$context"));
                instance = static_cast<T*>(reflection_instance.instance);
            }
            return instance;
        }

        template <typename T>
        static Json write(const ReflectionPtr<T>& instance) {
            T* instance_ptr = instance.get_ptr();
            const std::string& type_name    = instance.get_type_name();
            if (!instance_ptr) {
                return Json{ { "$typeName", type_name }, { "$context", Json() } };
            }
            return Json{ { "$typeName", type_name },
                         { "$context", TypeMeta::write_by_name(type_name, instance_ptr) } };
        }

        template <typename T>
        static T*& read(const Json& json_context, ReflectionPtr<T>& instance) {
            DO_ASSERT(json_context.is_object(), "Serializer::read expects object");
            DO_ASSERT(json_context.contains("$typeName"), "Serializer::read missing $typeName");
            std::string type_name = json_context.at("$typeName").get<std::string>();
            instance.set_type_name(type_name);
            return read_pointer(json_context, instance.get_ptr_reference());
        }

        template <typename T>
        static Json write(const T& instance) {

            if constexpr (std::is_pointer<T>::value) {
                return write_pointer((T)instance);
            }
            else if constexpr (std::is_enum_v<T>) {
                return Json(static_cast<std::underlying_type_t<T>>(instance));
            }
            else {
                static_assert(always_false<T>, "Serializer::write<T> has not been implemented yet!");
                return Json();
            }
        }

        template <typename T>
        static T& read(const Json& json_context, T& instance) {
            if constexpr (std::is_pointer<T>::value) {
                return read_pointer(json_context, instance);
            }
            else if constexpr (std::is_enum_v<T>) {
                using Underlying = std::underlying_type_t<T>;
                Underlying value{};
                read(json_context, value);
                instance = static_cast<T>(value);
                return instance;
            }
            else {
                static_assert(always_false<T>, "Serializer::read<T> has not been implemented yet!");
                return instance;
            }
        }
    };

    // implementation of base types
    template<>
    Json Serializer::write(const char& instance);
    template<>
    char& Serializer::read(const Json& json_context, char& instance);

    template<>
    Json Serializer::write(const int& instance);
    template<>
    int& Serializer::read(const Json& json_context, int& instance);

    template<>
    Json Serializer::write(const unsigned int& instance);
    template<>
    unsigned int& Serializer::read(const Json& json_context, unsigned int& instance);

    template<>
    Json Serializer::write(const size_t& instance);
    template<>
    size_t& Serializer::read(const Json& json_context, size_t& instance);

    template<>
    Json Serializer::write(const float& instance);
    template<>
    float& Serializer::read(const Json& json_context, float& instance);

    template<>
    Json Serializer::write(const double& instance);
    template<>
    double& Serializer::read(const Json& json_context, double& instance);

    template<>
    Json Serializer::write(const bool& instance);
    template<>
    bool& Serializer::read(const Json& json_context, bool& instance);

    template<>
    Json Serializer::write(const std::string& instance);
    template<>
    std::string& Serializer::read(const Json& json_context, std::string& instance);

    template<>
    Json Serializer::write(const Uuid& instance);
    template<>
    Uuid& Serializer::read(const Json& json_context, Uuid& instance);

    template<>
    Json Serializer::write(const Vector2f& instance);
    template<>
    Vector2f& Serializer::read(const Json& json_context, Vector2f& instance);

    template<>
    Json Serializer::write(const Vector3f& instance);
    template<>
    Vector3f& Serializer::read(const Json& json_context, Vector3f& instance);

    template<>
    Json Serializer::write(const Vector4f& instance);
    template<>
    Vector4f& Serializer::read(const Json& json_context, Vector4f& instance);

    template<>
    Json Serializer::write(const Color& instance);
    template<>
    Color& Serializer::read(const Json& json_context, Color& instance);

} // dodoe

#endif // DODOE_SERIALIZER_H
