// do@Redlive

#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_float4x4.hpp"

#include "entt/entt.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace dodoe {

    enum class AllocCategory : UInt8;
    class Memory;

    template <typename T>
    class Scope {
        T* m_ptr{nullptr};
    public:
        Scope() = default;
        explicit Scope(T* ptr) : m_ptr(ptr) {}
        ~Scope() { destroy(); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        Scope(Scope&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
        Scope& operator=(Scope&& other) noexcept {
            if (this != &other) { destroy(); m_ptr = other.m_ptr; other.m_ptr = nullptr; }
            return *this;
        }

        T* get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }
        T& operator*() const { return *m_ptr; }
        explicit operator Bool() const { return m_ptr != nullptr; }

        void reset(T* ptr = nullptr) {
            if (m_ptr != ptr) { destroy(); m_ptr = ptr; }
        }
        T* release() { T* p = m_ptr; m_ptr = nullptr; return p; }
        void swap(Scope& other) { std::swap(m_ptr, other.m_ptr); }

    private:
        void destroy() {
            if (m_ptr) {
                m_ptr->~T();
                Memory::Deallocate(m_ptr, sizeof(T), AllocCategory::Misc);
                m_ptr = nullptr;
            }
        }
    };

    template <typename T, typename... Args>
    Scope<T> create_scope(Args&&... args) {
        void* mem = Memory::Allocate(sizeof(T), alignof(T), AllocCategory::Object);
        return Scope<T>(new (mem) T(std::forward<Args>(args)...));
    }

    template <typename T>
    class Weak;

    template <typename T>
    class Ref {
        struct ControlBlock {
            std::atomic<Size_t> strong{1};
            std::atomic<Size_t> weak{1};
            T obj;

            template <typename... Args>
            explicit ControlBlock(Args&&... args) : obj(std::forward<Args>(args)...) {}
        };
        ControlBlock* m_ctrl{nullptr};

        explicit Ref(ControlBlock* cb) : m_ctrl(cb) {}
        friend class Weak<T>;

    public:
        Ref() = default;
        ~Ref() { release(); }

        Ref(const Ref& other) : m_ctrl(other.m_ctrl) {
            if (m_ctrl) m_ctrl->strong.fetch_add(1, std::memory_order_relaxed);
        }
        Ref(Ref&& other) noexcept : m_ctrl(other.m_ctrl) { other.m_ctrl = nullptr; }

        Ref& operator=(const Ref& other) {
            if (this != &other) {
                release();
                m_ctrl = other.m_ctrl;
                if (m_ctrl) m_ctrl->strong.fetch_add(1, std::memory_order_relaxed);
            }
            return *this;
        }
        Ref& operator=(Ref&& other) noexcept {
            if (this != &other) { release(); m_ctrl = other.m_ctrl; other.m_ctrl = nullptr; }
            return *this;
        }

        T* get() const { return m_ctrl ? &m_ctrl->obj : nullptr; }
        T* operator->() const { return get(); }
        T& operator*() const { return *get(); }
        explicit operator Bool() const { return m_ctrl != nullptr; }
        Size_t use_count() const {
            return m_ctrl ? m_ctrl->strong.load(std::memory_order_relaxed) : 0;
        }
        void reset() { release(); m_ctrl = nullptr; }

    private:
        void release() {
            if (!m_ctrl) return;
            if (m_ctrl->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                m_ctrl->obj.~T();
                m_ctrl->strong.store(0, std::memory_order_relaxed);
                if (m_ctrl->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    Memory::Deallocate(m_ctrl, sizeof(ControlBlock), AllocCategory::Object);
                }
            }
        }
    };

    template <typename T, typename... Args>
    Ref<T> create_ref(Args&&... args) {
        void* mem = Memory::Allocate(sizeof(typename Ref<T>::ControlBlock),
                                      alignof(typename Ref<T>::ControlBlock),
                                      AllocCategory::Object);
        auto* cb = new (mem) typename Ref<T>::ControlBlock(std::forward<Args>(args)...);
        return Ref<T>(cb);
    }

    template <typename T>
    class Weak {
        using ControlBlock = typename Ref<T>::ControlBlock;
        ControlBlock* m_ctrl{nullptr};
        friend class Ref<T>;

    public:
        Weak() = default;
        ~Weak() { decWeak(); }

        Weak(const Weak& other) : m_ctrl(other.m_ctrl) {
            if (m_ctrl) m_ctrl->weak.fetch_add(1, std::memory_order_relaxed);
        }
        Weak(const Ref<T>& ref) : m_ctrl(ref.m_ctrl) {
            if (m_ctrl) m_ctrl->weak.fetch_add(1, std::memory_order_relaxed);
        }
        Weak(Weak&& other) noexcept : m_ctrl(other.m_ctrl) { other.m_ctrl = nullptr; }

        Weak& operator=(const Weak& other) {
            if (this != &other) { decWeak(); m_ctrl = other.m_ctrl; incWeak(); }
            return *this;
        }
        Weak& operator=(Weak&& other) noexcept {
            if (this != &other) { decWeak(); m_ctrl = other.m_ctrl; other.m_ctrl = nullptr; }
            return *this;
        }

        Ref<T> lock() const {
            if (!m_ctrl) return Ref<T>();
            Size_t expected = m_ctrl->strong.load(std::memory_order_relaxed);
            while (expected > 0) {
                if (m_ctrl->strong.compare_exchange_weak(expected, expected + 1,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    Ref<T> result;
                    result.m_ctrl = m_ctrl;
                    return result;
                }
            }
            return Ref<T>();
        }

        Bool expired() const {
            return !m_ctrl || m_ctrl->strong.load(std::memory_order_relaxed) == 0;
        }

    private:
        void incWeak() { if (m_ctrl) m_ctrl->weak.fetch_add(1, std::memory_order_relaxed); }
        void decWeak() {
            if (m_ctrl && m_ctrl->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                Memory::Deallocate(m_ctrl, sizeof(ControlBlock), AllocCategory::Object);
            }
        }
    };

    using uint     = unsigned int;
    using uchar    = unsigned char;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using ui8  = uint8_t;
    using ui16 = uint16_t;
    using ui32 = uint32_t;
    using ui64 = uint64_t;

    using Int = int;
    using UInt = unsigned int;
    using Float = float;
    using Byte = char;
    using Char = char;
    using UByte = unsigned char;
    using UChar = unsigned char;
    using Bool = bool;
    using Size_t = size_t;

    using Int8 = int8_t;
    using Int16 = int16_t;
    using Int32 = int32_t;
    using Int64 = int64_t;
    using UInt8  = uint8_t;
    using UInt16 = uint16_t;
    using UInt32 = uint32_t;
    using UInt64 = uint64_t;

    using String = std::string;
    using StringView = std::string_view;

    template <typename T>
    using DynamicArray = std::vector<T>;

    template <typename T, size_t N>
    using StaticArray = std::array<T, N>;

    template <typename TKey, typename TValue, typename THash = std::hash<TKey>, typename TEqual = std::equal_to<TKey>>
    using UnorderedMap = std::unordered_map<TKey, TValue, THash, TEqual>;

    template <typename T, typename THash = std::hash<T>, typename TEqual = std::equal_to<T>>
    using UnorderedSet = std::unordered_set<T, THash, TEqual>;

    template <typename TKey, typename TValue>
    using Dictionary = UnorderedMap<TKey, TValue> ;

    template <typename T1, typename T2>
    using Pair = std::pair<T1, T2>;


    using Vector2f = glm::vec2;
    using Vector2i = glm::ivec2;
    using Vector3f = glm::vec3;
    using Vector3i = glm::ivec3;
    using Vector4f = glm::vec4;
    using Vector4i = glm::ivec4;
    using Matrix3f = glm::mat3;
    using Matrix4f = glm::mat4;

    using identifier = entt::id_type;
    using Identifier = entt::id_type;

    using InstanceID = Int32;

} // dodoe
