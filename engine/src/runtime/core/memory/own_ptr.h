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

    public:
        OwnPtr() = default;
        explicit OwnPtr(T* ptr) : m_ptr(ptr) {}
        OwnPtr(std::nullptr_t) : m_ptr(nullptr) {}
        ~OwnPtr() { if (m_ptr) delete m_ptr; }

        OwnPtr(const OwnPtr&) = delete;
        OwnPtr& operator=(const OwnPtr&) = delete;

        OwnPtr(OwnPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
        OwnPtr& operator=(OwnPtr&& other) noexcept {
            if (this != &other) {
                if (m_ptr) delete m_ptr;
                m_ptr = other.m_ptr;
                other.m_ptr = nullptr;
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
                if (m_ptr) delete m_ptr;
                m_ptr = ptr;
            }
        }
        [[nodiscard]] T* release() { T* p = m_ptr; m_ptr = nullptr; return p; }
        void swap(OwnPtr& other) noexcept { std::swap(m_ptr, other.m_ptr); }
    };

    template <typename T, typename... Args>
    OwnPtr<T> create_own_ptr(Args&&... args) {
        void* memory = Memory::AllocatePersistent(sizeof(T), alignof(T), AllocTag::Object);
        return OwnPtr<T>(new (memory) T(std::forward<Args>(args)...));
    }

} // namespace dodoe
