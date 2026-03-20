//
// Created by GreenMuffin on 2025/12/12.
//

#ifndef DODOE_REFLECTION_REGISTER_H
#define DODOE_REFLECTION_REGISTER_H

#include "dopch.h"
#include "entt/entt.hpp"

namespace dodoe {
    template<typename class_type>
    class ReflectionRegister {
    public:
        explicit ReflectionRegister(const std::string& class_name) {
            factory_ = entt::meta_factory<class_type>();
            factory_.type(entt::hashed_string{class_name.c_str()}.value());
        }

        template<auto field_ptr>
        ReflectionRegister& field(const std::string& field_name) {
            factory_.template data<field_ptr>(entt::hashed_string{field_name.c_str()}.value()).template custom<std::string>(field_name);
            return *this;
        }

        template<typename func_type, typename... Args>
        ReflectionRegister& method(const std::string& method_name, func_type (class_type::* method_ptr)(Args...)) {
            factory_.func(method_ptr, entt::hashed_string{method_name.c_str()}.value()).template custom<std::string>(method_name);
            return *this;
        }

    private:
        entt::meta_factory<class_type> factory_;
    };

    template<typename class_type>
    ReflectionRegister<class_type> do_reflection_register(const std::string& class_name) {
        return ReflectionRegister<class_type>(class_name);
    }
} // dodoe

#endif //DODOE_REFLECTION_REGISTER_H
