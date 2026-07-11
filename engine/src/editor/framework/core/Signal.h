// do@Redlive

#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace cakery {

template <typename... Args>
class Signal {
public:
    using Slot   = std::function<void(Args...)>;
    using Handle = std::uint64_t;

    Handle connect(Slot slot) {
        Handle h = ++m_next;
        m_slots.push_back({h, std::move(slot)});
        return h;
    }

    void disconnect(Handle h) {
        m_slots.erase(
            std::remove_if(m_slots.begin(), m_slots.end(),
                          [h](const auto& e) { return e.handle == h; }),
            m_slots.end());
    }

    void fire(Args... args) const {
        auto snapshot = m_slots;
        for (auto& e : snapshot) e.slot(args...);
    }

private:
    struct Entry { Handle handle; Slot slot; };
    mutable std::vector<Entry> m_slots;
    Handle m_next{0};
};

class ScopedConnection {
public:
    ScopedConnection() = default;

    template <typename S>
    ScopedConnection(S& sig, typename S::Handle h)
        : m_disconnect([&sig, h] { sig.disconnect(h); }) {}

    ~ScopedConnection() { if (m_disconnect) m_disconnect(); }

    ScopedConnection(ScopedConnection&&) noexcept = default;
    ScopedConnection& operator=(ScopedConnection&&) noexcept = default;
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

private:
    std::function<void()> m_disconnect;
};

} // namespace cakery
