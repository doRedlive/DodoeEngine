// do@Redlive

#pragma once

#include "dopch.h"

#include <typeindex>

namespace dodoe {

    class TypedKeyValueStore {
    private:
        struct Holder {
            struct Base {
                std::atomic<Size_t> refs{1};
                virtual ~Base() = default;
                virtual void* ptr() = 0;
                virtual void destroy() noexcept = 0;
            };

            template <typename T>
            struct Impl : Base {
                T value;
                explicit Impl(const T& v) : value(v) {}
                void* ptr() override { return &value; }
                void destroy() noexcept override {
                    this->~Impl();
                    Memory::DeallocatePersistent(this, sizeof(Impl), AllocTag::Object);
                }
            };

            Base* ctrl{nullptr};

            Holder() = default;
            explicit Holder(Base* b) : ctrl(b) {}
            ~Holder() { release(); }

            Holder(const Holder& other) : ctrl(other.ctrl) {
                if (ctrl) ctrl->refs.fetch_add(1, std::memory_order_relaxed);
            }
            Holder(Holder&& other) noexcept : ctrl(other.ctrl) { other.ctrl = nullptr; }

            Holder& operator=(const Holder& other) {
                if (this != &other) {
                    release();
                    ctrl = other.ctrl;
                    if (ctrl) ctrl->refs.fetch_add(1, std::memory_order_relaxed);
                }
                return *this;
            }
            Holder& operator=(Holder&& other) noexcept {
                if (this != &other) { release(); ctrl = other.ctrl; other.ctrl = nullptr; }
                return *this;
            }

            void* get() const { return ctrl ? ctrl->ptr() : nullptr; }
            explicit operator Bool() const { return ctrl != nullptr; }

        private:
            void release() {
                Base* released = std::exchange(ctrl, nullptr);
                if (released && released->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    released->destroy();
                }
            }
        };

        UnorderedMap<std::type_index, Holder> m_values{};

    public:
        template <typename TKey, typename TValue>
        void set(const TValue& value) {
            using ImplType = typename Holder::template Impl<TValue>;
            void* mem = Memory::AllocatePersistent(
                sizeof(ImplType), alignof(ImplType), AllocTag::Object);
            m_values[std::type_index(typeid(TKey))] =
                Holder(new (mem) ImplType(value));
        }

        template <typename TKey, typename TValue>
        [[nodiscard]] TValue* get() {
            auto it = m_values.find(std::type_index(typeid(TKey)));
            return it != m_values.end() ? static_cast<TValue*>(it->second.get()) : nullptr;
        }

        template <typename TKey, typename TValue>
        [[nodiscard]] const TValue* get() const {
            auto it = m_values.find(std::type_index(typeid(TKey)));
            return it != m_values.end() ? static_cast<const TValue*>(it->second.get()) : nullptr;
        }

        void clear() { m_values.clear(); }
    };

} // namespace dodoe
