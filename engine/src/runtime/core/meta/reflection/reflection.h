#ifndef DODOE_REFLECTION_HPP
#define DODOE_REFLECTION_HPP

#include "dopch.h"

#include <type_traits>

#include "runtime/core/meta/json.h"

namespace dodoe {

#if defined(__REFLECTION_PARSER__)
#define META(...) __attribute__((annotate(#__VA_ARGS__)))
#define CLASS(class_name, ...) class __attribute__((annotate(#__VA_ARGS__))) class_name
#define STRUCT(struct_name, ...) struct __attribute__((annotate(#__VA_ARGS__))) struct_name
#else
#define META(...)
#define CLASS(class_name, ...) class class_name
#define STRUCT(struct_name, ...) struct struct_name
#endif

#define REFLECTION_BODY(class_name) \
    friend class dodoe::reflection::TypeFieldReflectionOperator::Type##class_name##Operator; \
    friend class dodoe::Serializer;

#define REFLECTION_TYPE(class_name) \
    namespace dodoe::reflection { \
        namespace TypeFieldReflectionOperator { \
            class Type##class_name##Operator; \
        } \
    }

#define REGISTER_FIELD_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_field_map(name, value)
#define REGISTER_METHOD_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_method_map(name, value)
#define REGISTER_BASE_CLASS_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_class_map(name, value)
#define REGISTER_ARRAY_TO_MAP(name, value) dodoe::reflection::TypeMetaRegisterInterface::register_to_array_map(name, value)
#define UNREGISTER_ALL dodoe::reflection::TypeMetaRegisterInterface::unregister_all()

    template<typename T, typename U, typename = void>
    struct is_safely_castable : std::false_type
    {};

    template<typename T, typename U>
    struct is_safely_castable<T, U, std::void_t<decltype(static_cast<U>(std::declval<T>()))>> : std::true_type
    {};

    class Serializer;

    namespace reflection {
        class TypeMeta;
        class FieldAccessor;
        class MethodAccessor;
        class ArrayAccessor;
        class ReflectionInstance;

        using SetFunc = std::function<void(void*, void*)>;
        using GetFunc = std::function<void*(void*)>;
        using GetNameFunc = std::function<const char*()>;
        using SetArrayFunc = std::function<void(int, void*, void*)>;
        using GetArrayFunc = std::function<void*(int, void*)>;
        using GetSizeFunc = std::function<int(void*)>;
        using GetBoolFunc = std::function<bool()>;
        using InvokeFunc = std::function<void(void*)>;

        using ConstructorWithJson = std::function<void*(const Json&)>;
        using WriteJsonByName = std::function<Json(void*)>;
        using GetBaseClassReflectionInstanceListFunc = std::function<int(ReflectionInstance*&, void*)>;

        using FieldFuncTuple = std::tuple<SetFunc, GetFunc, GetNameFunc, GetNameFunc, GetNameFunc, GetBoolFunc>;
        using ClassFuncTuple = std::tuple<GetBaseClassReflectionInstanceListFunc, ConstructorWithJson, WriteJsonByName>;
        using ArrayFuncTuple = std::tuple<SetArrayFunc, GetArrayFunc, GetSizeFunc, GetNameFunc, GetNameFunc>;
        using MethodFuncTuple = std::tuple<GetNameFunc, InvokeFunc>;

        class TypeMetaRegisterInterface {
        public:
            static void register_to_class_map(const char* name, ClassFuncTuple* value);
            static void register_to_field_map(const char* name, FieldFuncTuple* value);
            static void register_to_array_map(const char* name, ArrayFuncTuple* value);
            static void register_to_method_map(const char* name, MethodFuncTuple* value);
            static void unregister_all();

            static void register2classmap(const char* name, ClassFuncTuple* value);
            static void register2fieldmap(const char* name, FieldFuncTuple* value);
            static void register2arraymap(const char* name, ArrayFuncTuple* value);
            static void register2methodmap(const char* name, MethodFuncTuple* value);
        };

        class TypeMeta {
            friend class FieldAccessor;
            friend class ArrayAccessor;
            friend class TypeMetaRegisterInterface;
        public:
            TypeMeta();

            static TypeMeta new_meta_from_name(const std::string& name);
            static bool new_array_accessor_from_name(const std::string& array_type_name, ArrayAccessor& accessor);
            static ReflectionInstance new_from_name_and_json(const std::string& type_name, const Json& json_context);
            static Json write_by_name(const std::string& name, void* instance);

            static ReflectionInstance newFromNameAndJson(const std::string& type_name, const Json& json_context);
            static Json writeByName(const std::string& name, void* instance);

            const std::string& get_type_name() const;
            int get_field_list(FieldAccessor*& out_list);
            int get_method_list(MethodAccessor*& out_list);
            int get_base_class_reflection_instance_list(ReflectionInstance*& out_list, void* instance);
            FieldAccessor get_field_by_name(const char* name);
            MethodAccessor get_method_by_name(const char* name);
            bool is_valid() const { return is_valid_; }
            TypeMeta& operator=(const TypeMeta& dest);

        private:
            std::vector<FieldAccessor> fields_;
            std::vector<MethodAccessor> methods_;
            std::string type_name_;
            bool is_valid_{false};

            explicit TypeMeta(const std::string& type_name);
        };

        class FieldAccessor {
            friend class TypeMeta;
        public:
            FieldAccessor();

            void* get(void* instance);
            void set(void* instance, void* value);
            TypeMeta get_owner_type_meta();
            bool get_type_meta(TypeMeta& field_type);
            const char* get_field_name() const;
            const char* get_field_type_name();
            bool is_array_type();
            FieldAccessor& operator=(const FieldAccessor& dest);

        private:
            FieldFuncTuple* functions_;
            const char* field_name_;
            const char* field_type_name_;

            explicit FieldAccessor(FieldFuncTuple* functions);
        };

        class MethodAccessor {
            friend class TypeMeta;
        public:
            MethodAccessor();

            void invoke(void* instance);
            const char* get_method_name() const;
            MethodAccessor& operator=(const MethodAccessor& dest);

        private:
            MethodFuncTuple* functions_;
            const char* method_name_;

            explicit MethodAccessor(MethodFuncTuple* functions);
        };

        class ArrayAccessor {
            friend class TypeMeta;
        public:
            ArrayAccessor();

            const char* get_array_type_name();
            const char* get_element_type_name();
            void set(int index, void* instance, void* element_value);
            void* get(int index, void* instance);
            int get_size(void* instance);
            ArrayAccessor& operator=(ArrayAccessor& dest);

        private:
            ArrayFuncTuple* functions_;
            const char* array_type_name_;
            const char* element_type_name_;

            explicit ArrayAccessor(ArrayFuncTuple* functions);
        };

        class ReflectionInstance {
        public:
            TypeMeta type_meta;
            void* instance;

            ReflectionInstance(TypeMeta meta, void* ins) : type_meta(meta), instance(ins) {}
            ReflectionInstance() : type_meta(), instance(nullptr) {}

            ReflectionInstance& operator=(ReflectionInstance& dest);
            ReflectionInstance& operator=(ReflectionInstance&& dest);
        };
        template<typename T>
        class ReflectionPtr {
            template<typename U>
            friend class ReflectionPtr;

        public:
            ReflectionPtr(std::string type_name, T* instance) : type_name_(std::move(type_name)), instance_(instance) {}
            ReflectionPtr() = default;

            ReflectionPtr(const ReflectionPtr& dest) : type_name_(dest.type_name_), instance_(dest.instance_) {}

            template<typename U>
            ReflectionPtr<T>& operator=(const ReflectionPtr<U>& dest) {
                if (this == static_cast<const void*>(&dest)) {
                    return *this;
                }
                type_name_ = dest.type_name_;
                instance_  = static_cast<T*>(dest.instance_);
                return *this;
            }

            template<typename U>
            ReflectionPtr<T>& operator=(ReflectionPtr<U>&& dest) {
                if (this == static_cast<const void*>(&dest)) {
                    return *this;
                }
                type_name_ = std::move(dest.type_name_);
                instance_  = static_cast<T*>(dest.instance_);
                return *this;
            }

            ReflectionPtr<T>& operator=(const ReflectionPtr<T>& dest) {
                if (this == &dest) {
                    return *this;
                }
                type_name_ = dest.type_name_;
                instance_  = dest.instance_;
                return *this;
            }

            ReflectionPtr<T>& operator=(ReflectionPtr<T>&& dest) {
                if (this == &dest) {
                    return *this;
                }
                type_name_ = std::move(dest.type_name_);
                instance_  = dest.instance_;
                return *this;
            }

            std::string getTypeName() const { return type_name_; }
            void setTypeName(std::string name) { type_name_ = std::move(name); }

            const std::string& get_type_name() const { return type_name_; }
            void set_type_name(std::string name) { type_name_ = std::move(name); }

            bool operator==(const T* ptr) const { return (instance_ == ptr); }
            bool operator!=(const T* ptr) const { return (instance_ != ptr); }
            bool operator==(const ReflectionPtr<T>& rhs_ptr) const { return (instance_ == rhs_ptr.instance_); }
            bool operator!=(const ReflectionPtr<T>& rhs_ptr) const { return (instance_ != rhs_ptr.instance_); }

            template<typename T1>
            explicit operator T1*() {
                return static_cast<T1*>(instance_);
            }

            template<typename T1>
            operator ReflectionPtr<T1>() {
                return ReflectionPtr<T1>(type_name_, static_cast<T1*>(instance_));
            }

            template<typename T1>
            explicit operator const T1*() const {
                return static_cast<const T1*>(instance_);
            }

            template<typename T1>
            operator const ReflectionPtr<T1>() const {
                return ReflectionPtr<T1>(type_name_, static_cast<T1*>(instance_));
            }

            T* operator->() { return instance_; }
            T* operator->() const { return instance_; }
            T& operator*() { return *(instance_); }
            const T& operator*() const { return *(static_cast<const T*>(instance_)); }

            T* getPtr() { return instance_; }
            T* getPtr() const { return instance_; }
            T* get_ptr() { return instance_; }
            T* get_ptr() const { return instance_; }

            T*& getPtrReference() { return instance_; }
            T*& get_ptr_reference() { return instance_; }

            operator bool() const { return (instance_ != nullptr); }

        private:
            std::string type_name_{};
            T*          instance_{nullptr};
        };
    }

    template<typename T>
    using ReflectionPtr = reflection::ReflectionPtr<T>;

} // namespace dodoe

#endif//DODOE_REFLECTION_HPP

