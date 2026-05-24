// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    template <typename T, typename CreateInfoT>
    class Managed {
    public:
        static Scope<T> Create(const CreateInfoT& info) {
            auto obj = CreateObject(info);
            if (obj && obj->initialize(info)) { 
                return obj;
            }
            return nullptr;
        }

        static void Destroy(Scope<T>& obj) {
            if (!obj) return;
            obj->shutdown();
            obj.reset();
        }

    private:
        static Scope<T> CreateObject(const CreateInfoT& info) {
            return CreateObjectImpl(info, 0);
        }

        template <typename U = T, std::enable_if_t<std::is_constructible_v<U, const CreateInfoT&>, int> = 0>
        static Scope<U> CreateObjectImpl(const CreateInfoT& info, int) {
            return create_scope<U>(info);
        }

        template <typename U = T, std::enable_if_t<!std::is_constructible_v<U, const CreateInfoT&>, int> = 0>
        static Scope<U> CreateObjectImpl(const CreateInfoT&, long) {
            return create_scope<U>();
        }
    };

} // dodoe
