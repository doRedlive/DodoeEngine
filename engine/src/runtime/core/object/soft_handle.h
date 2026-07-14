// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"
#include "obj_handle.h"

namespace dodoe {

    template <typename T>
    class SoftHandle {
        InstanceID m_id{0};

    public:
        SoftHandle() = default;

        explicit SoftHandle(T* obj) : m_id(obj ? obj->getInstanceID() : 0) {
            if (obj) {
                obj->addWeakRef();
            }
        }

        SoftHandle(const ObjHandle<T>& handle) : m_id(handle.getInstanceID()) {
            if (m_id != 0) {
                auto* obj = Object::FindObjectFromInstanceID(m_id);
                if (obj) obj->addWeakRef();
            }
        }

        SoftHandle(const SoftHandle& other) : m_id(other.m_id) {
            if (m_id != 0) {
                auto* obj = Object::FindObjectFromInstanceID(m_id);
                if (obj) obj->addWeakRef();
            }
        }

        SoftHandle(SoftHandle&& other) noexcept : m_id(other.m_id) {
            other.m_id = 0;
        }

        ~SoftHandle() {
            if (m_id != 0) {
                auto* obj = Object::FindObjectFromInstanceID(m_id);
                if (obj) obj->releaseWeakRef();
            }
        }

        SoftHandle& operator=(const SoftHandle& other) {
            if (this != &other) {
                if (m_id != 0) {
                    auto* oldObj = Object::FindObjectFromInstanceID(m_id);
                    if (oldObj) oldObj->releaseWeakRef();
                }
                m_id = other.m_id;
                if (m_id != 0) {
                    auto* newObj = Object::FindObjectFromInstanceID(m_id);
                    if (newObj) newObj->addWeakRef();
                }
            }
            return *this;
        }

        SoftHandle& operator=(SoftHandle&& other) noexcept {
            if (this != &other) {
                if (m_id != 0) {
                    auto* oldObj = Object::FindObjectFromInstanceID(m_id);
                    if (oldObj) oldObj->releaseWeakRef();
                }
                m_id = other.m_id;
                other.m_id = 0;
            }
            return *this;
        }

        [[nodiscard]] T* Get() const {
            if (m_id == 0) return nullptr;
            auto* obj = Object::FindObjectFromInstanceID(m_id);
            if (!obj || !obj->isAlive()) return nullptr;
            return static_cast<T*>(obj);
        }

        [[nodiscard]] ObjHandle<T> Lock() const {
            if (m_id == 0) return ObjHandle<T>();
            auto* obj = Object::FindObjectFromInstanceID(m_id);
            if (!obj || !obj->isAlive()) return ObjHandle<T>();
            obj->addRef();
            return ObjHandle<T>(static_cast<T*>(obj));
        }

        T* operator->() const { return Get(); }
        explicit operator Bool() const { return Get() != nullptr; }

        [[nodiscard]] Bool Expired() const {
            if (m_id == 0) return true;
            auto* obj = Object::FindObjectFromInstanceID(m_id);
            return !obj || !obj->isAlive();
        }
    };

} // namespace dodoe
