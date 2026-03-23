//
// Created by GreenMuffin on 2026/3/10.
//

#include "reflection.h"

#include <algorithm>
#include <cstring>

namespace dodoe {

    namespace reflection {

        const char* unknownType = "unknownType";
        const char* unknownName = "unknownName";

        static std::map<std::string, ClassFuncTuple*> class_map;
        static std::multimap<std::string, FieldFuncTuple*> field_map;
        static std::multimap<std::string, MethodFuncTuple*> method_map;
        static std::map<std::string, ArrayFuncTuple*> array_map;

        void TypeMetaRegisterInterface::register2classmap(const char* name, ClassFuncTuple* value) {
            if (class_map.find(name) == class_map.end()) {
                class_map.insert(std::make_pair(name, value));
            }
            else {
                delete value;
            }
        }

        void TypeMetaRegisterInterface::register2methodmap(const char* name, MethodFuncTuple* value) {
            method_map.insert(std::make_pair(name, value));
        }

        void TypeMetaRegisterInterface::register2fieldmap(const char* name, FieldFuncTuple* value) {
            field_map.insert(std::make_pair(name, value));
        }

        void TypeMetaRegisterInterface::register2arraymap(const char* name, ArrayFuncTuple* value) {
            if (array_map.find(name) == array_map.end()) {
                array_map.insert(std::make_pair(name, value));
            }
            else {
                delete value;
            }
        }

        void TypeMetaRegisterInterface::unregister_all() {
            for (auto it : class_map) {
                delete it.second;
            }
            class_map.clear();

            for (auto it : field_map) {
                delete it.second;
            }
            field_map.clear();

            for (auto it : method_map) {
                delete it.second;
            }
            method_map.clear();

            for (auto it : array_map) {
                delete it.second;
            }
            array_map.clear();
        }


        TypeMeta::TypeMeta(const std::string& name) : type_name_(name) {
            fields_.clear();
            methods_.clear();

            auto fields_it = field_map.equal_range(type_name_);
            while (fields_it.first != fields_it.second) {
                FieldAccessor field(fields_it.first->second);
                fields_.emplace_back(field);
                is_valid_ = true;

                fields_it.first++;
            }

            auto methods_it = method_map.equal_range(type_name_);
            while (methods_it.first != methods_it.second) {
                MethodAccessor method(methods_it.first->second);
                methods_.emplace_back(method);
                is_valid_ = true;

                methods_it.first++;
            }
        }

        TypeMeta::TypeMeta() : type_name_(unknownType) {
            fields_.clear();
            methods_.clear();
        }

        TypeMeta TypeMeta::new_meta_from_name(const std::string& name) {
            TypeMeta type_meta(name);
            return type_meta;
        }

        bool TypeMeta::new_array_accessor_from_name(const std::string& array_type_name, ArrayAccessor& accessor) {
            auto it = array_map.find(array_type_name);

            if (it != array_map.end()) {
                ArrayAccessor new_accessor(it->second);
                accessor = new_accessor;
                return true;
            }

            return false;
        }

        ReflectionInstance TypeMeta::new_from_name_and_json(const std::string& type_name, const Json& json_context) {
            auto it = class_map.find(type_name);

            if (it != class_map.end()) {
                return ReflectionInstance(TypeMeta(type_name), (std::get<1>(*it->second)(json_context)));
            }

            return ReflectionInstance();
        }

        Json TypeMeta::write_by_name(const std::string& name, void* instance) {
            auto it = class_map.find(name);

            if (it != class_map.end()) {
                return std::get<2>(*it->second)(instance);
            }

            return Json();
        }

        ReflectionInstance TypeMeta::newFromNameAndJson(const std::string& type_name, const Json& json_context) {
            return new_from_name_and_json(type_name, json_context);
        }

        Json TypeMeta::writeByName(const std::string& name, void* instance) {
            return write_by_name(name, instance);
        }

        const std::string& TypeMeta::get_type_name() const {
            return type_name_;
        }

        int TypeMeta::get_field_list(FieldAccessor*& out_list) {
            int count = fields_.size();
            out_list = new FieldAccessor[count];
            for (int i = 0; i < count; i++) {
                out_list[i] = fields_[i];
            }

            return count;
        }

        int TypeMeta::get_method_list(MethodAccessor*& out_list) {
            int count = methods_.size();
            out_list = new MethodAccessor[count];
            for (int i = 0; i < count; i++) {
                out_list[i] = methods_[i];
            }

            return count;
        }

        int TypeMeta::get_base_class_reflection_instance_list(ReflectionInstance*& out_list, void* instance) {
            auto it = class_map.find(type_name_);

            if (it != class_map.end()) {
                return (std::get<0>(*it->second))(out_list, instance);
            }

            return 0;
        }

        FieldAccessor TypeMeta::get_field_by_name(const char* name) {
            const auto it = std::find_if(fields_.begin(), fields_.end(), [&](const auto& i) {
                return std::strcmp(i.get_field_name(), name) == 0;
            });

            if (it != fields_.end()) {
                return *it;
            }

            return FieldAccessor(nullptr);
        }

        MethodAccessor TypeMeta::get_method_by_name(const char* name) {
            const auto it = std::find_if(methods_.begin(), methods_.end(), [&](const auto& i) {
                return std::strcmp(i.get_method_name(), name) == 0;
            });

            if (it != methods_.end()) {
                return *it;
            }

            return MethodAccessor(nullptr);
        }
        
        TypeMeta& TypeMeta::operator=(const TypeMeta& dest) {
            if (this == &dest) {
                return *this;
            }

            fields_.clear();
            fields_ = dest.fields_;

            methods_.clear();
            methods_ = dest.methods_;

            type_name_ = dest.type_name_;
            is_valid_ = dest.is_valid_;

            return *this;
        }

        FieldAccessor::FieldAccessor() : functions_(nullptr), field_name_(unknownName), field_type_name_(unknownType) { }

        FieldAccessor::FieldAccessor(FieldFuncTuple* functions) : functions_(functions), field_name_(unknownName), field_type_name_(unknownType) {
            if (!functions_) return;

            field_name_      = (std::get<3>(*functions))();
            field_type_name_ = (std::get<4>(*functions))();
        }

        void* FieldAccessor::get(void* instance) {
            return static_cast<void*>((std::get<1>(*functions_))(instance));
        }

        void FieldAccessor::set(void* instance, void* value) {
            (std::get<0>(*functions_))(instance, value);
        }

        TypeMeta FieldAccessor::get_owner_type_meta() {
            TypeMeta type_meta((std::get<2>(*functions_))());
            return type_meta;
        }

        bool FieldAccessor::get_type_meta(TypeMeta& field_type) {
            TypeMeta type_meta(field_type_name_);
            field_type = type_meta;
            return field_type.is_valid_;
        }

        const char* FieldAccessor::get_field_name() const {
            return field_name_;
        }

        const char* FieldAccessor::get_field_type_name() {
            return field_type_name_;
        }

        bool FieldAccessor::is_array_type() {
            return (std::get<5>(*functions_)());
        }

        FieldAccessor& FieldAccessor::operator=(const FieldAccessor& dest) {
            if (this == &dest) {
                return *this;
            }

            functions_       = dest.functions_;
            field_name_      = dest.field_name_;
            field_type_name_ = dest.field_type_name_;

            return *this;
        }

        MethodAccessor::MethodAccessor() : method_name_(unknownName), functions_(nullptr) { }

        MethodAccessor::MethodAccessor(MethodFuncTuple* functions) : method_name_(unknownName), functions_(functions) {

            if (!functions_) return;

            method_name_ = (std::get<0>(*functions_))();
        } 

        const char* MethodAccessor::get_method_name() const {
            return (std::get<0>(*functions_))();
        }

        MethodAccessor& MethodAccessor::operator=(const MethodAccessor& dest) {
            if (this == &dest) {
                return *this;
            }

            functions_   = dest.functions_;
            method_name_ = dest.method_name_;

            return *this;
        }

        void MethodAccessor::invoke(void* instance) {
            (std::get<1>(*functions_))(instance);
        }

        ArrayAccessor::ArrayAccessor() : functions_(nullptr), array_type_name_(unknownType), element_type_name_(unknownType) {
        }

        ArrayAccessor::ArrayAccessor(ArrayFuncTuple* functions) : functions_(functions), array_type_name_(unknownType), element_type_name_(unknownType) {
            if (!functions_) return;

            array_type_name_   = std::get<3>(*functions_)();
            element_type_name_ = std::get<4>(*functions_)();
        }

        const char* ArrayAccessor::get_array_type_name() {
            return array_type_name_;
        }

        const char* ArrayAccessor::get_element_type_name() {
            return element_type_name_;
        }

        void ArrayAccessor::set(int index, void* instance, void* element_value) {
            std::get<0>(*functions_)(index, instance, element_value);
        }

        void* ArrayAccessor::get(int index, void* instance) {
            return std::get<1>(*functions_)(index, instance);
        }

        int ArrayAccessor::get_size(void* instance) {
            return std::get<2>(*functions_)(instance);
        }

        ArrayAccessor& ArrayAccessor::operator=(ArrayAccessor& dest) {
            if (this == &dest) {
                return *this;
            }

            functions_         = dest.functions_;
            array_type_name_   = dest.array_type_name_;
            element_type_name_ = dest.element_type_name_;

            return *this;
        }

        ReflectionInstance& ReflectionInstance::operator=(ReflectionInstance& dest) {
            if (this == &dest) {
                return *this;
            }

            instance  = dest.instance;
            type_meta = dest.type_meta;

            return *this;
        }

        ReflectionInstance& ReflectionInstance::operator=(ReflectionInstance&& dest) {
            if (this == &dest) {
                return *this;
            }

            instance  = dest.instance;
            type_meta = dest.type_meta;

            return *this;
        }

    } // namespace reflection
} // namespace dodoe

