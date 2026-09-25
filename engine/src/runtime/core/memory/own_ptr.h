// do@Redlive

#pragma once

#include "runtime/core/memory/memory.h"

#include <cstddef>
#include <new>
#include <utility>

namespace dodoe {

    template <typename T>
    class OwnPtr {
        T* m_ptr{nullptr};
        void (*m_destroy_fn)(T*){nullptr};

        static void destroyImpl(T* ptr) {
            ptr->~T();
            Memory::DeallocatePersistent(ptr, sizeof(T), AllocTag::Object);
        }

    public:
        OwnPtr() = default;
        explicit OwnPtr(T* ptr) : m_ptr(ptr), m_destroy_fn(&destroyImpl) {}
        OwnPtr(std::nullptr_t) : m_ptr(nullptr) {}
        ~OwnPtr() { destroy(); }

        OwnPtr(const OwnPtr&) = delete;
        OwnPtr& operator=(const OwnPtr&) = delete;

        OwnPtr(OwnPtr&& other) noexcept
            : m_ptr(other.m_ptr), m_destroy_fn(other.m_destroy_fn) {
            other.m_ptr = nullptr;
            other.m_destroy_fn = nullptr;
        }
        OwnPtr& operator=(OwnPtr&& other) noexcept {
            if (this != &other) {
                destroy();
                m_ptr = other.m_ptr;
                m_destroy_fn = other.m_destroy_fn;
                other.m_ptr = nullptr;
                other.m_destroy_fn = nullptr;
            }
            return *this;
        }

        [[nodiscard]] T* get() const { return m_ptr; }
        [[nodiscard]] T* operator->() const { return m_ptr; }
        [[nodiscard]] T& operator*() const { return *m_ptr; }
        explicit operator Bool() const { return m_ptr != nullptr; }
        bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
        bool operator!=(std::nullptr_t) const { return m_ptr != nullptr; }

        void reset(T* ptr = nullptr) {
            if (m_ptr != ptr) {
                destroy();
                m_ptr = ptr;
                m_destroy_fn = ptr ? &destroyImpl : nullptr;
            }
        }
        [[nodiscard]] T* release() {
            T* p = m_ptr;
            m_ptr = nullptr;
            m_destroy_fn = nullptr;
            return p;
        }
        void swap(OwnPtr& other) noexcept {
            std::swap(m_ptr, other.m_ptr);
            std::swap(m_destroy_fn, other.m_destroy_fn);
        }

    private:
        void destroy() {
            if (m_ptr && m_destroy_fn) {
                m_destroy_fn(m_ptr);
            }
            m_ptr = nullptr;
        }
    };

    template <typename T, typename... Args>
    OwnPtr<T> create_own_ptr(Args&&... args) {
        void* memory = Memory::AllocatePersistent(sizeof(T), alignof(T), AllocTag::Object);
        return OwnPtr<T>(new (memory) T(std::forward<Args>(args)...));
    }

} // dodoe
