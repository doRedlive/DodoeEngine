// do@GreenMuffin

#include "script_class.h"

#include "script_engine.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "mono/metadata/debug-helpers.h"
#include "mono/metadata/attrdefs.h"
#include "mono/metadata/class.h"
#include "mono/metadata/appdomain.h"
#include "mono/utils/mono-publib.h"

namespace dodoe {

    namespace {

        Json WriteScriptFieldValue(const MonoComponentInstance& instance, const ScriptField& field) {
            switch (field.type) {
                case ScriptFieldType::Float:  return Json(instance.getFieldValue<float>(field.name));
                case ScriptFieldType::Double: return Json(instance.getFieldValue<double>(field.name));
                case ScriptFieldType::Bool:   return Json(instance.getFieldValue<bool>(field.name));
                case ScriptFieldType::Char:   return Json(instance.getFieldValue<char>(field.name));
                case ScriptFieldType::Byte:   return Json(instance.getFieldValue<int8_t>(field.name));
                case ScriptFieldType::Short:  return Json(instance.getFieldValue<int16_t>(field.name));
                case ScriptFieldType::Int:    return Json(instance.getFieldValue<int32_t>(field.name));
                case ScriptFieldType::Long:   return Json(instance.getFieldValue<int64_t>(field.name));
                case ScriptFieldType::UByte:  return Json(instance.getFieldValue<uint8_t>(field.name));
                case ScriptFieldType::UShort: return Json(instance.getFieldValue<uint16_t>(field.name));
                case ScriptFieldType::UInt:   return Json(instance.getFieldValue<uint32_t>(field.name));
                case ScriptFieldType::ULong:  return Json(instance.getFieldValue<uint64_t>(field.name));
                case ScriptFieldType::Vector2:return Serializer::write(instance.getFieldValue<Vector2f>(field.name));
                case ScriptFieldType::Vector3:return Serializer::write(instance.getFieldValue<Vector3f>(field.name));
                case ScriptFieldType::Vector4:return Serializer::write(instance.getFieldValue<Vector4f>(field.name));
                case ScriptFieldType::Entity: {
                    MonoObject* entity_object = nullptr;
                    mono_field_get_value(instance.getManagedInstance(), field.mono_field, &entity_object);
                    if (!entity_object) {
                        return Json(nullptr);
                    }

                    MonoClass* entity_class = mono_object_get_class(entity_object);
                    MonoClassField* id_field = entity_class ? mono_class_get_field_from_name(entity_class, "ID") : nullptr;
                    uint64_t entity_id = 0;
                    if (id_field) {
                        mono_field_get_value(entity_object, id_field, &entity_id);
                    }
                    return Json(entity_id);
                }
                case ScriptFieldType::None:
                default:
                    return Json();
            }
        }

        bool ReadScriptFieldValue(MonoComponentInstance& instance, const ScriptField& field, const Json& json_context) {
            switch (field.type) {
                case ScriptFieldType::Float: {
                    float value = json_context.get<float>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Double: {
                    double value = json_context.get<double>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Bool: {
                    bool value = json_context.get<bool>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Char: {
                    char value = json_context.get<char>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Byte: {
                    int8_t value = json_context.get<int8_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Short: {
                    int16_t value = json_context.get<int16_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Int: {
                    int32_t value = json_context.get<int32_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Long: {
                    int64_t value = json_context.get<int64_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::UByte: {
                    uint8_t value = json_context.get<uint8_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::UShort: {
                    uint16_t value = json_context.get<uint16_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::UInt: {
                    uint32_t value = json_context.get<uint32_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::ULong: {
                    uint64_t value = json_context.get<uint64_t>();
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Vector2: {
                    Vector2f value{};
                    Serializer::read(json_context, value);
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Vector3: {
                    Vector3f value{};
                    Serializer::read(json_context, value);
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Vector4: {
                    Vector4f value{};
                    Serializer::read(json_context, value);
                    instance.setFieldValue(field.name, value);
                    return true;
                }
                case ScriptFieldType::Entity: {
                    MonoObject* entity_object = nullptr;
                    if (!json_context.is_null()) {
                        const uint64_t entity_id = json_context.get<uint64_t>();
                        if (entity_id != 0) {
                            MonoClass* entity_class = mono_class_from_name(instance.getScriptClass()->getEngine()->getCoreImage(), "GreenCake", "Entity");
                            if (!entity_class) {
                                return false;
                            }

                            entity_object = mono_object_new(instance.getScriptClass()->getEngine()->getCoreDomain(), entity_class);
                            if (!entity_object) {
                                return false;
                            }

                            MonoClassField* id_field = mono_class_get_field_from_name(entity_class, "ID");
                            if (!id_field) {
                                return false;
                            }
                            mono_field_set_value(entity_object, id_field, const_cast<uint64_t*>(&entity_id));
                        }
                    }

                    mono_field_set_value(instance.getManagedInstance(), field.mono_field, entity_object);
                    return true;
                }
                case ScriptFieldType::None:
                default:
                    return false;
            }
        }

    } // namespace

    ScriptFieldType MonoType2ScriptFieldType(MonoType* mono_type) {
        const auto mono_type_name = std::string(mono_type_get_name(mono_type));

        if (mono_type_name == "System.Boolean") return ScriptFieldType::Bool;
        if (mono_type_name == "System.SByte") return ScriptFieldType::Byte;
        if (mono_type_name == "System.Int16") return ScriptFieldType::Short;
        if (mono_type_name == "System.Int32") return ScriptFieldType::Int;
        if (mono_type_name == "System.Int64") return ScriptFieldType::Long;
        if (mono_type_name == "System.Byte") return ScriptFieldType::UByte;
        if (mono_type_name == "System.UInt16") return ScriptFieldType::UShort;
        if (mono_type_name == "System.UInt32") return ScriptFieldType::UInt;
        if (mono_type_name == "System.UInt64") return ScriptFieldType::ULong;
        if (mono_type_name == "System.Single") return ScriptFieldType::Float;
        if (mono_type_name == "System.Double") return ScriptFieldType::Double;
        if (mono_type_name == "GreenCake.Vector2f") return ScriptFieldType::Vector2;
        if (mono_type_name == "GreenCake.Vector3f") return ScriptFieldType::Vector3;
        if (mono_type_name == "GreenCake.Entity") return ScriptFieldType::Entity;

        return ScriptFieldType::None;
    }

    ScriptClass::ScriptClass(ScriptEngine* engine, const std::string& class_namespace, const std::string& class_name, const bool is_core) :
        m_engine(engine), m_class_namespace(class_namespace), m_class_name(class_name) {

        m_mono_class = mono_class_from_name(is_core ? m_engine->getCoreImage() : m_engine->getAppImage(),
            m_class_namespace.c_str(), m_class_name.c_str());
        if (!m_mono_class && !is_core) {
            // Some component types (e.g. GreenCake built-ins) live in core image.
            m_mono_class = mono_class_from_name(m_engine->getCoreImage(), m_class_namespace.c_str(), m_class_name.c_str());
        }
        if (!m_mono_class) {
            DO_ERROR("Could not resolve managed class {}.{}.", m_class_namespace, m_class_name);
            return;
        }

        void* iterator = nullptr;
        while (MonoClassField* field = mono_class_get_fields(m_mono_class, &iterator)) {
            const char* field_name = mono_field_get_name(field);
            if (const ui32 flag = mono_field_get_flags(field); flag & MONO_FIELD_ATTR_PUBLIC) {
                MonoType* type = mono_field_get_type(field);
                const ScriptFieldType field_type = MonoType2ScriptFieldType(type);
                m_fields[field_name] = {field_type, field_name, field };
            }
        }
    }

    MonoObject* ScriptClass::instantiate() const {
        if (!m_mono_class) {
            return nullptr;
        }
        MonoObject* instance = mono_object_new(m_engine->getCoreDomain(), m_mono_class);
        if (!instance) {
            return nullptr;
        }

        MonoMethod* ctor = mono_class_get_method_from_name(m_mono_class, ".ctor", 0);
        if (ctor) {
            MonoObject* exception = nullptr;
            mono_runtime_invoke(ctor, instance, nullptr, &exception);
            if (exception) {
                MonoString* exception_string = mono_object_to_string(exception, nullptr);
                char* exception_chars = exception_string ? mono_string_to_utf8(exception_string) : nullptr;
                DO_ERROR("Managed exception in {}.{} constructor: {}",
                    m_class_namespace, m_class_name, exception_chars ? exception_chars : "<unknown>");
                if (exception_chars) {
                    mono_free(exception_chars);
                }
                return nullptr;
            }
        }
        else {
            mono_runtime_object_init(instance);
        }

        return instance;
    }

    MonoMethod* ScriptClass::getMethod(const std::string& name, const int parameter_count) const {
        if (!m_mono_class) {
            return nullptr;
        }
        return mono_class_get_method_from_name(m_mono_class, name.c_str(), parameter_count);
    }

    MonoObject* ScriptClass::invokeMethod(MonoObject* instance, MonoMethod* method, void** params) {
        if (!method) {
            DO_ERROR("Managed method lookup failed for {}.{}.", m_class_namespace, m_class_name);
            return nullptr;
        }

        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, instance, params, &exception);
        if (exception) {
            MonoString* exception_string = mono_object_to_string(exception, nullptr);
            char* exception_chars = exception_string ? mono_string_to_utf8(exception_string) : nullptr;
            DO_ERROR("Managed exception in {}.{}: {}",
                m_class_namespace, m_class_name, exception_chars ? exception_chars : "<unknown>");
            if (exception_chars) {
                mono_free(exception_chars);
            }
        }
        return result;
    }

    MonoComponentInstance::MonoComponentInstance(const Ref<ScriptClass> &script_class) :
        m_script_class(script_class) {

        m_instance = m_script_class->instantiate();
    }

    MonoComponentInstance::MonoComponentInstance(const Ref<ScriptClass>& script_class, MonoObject* instance) :
        m_script_class(script_class), m_instance(instance) {
    }

    Json MonoComponentInstance::serializeFields() const {
        Json json = Json::object();
        if (!m_instance || !m_script_class) {
            return json;
        }

        for (const auto& [field_name, field] : m_script_class->getFields()) {
            json[field_name] = WriteScriptFieldValue(*this, field);
        }

        return json;
    }

    bool MonoComponentInstance::deserializeFields(const Json& json_context) {
        if (!m_instance || !m_script_class || !json_context.is_object()) {
            return false;
        }

        for (const auto& [field_name, field] : m_script_class->getFields()) {
            if (!json_context.contains(field_name)) {
                continue;
            }
            if (!ReadScriptFieldValue(*this, field, json_context.at(field_name))) {
                return false;
            }
        }

        return true;
    }

    bool MonoComponentInstance::getFieldValueInternal(const std::string& name, void* buffer) const {
        if (!m_instance) {
            return false;
        }
        const auto& fields = m_script_class->getFields();
        const auto it = fields.find(name);
        if (it == fields.end())
            return false;

        const ScriptField& field = it->second;
        mono_field_get_value(m_instance, field.mono_field, buffer);
        return true;
    }

    bool MonoComponentInstance::setFieldValueInternal(const std::string& name, const void* value) const {
        if (!m_instance) {
            return false;
        }
        const auto& fields = m_script_class->getFields();
        const auto it = fields.find(name);
        if (it == fields.end())
            return false;

        const ScriptField& field = it->second;
        mono_field_set_value(m_instance, field.mono_field, const_cast<void *>(value));
        return true;
    }

    MonoSystemInstance::MonoSystemInstance(const Ref<ScriptClass>& script_class) :
        m_script_class(script_class) {
        m_instance = m_script_class->instantiate();
        m_constructor = m_script_class->getMethod(".ctor", 0);
        m_start = m_script_class->getMethod("Start", 0);
        m_update = m_script_class->getMethod("Update", 0);
        m_finalize = m_script_class->getMethod("Finalize", 0);
    }

    void MonoSystemInstance::invokeStart() const {
        if (m_start)
            m_script_class->invokeMethod(m_instance, m_start);
    }

    void MonoSystemInstance::invokeUpdate() const {
        if (m_update) 
            m_script_class->invokeMethod(m_instance, m_update);
    }

    void MonoSystemInstance::invokeFinalize() const {
        if (m_finalize)
            m_script_class->invokeMethod(m_instance, m_finalize);
    }

} // dodoe
