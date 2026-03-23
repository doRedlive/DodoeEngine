//
// Created by Redlive on 2026/3/20.
//

#include "serializer.h"

namespace dodoe {

    template <>
    Json Serializer::write(const char& instance) {
        return Json(instance);
    }
    template <>
    char& Serializer::read(const Json& json_context, char& instance) {
        DoAssert(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<char> expects integer");
        return instance = static_cast<char>(json_context.get<int>());
    }

    template <>
    Json Serializer::write(const int& instance) {
        return Json(instance);
    }
    template <>
    int& Serializer::read(const Json& json_context, int& instance) {
        DoAssert(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<int> expects integer");
        return instance = json_context.get<int>();
    }

    template <>
    Json Serializer::write(const unsigned int& instance) {
        return Json(instance);
    }
    template <>
    unsigned int& Serializer::read(const Json& json_context, unsigned int& instance) {
        DoAssert(json_context.is_number_integer() || json_context.is_number_unsigned(),
                 "Serializer::read<unsigned int> expects integer");
        const auto value = json_context.get<long long>();
        DoAssert(value >= 0, "Serializer::read<unsigned int> expects non-negative integer");
        return instance = static_cast<unsigned int>(value);
    }

    template <>
    Json Serializer::write(const float& instance) {
        return Json(instance);
    }
    template <>
    float& Serializer::read(const Json& json_context, float& instance) {
        DoAssert(json_context.is_number(), "Serializer::read<float> expects number");
        return instance = json_context.get<float>();
    }

    template <>
    Json Serializer::write(const double& instance) {
        return Json(instance);
    }
    template <>
    double& Serializer::read(const Json& json_context, double& instance) {
        DoAssert(json_context.is_number(), "Serializer::read<double> expects number");
        return instance = json_context.get<double>();
    }

    template <>
    Json Serializer::write(const bool& instance) {
        return Json(instance);
    }
    template <>
    bool& Serializer::read(const Json& json_context, bool& instance) {
        DoAssert(json_context.is_boolean(), "Serializer::read<bool> expects boolean");
        return instance = json_context.get<bool>();
    }

    template <>
    Json Serializer::write(const std::string& instance) {
        return Json(instance);
    }
    template <>
    std::string& Serializer::read(const Json& json_context, std::string& instance) {
        DoAssert(json_context.is_string(), "Serializer::read<std::string> expects string");
        return instance = json_context.get_ref<const std::string&>();
    }

} // dodoe
