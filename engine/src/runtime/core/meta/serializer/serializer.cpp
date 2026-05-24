// do@Redlive

#include "serializer.h"

namespace dodoe {

    template <>
    Json Serializer::write(const char& instance) {
        return Json(instance);
    }
    template <>
    char& Serializer::read(const Json& json_context, char& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<char> expects integer");
        return instance = static_cast<char>(json_context.get<int>());
    }

    template <>
    Json Serializer::write(const int& instance) {
        return Json(instance);
    }
    template <>
    int& Serializer::read(const Json& json_context, int& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<int> expects integer");
        return instance = json_context.get<int>();
    }

    template <>
    Json Serializer::write(const unsigned int& instance) {
        return Json(instance);
    }
    template <>
    unsigned int& Serializer::read(const Json& json_context, unsigned int& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<unsigned int> expects integer");
        const auto value = json_context.get<long long>();
        DO_ASSERT(value >= 0, "Serializer::read<unsigned int> expects non-negative integer");
        return instance = static_cast<unsigned int>(value);
    }

    template <>
    Json Serializer::write(const unsigned short& instance) {
        return Json(static_cast<unsigned int>(instance));
    }
    template <>
    unsigned short& Serializer::read(const Json& json_context, unsigned short& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<unsigned short> expects integer");
        const auto value = json_context.get<long long>();
        DO_ASSERT(value >= 0, "Serializer::read<unsigned short> expects non-negative integer");
        instance = static_cast<unsigned short>(value);
        return instance;
    }

    template <>
    Json Serializer::write(const size_t& instance) {
        return Json(static_cast<uint64_t>(instance));
    }
    template <>
    size_t& Serializer::read(const Json& json_context, size_t& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<size_t> expects integer");
        const auto value = json_context.get<uint64_t>();
        return instance = static_cast<size_t>(value);
    }

    template <>
    Json Serializer::write(const float& instance) {
        return Json(instance);
    }
    template <>
    float& Serializer::read(const Json& json_context, float& instance) {
        DO_ASSERT(json_context.is_number(), "Serializer::read<float> expects number");
        return instance = json_context.get<float>();
    }

    template <>
    Json Serializer::write(const double& instance) {
        return Json(instance);
    }
    template <>
    double& Serializer::read(const Json& json_context, double& instance) {
        DO_ASSERT(json_context.is_number(), "Serializer::read<double> expects number");
        return instance = json_context.get<double>();
    }

    template <>
    Json Serializer::write(const bool& instance) {
        return Json(instance);
    }
    template <>
    bool& Serializer::read(const Json& json_context, bool& instance) {
        DO_ASSERT(json_context.is_boolean(), "Serializer::read<bool> expects boolean");
        return instance = json_context.get<bool>();
    }

    template <>
    Json Serializer::write(const std::string& instance) {
        return Json(instance);
    }
    template <>
    std::string& Serializer::read(const Json& json_context, std::string& instance) {
        DO_ASSERT(json_context.is_string(), "Serializer::read<std::string> expects string");
        return instance = json_context.get_ref<const std::string&>();
    }

    template <>
    Json Serializer::write(const Uuid& instance) {
        return Json(static_cast<uint64_t>(instance));
    }
    template <>
    Uuid& Serializer::read(const Json& json_context, Uuid& instance) {
        DO_ASSERT(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<Uuid> expects integer");
        instance = Uuid(static_cast<uint64_t>(json_context.get<uint64_t>()));
        return instance;
    }

    template <>
    Json Serializer::write(const Vector2f& instance) {
        return Json::array({ instance.x, instance.y });
    }
    template <>
    Vector2f& Serializer::read(const Json& json_context, Vector2f& instance) {
        DO_ASSERT(json_context.is_array() && json_context.size() >= 2,
                 "Serializer::read<Vector2f> expects [x, y]");
        instance.x = json_context.at(0).get<float>();
        instance.y = json_context.at(1).get<float>();
        return instance;
    }

    template <>
    Json Serializer::write(const Vector3f& instance) {
        return Json::array({ instance.x, instance.y, instance.z });
    }
    template <>
    Vector3f& Serializer::read(const Json& json_context, Vector3f& instance) {
        DO_ASSERT(json_context.is_array() && json_context.size() >= 3,
                 "Serializer::read<Vector3f> expects [x, y, z]");
        instance.x = json_context.at(0).get<float>();
        instance.y = json_context.at(1).get<float>();
        instance.z = json_context.at(2).get<float>();
        return instance;
    }

    template <>
    Json Serializer::write(const Vector4f& instance) {
        return Json::array({ instance.x, instance.y, instance.z, instance.w });
    }
    template <>
    Vector4f& Serializer::read(const Json& json_context, Vector4f& instance) {
        DO_ASSERT(json_context.is_array() && json_context.size() >= 4,
                 "Serializer::read<Vector4f> expects [x, y, z, w]");
        instance.x = json_context.at(0).get<float>();
        instance.y = json_context.at(1).get<float>();
        instance.z = json_context.at(2).get<float>();
        instance.w = json_context.at(3).get<float>();
        return instance;
    }

    template <>
    Json Serializer::write(const Color& instance) {
        return Json::array({ instance.r, instance.g, instance.b, instance.a });
    }
    template <>
    Color& Serializer::read(const Json& json_context, Color& instance) {
        DO_ASSERT(json_context.is_array() && json_context.size() >= 4,
                 "Serializer::read<Color> expects [r, g, b, a]");
        instance.r = json_context.at(0).get<float>();
        instance.g = json_context.at(1).get<float>();
        instance.b = json_context.at(2).get<float>();
        instance.a = json_context.at(3).get<float>();
        return instance;
    }

} // dodoe
