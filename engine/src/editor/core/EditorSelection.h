// do@Redlive

#pragma once

#include "core/Signal.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace cakery {

class EditorSelection {
public:
    enum class Target { None, Entity, Asset };

    void set(std::uint64_t entityUuid) {
        if (m_selected.size() == 1 && m_selected.front() == entityUuid && m_target == Target::Entity) {
            return;
        }
        m_selected.clear();
        if (entityUuid != 0) {
            m_selected.push_back(entityUuid);
        }
        m_target = Target::Entity;
        emitChanged();
    }

    void setAsset(std::uint64_t uuid) {
        m_selected.clear();
        if (uuid != 0) {
            m_selected.push_back(uuid);
        }
        m_target = Target::Asset;
        emitChanged();
    }

    void selectMany(std::vector<std::uint64_t> uuids) {
        m_selected = std::move(uuids);
        m_target = Target::Entity;
        emitChanged();
    }

    void add(std::uint64_t uuid) {
        if (uuid == 0 || isSelected(uuid)) {
            return;
        }
        m_selected.push_back(uuid);
        emitChanged();
    }

    void toggle(std::uint64_t uuid) {
        if (uuid == 0) {
            return;
        }
        auto it = std::find(m_selected.begin(), m_selected.end(), uuid);
        if (it != m_selected.end()) {
            m_selected.erase(it);
        } else {
            m_selected.push_back(uuid);
        }
        emitChanged();
    }

    void remove(std::uint64_t uuid) {
        auto it = std::find(m_selected.begin(), m_selected.end(), uuid);
        if (it != m_selected.end()) {
            m_selected.erase(it);
            emitChanged();
        }
    }

    void clear() {
        if (!m_selected.empty()) {
            m_selected.clear();
            m_target = Target::None;
            emitChanged();
        }
    }

    bool hasSelection() const { return !m_selected.empty(); }
    bool empty() const { return m_selected.empty(); }
    bool isSelected(std::uint64_t uuid) const {
        return std::find(m_selected.begin(), m_selected.end(), uuid) != m_selected.end();
    }

    std::uint64_t selected() const { return m_selected.empty() ? 0 : m_selected.front(); }
    std::uint64_t primary() const { return selected(); }
    const std::vector<std::uint64_t>& selectedAll() const { return m_selected; }
    Target target() const { return m_target; }

    ScopedConnection subscribe(std::function<void()> onChange) {
        return ScopedConnection(m_changed, m_changed.connect(std::move(onChange)));
    }

private:
    void emitChanged() const {
        m_changed.fire();
    }

    std::vector<std::uint64_t> m_selected;
    Target m_target = Target::None;
    Signal<> m_changed;
};

} // namespace cakery
