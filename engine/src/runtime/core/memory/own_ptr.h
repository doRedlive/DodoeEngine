// do@Redlive

#pragma once

#include "runtime/core/memory/memory.h"

#include <cstddef>
#include <new>
#include <utility>

namespace dodoe {

    // OwnPtr: 独占所有权指针，语义同 std::unique_ptr。
    //
    // 分配走引擎 Memory（create_own_ptr 直接 AllocatePersistent，入内存统计）；
    // 销毁用 delete 表达式（释放 + 析构一步完成）。delete 对不完整类型也能编译
    // 通过（不要求 sizeof / 可见析构），因此 OwnPtr 可以持有不完整类型完成 pimpl
    // （头文件 forward-declare，.cpp 完整定义）。真正销毁发生在 T 完整的编译单元，
    // 那里 delete 会解析到 T 重载的 sized operator delete。
    //
    // 被 OwnPtr 持有的类型（如 physics_world.cpp 的 PhysicsWorld::Impl）应定义
    // sized operator delete 并把释放路由回 Memory::DeallocatePersistent，
    // 否则 delete 默认落全局 ::operator delete，逃过引擎内存统计。
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
