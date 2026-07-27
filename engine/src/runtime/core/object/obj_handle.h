// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"

namespace dodoe {

    template <typename T>
    class ObjHandle {
        InstanceID m_id{0};

        void acquire(Object* obj) {
            if (obj) {
                obj->addRef();
            }
        }

        void release(Object* obj) {
            if (obj) {
                obj->releaseRef();
            }
        }

    public:
        ObjHandle() = default;

        explicit ObjHandle(T* obj) : m_id(obj ? obj->getInstanceID() : 0) {
            acquire(obj);
        }

        ObjHandle(const ObjHandle& other) : m_id(other.m_id) {
            if (m_id != 0) {
                auto* obj = Object::FindObjectFromInstanceID(m_id);
                acquire(obj);
            }
        }

        ObjHandle(ObjHandle&& other) noexcept : m_id(other.m_id) {
            other.m_id = 0;
        }

        ~ObjHandle() {
            if (m_id != 0) {
                auto* obj = Object::FindObjectFromInstanceID(m_id);
                release(obj);
            }
        }

        ObjHandle& operator=(const ObjHandle& other) {
            if (this != &other) {
                if (m_id != 0) {
                    auto* oldObj = Object::FindObjectFromInstanceID(m_id);
                    release(oldObj);
                }
                m_id = other.m_id;
                if (m_id != 0) {
                    auto* newObj = Object::FindObjectFromInstanceID(m_id);
                    acquire(newObj);
                }
            }
            return *this;
        }

        ObjHandle& operator=(ObjHandle&& other) noexcept {
            if (this != &other) {
                if (m_id != 0) {
                    auto* oldObj = Object::FindObjectFromInstanceID(m_id);
                    release(oldObj);
                }
                m_id = other.m_id;
                other.m_id = 0;
            }
            return *this;
        }

        [[nodiscard]] T* get() const {
            if (m_id == 0) return nullptr;
            auto* obj = Object::FindObjectFromInstanceID(m_id);
            if (!obj) return nullptr;
            return reinterpret_cast<T*>(obj);
        }

        T* operator->() const { return get(); }
        T& operator*() const { return *get(); }
        explicit operator Bool() const { return get() != nullptr; }
        [[nodiscard]] InstanceID getInstanceID() const { return m_id; }
    };

} // namespace dodoe
