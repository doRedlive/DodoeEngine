// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    template <UInt32 GenBits = 8>
    struct SlotHandle {
        UInt32 raw{~0u};

        Bool valid() const { return raw != ~0u; }
        UInt32 index() const { return raw & ((1u << (32 - GenBits)) - 1); }
        UInt32 generation() const { return raw >> (32 - GenBits); }

        Bool operator==(const SlotHandle& o) const { return raw == o.raw; }
        Bool operator!=(const SlotHandle& o) const { return raw != o.raw; }
    };

    template <typename T, UInt32 GenBits = 8>
    class SlotMap {
        static constexpr UInt32 kIndexBits = 32 - GenBits;
        static constexpr UInt32 kIndexMask = (1u << kIndexBits) - 1;
        static constexpr UInt32 kMaxIndex = ~0u >> GenBits;

        struct Slot {
            T value;
            UInt32 generation{0};
            Bool occupied{false};
        };

        DynamicArray<Slot> m_slots{};
        UInt32 m_search_start{0};
        UInt32 m_occupied_count{0};

        SlotHandle<GenBits> makeHandle(UInt32 index, UInt32 generation) const {
            SlotHandle<GenBits> h{};
            h.raw = (generation << kIndexBits) | (index & kIndexMask);
            return h;
        }

    public:
        SlotMap() = default;

        SlotHandle<GenBits> insert(const T& value) {
            for (UInt32 i = m_search_start; i < m_slots.size(); ++i) {
                if (!m_slots[i].occupied) {
                    Slot& slot = m_slots[i];
                    slot.value = value;
                    slot.occupied = true;
                    m_search_start = i + 1;
                    m_occupied_count++;
                    return makeHandle(i, slot.generation);
                }
            }
            m_search_start = kMaxIndex;
            const UInt32 idx = static_cast<UInt32>(m_slots.size());
            m_slots.emplace_back();
            Slot& slot = m_slots.back();
            slot.value = value;
            slot.occupied = true;
            m_occupied_count++;
            return makeHandle(idx, slot.generation);
        }

        SlotHandle<GenBits> insert(T&& value) {
            for (UInt32 i = m_search_start; i < m_slots.size(); ++i) {
                if (!m_slots[i].occupied) {
                    Slot& slot = m_slots[i];
                    slot.value = std::move(value);
                    slot.occupied = true;
                    m_search_start = i + 1;
                    m_occupied_count++;
                    return makeHandle(i, slot.generation);
                }
            }
            m_search_start = kMaxIndex;
            const UInt32 idx = static_cast<UInt32>(m_slots.size());
            m_slots.emplace_back();
            Slot& slot = m_slots.back();
            slot.value = std::move(value);
            slot.occupied = true;
            m_occupied_count++;
            return makeHandle(idx, slot.generation);
        }

        void remove(const SlotHandle<GenBits> handle) {
            if (!handle.valid()) return;
            const UInt32 idx = handle.index();
            if (idx >= m_slots.size()) return;

            Slot& slot = m_slots[idx];
            if (!slot.occupied || slot.generation != handle.generation()) return;

            slot.occupied = false;
            slot.generation++;
            if (slot.generation == 0) slot.generation = 1;
            if (idx < m_search_start) m_search_start = idx;
            m_occupied_count--;
        }

        T* get(const SlotHandle<GenBits> handle) {
            if (!handle.valid()) return nullptr;
            const UInt32 idx = handle.index();
            if (idx >= m_slots.size()) return nullptr;

            Slot& slot = m_slots[idx];
            if (!slot.occupied || slot.generation != handle.generation()) return nullptr;
            return &slot.value;
        }

        const T* get(const SlotHandle<GenBits> handle) const {
            if (!handle.valid()) return nullptr;
            const UInt32 idx = handle.index();
            if (idx >= m_slots.size()) return nullptr;

            const Slot& slot = m_slots[idx];
            if (!slot.occupied || slot.generation != handle.generation()) return nullptr;
            return &slot.value;
        }

        void clear() {
            m_slots.clear();
            m_search_start = 0;
            m_occupied_count = 0;
        }

        UInt32 slotCount() const { return static_cast<UInt32>(m_slots.size()); }
        UInt32 occupiedCount() const { return m_occupied_count; }

        template <typename F>
        void forEachOccupied(F&& func) const {
            for (UInt32 i = 0; i < m_slots.size(); ++i) {
                if (m_slots[i].occupied) {
                    func(makeHandle(i, m_slots[i].generation), m_slots[i].value);
                }
            }
        }
    };

} // dodoe

namespace std {
    template <dodoe::UInt32 GenBits>
    struct hash<dodoe::SlotHandle<GenBits>> {
        size_t operator()(const dodoe::SlotHandle<GenBits>& h) const {
            return std::hash<dodoe::UInt32>{}(h.raw);
        }
    };
}
