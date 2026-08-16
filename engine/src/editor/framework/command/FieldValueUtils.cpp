#include "FieldValueUtils.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/object/object_id.h"

#include <cstring>

namespace cakery {

bool IsIntegerJson(const dodoe::Json& value)
{
    return value.is_number_integer() || value.is_number_unsigned();
}

dodoe::Json CaptureFieldValue(const char* typeName, void* value)
{
    if (!typeName || !typeName[0] || !value) return dodoe::Json();

    if (std::strcmp(typeName, "float") == 0 || std::strcmp(typeName, "Float") == 0) {
        return dodoe::Serializer::write(*static_cast<float*>(value));
    }
    if (std::strcmp(typeName, "double") == 0 || std::strcmp(typeName, "Double") == 0) {
        return dodoe::Serializer::write(*static_cast<double*>(value));
    }
    if (std::strcmp(typeName, "int") == 0 || std::strcmp(typeName, "int32_t") == 0
        || std::strcmp(typeName, "Int32") == 0 || std::strcmp(typeName, "Int") == 0) {
        return dodoe::Serializer::write(*static_cast<int*>(value));
    }
    if (std::strcmp(typeName, "unsigned int") == 0 || std::strcmp(typeName, "uint32_t") == 0
        || std::strcmp(typeName, "UInt32") == 0 || std::strcmp(typeName, "UInt") == 0
        || std::strcmp(typeName, "Identifier") == 0) {
        return dodoe::Serializer::write(*static_cast<unsigned int*>(value));
    }
    if (std::strcmp(typeName, "bool") == 0 || std::strcmp(typeName, "Bool") == 0) {
        return dodoe::Serializer::write(*static_cast<bool*>(value));
    }
    if (std::strcmp(typeName, "std::string") == 0) {
        return dodoe::Json(*static_cast<std::string*>(value));
    }
    if (std::strcmp(typeName, "String") == 0 || std::strcmp(typeName, "dodoe::String") == 0) {
        return dodoe::Serializer::write(*static_cast<dodoe::String*>(value));
    }
    if (std::strcmp(typeName, "UUID") == 0) {
        return dodoe::Serializer::write(*static_cast<dodoe::UUID*>(value));
    }
    if (std::strcmp(typeName, "Vector2f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector2f*>(value));
    if (std::strcmp(typeName, "Vector2i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector2i*>(value));
    if (std::strcmp(typeName, "Vector3f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector3f*>(value));
    if (std::strcmp(typeName, "Vector3i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector3i*>(value));
    if (std::strcmp(typeName, "Vector4f") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector4f*>(value));
    if (std::strcmp(typeName, "Vector4i") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Vector4i*>(value));
    if (std::strcmp(typeName, "Color") == 0) return dodoe::Serializer::write(*static_cast<dodoe::Color*>(value));

    if (std::strncmp(typeName, "AssetHandle<", 12) == 0) {
        const auto* id = reinterpret_cast<const dodoe::ObjectID*>(value);
        return dodoe::Json{{"asset_id", dodoe::Serializer::write(id->asset_id)},
                           {"sub_object_id", static_cast<uint32_t>(id->local_id)}};
    }

    return dodoe::TypeMeta::writeByName(typeName, value);
}

bool ApplyFieldValue(const char* typeName, dodoe::FieldAccessor& field, void* objPtr, const dodoe::Json& value)
{
    if (!typeName || !typeName[0] || value.is_null()) return false;

    if (std::strcmp(typeName, "float") == 0 || std::strcmp(typeName, "Float") == 0) {
        if (!value.is_number()) return false;
        float v = value.get<float>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "double") == 0 || std::strcmp(typeName, "Double") == 0) {
        if (!value.is_number()) return false;
        double v = value.get<double>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "int") == 0 || std::strcmp(typeName, "int32_t") == 0
        || std::strcmp(typeName, "Int32") == 0 || std::strcmp(typeName, "Int") == 0) {
        if (!IsIntegerJson(value)) return false;
        int v = value.get<int>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "unsigned int") == 0 || std::strcmp(typeName, "uint32_t") == 0
        || std::strcmp(typeName, "UInt32") == 0 || std::strcmp(typeName, "UInt") == 0
        || std::strcmp(typeName, "Identifier") == 0) {
        if (!IsIntegerJson(value)) return false;
        unsigned int v = value.get<unsigned int>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "bool") == 0 || std::strcmp(typeName, "Bool") == 0) {
        if (!value.is_boolean()) return false;
        bool v = value.get<bool>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "std::string") == 0) {
        if (!value.is_string()) return false;
        std::string v = value.get<std::string>();
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "String") == 0 || std::strcmp(typeName, "dodoe::String") == 0) {
        if (!value.is_string()) return false;
        std::string s = value.get<std::string>();
        dodoe::String v(s.data(), s.size());
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "UUID") == 0) {
        if (!IsIntegerJson(value)) return false;
        dodoe::UUID v(static_cast<uint64_t>(value.get<uint64_t>()));
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector2f") == 0) {
        if (!value.is_array() || value.size() != 2) return false;
        dodoe::Vector2f v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector2i") == 0) {
        if (!value.is_array() || value.size() != 2) return false;
        dodoe::Vector2i v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector3f") == 0) {
        if (!value.is_array() || value.size() != 3) return false;
        dodoe::Vector3f v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector3i") == 0) {
        if (!value.is_array() || value.size() != 3) return false;
        dodoe::Vector3i v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector4f") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Vector4f v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Vector4i") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Vector4i v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }
    if (std::strcmp(typeName, "Color") == 0) {
        if (!value.is_array() || value.size() != 4) return false;
        dodoe::Color v;
        dodoe::Serializer::read(value, v);
        field.set(objPtr, &v);
        return true;
    }

    if (std::strncmp(typeName, "AssetHandle<", 12) == 0) {
        if (!value.is_object()) return false;
        dodoe::ObjectID id;
        if (value.contains("asset_id") && value["asset_id"].is_number_unsigned()) {
            id.asset_id = dodoe::UUID(static_cast<uint64_t>(value["asset_id"].get<uint64_t>()));
        }
        if (value.contains("sub_object_id") && value["sub_object_id"].is_number_unsigned()) {
            id.local_id = static_cast<uint32_t>(value["sub_object_id"].get<uint32_t>());
        }
        field.set(objPtr, &id);
        return true;
    }

    dodoe::ReflectionInstance inst = dodoe::TypeMeta::newFromNameAndJson(typeName, value);
    if (!inst.instance) return false;
    field.set(objPtr, inst.instance);
    delete inst.instance;
    return true;
}

} // namespace cakery
