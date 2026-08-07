#include "common/precompiled.h"

#include "generator/generator.h"
#include "language_types/class.h"
#include "language_types/enum_def.h"

namespace Generator
{
    void GeneratorInterface::prepareStatus(std::string path)
    {
        if (!fs::exists(path))
        {
            fs::create_directories(path);
        }
    }
    void GeneratorInterface::genClassRenderData(std::shared_ptr<Class> class_temp, Mustache::data& class_def)
    {
        class_def.set("class_name", class_temp->getClassName());
        class_def.set("class_base_class_size", std::to_string(class_temp->m_base_classes.size()));
        class_def.set("class_need_register", true);

        if (class_temp->m_base_classes.size() > 0)
        {
            Mustache::data class_base_class_defines(Mustache::data::type::list);
            class_def.set("class_has_base", true);
            for (int index = 0; index < class_temp->m_base_classes.size(); ++index)
            {
                Mustache::data class_base_class_def;
                class_base_class_def.set("class_base_class_name", class_temp->m_base_classes[index]->name);
                class_base_class_def.set("class_base_class_index", std::to_string(index));
                class_base_class_defines.push_back(class_base_class_def);
            }
            class_def.set("class_base_class_defines", class_base_class_defines);
        }

        Mustache::data class_field_defines = Mustache::data::type::list;
        genClassFieldRenderData(class_temp, class_field_defines);
        class_def.set("class_field_defines", class_field_defines);

        
        Mustache::data class_method_defines = Mustache::data::type::list;
        genClassMethodRenderData(class_temp, class_method_defines);
        class_def.set("class_method_defines", class_method_defines);
    }
    static std::string mapFieldTypeToken(const std::string& type_name)
    {
        static const std::map<std::string, std::string> scalar_map = {
            {"bool", "Bool"},
            {"Bool", "Bool"},
            {"int", "I32"},
            {"int32_t", "I32"},
            {"Int32", "I32"},
            {"uint32_t", "U32"},
            {"UInt32", "U32"},
            {"float", "F32"},
            {"Float", "F32"},
            {"double", "F64"},
            {"Double", "F64"},
            {"String", "String"},
            {"std::string", "String"},
            {"Vector2f", "Vec2"},
            {"Vector2i", "Vec2i"},
            {"Vector3f", "Vec3"},
            {"Vector3i", "Vec3i"},
            {"Vector4f", "Vec4"},
            {"Vector4i", "Vec4i"},
            {"Color", "Color"},
        };

        if (type_name.rfind("std::vector<", 0) == 0)
        {
            return "Array";
        }
        if (type_name.rfind("PPtr<", 0) == 0)
        {
            return "Ptr";
        }
        if (g_enum_table.find(type_name) != g_enum_table.end())
        {
            return "Enum";
        }
        auto it = scalar_map.find(type_name);
        if (it != scalar_map.end())
        {
            return it->second;
        }
        return "Struct";
    }

    static std::string escapeStringLiteral(const std::string& input)
    {
        std::string out;
        out.reserve(input.size());
        for (char c : input)
        {
            if (c == '\\' || c == '"')
            {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    }

    void GeneratorInterface::genClassFieldRenderData(std::shared_ptr<Class> class_temp, Mustache::data& feild_defs)
    {
        static const std::string vector_prefix = "std::vector<";

        for (auto& field : class_temp->m_fields)
        {
            if (!field->shouldCompile())
                continue;
            Mustache::data filed_define;

            filed_define.set("class_field_name", field->m_name);
            filed_define.set("class_field_type", field->m_type);
            filed_define.set("class_field_display_name", field->m_display_name);
            bool is_vector = field->m_type.find(vector_prefix) == 0;
            filed_define.set("class_field_is_vector", is_vector);
            filed_define.set("class_field_type_token", mapFieldTypeToken(field->m_type));

            Mustache::data attr_defines(Mustache::data::type::list);
            for (const auto& [key, value] : field->getMetaData().getProperties())
            {
                Mustache::data attr_def;
                attr_def.set("attr_key", key);
                attr_def.set("attr_value", escapeStringLiteral(value));
                attr_defines.push_back(attr_def);
            }
            filed_define.set("class_field_attr_defines", attr_defines);

            feild_defs.push_back(filed_define);
        }
    }

    void GeneratorInterface::genClassMethodRenderData(std::shared_ptr<Class> class_temp, Mustache::data& method_defs)
    {
       for (auto& method : class_temp->m_methods)
        {
            if (!method->shouldCompile())
                continue;
            Mustache::data method_define;

            method_define.set("class_method_name", method->m_name);   
            method_defs.push_back(method_define);
        }
    }
} // namespace Generator
