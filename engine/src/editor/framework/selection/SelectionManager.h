// do@Redlive

#pragma once

#include <vector>
#include <algorithm>

#include "runtime/core/utils/uuid.h"
#include "framework/core/Signal.h"

namespace cakery {

class SelectionManager {
public:
    const std::vector<dodoe::Uuid>& selected() const { return m_selected; }

    bool isSelected(dodoe::Uuid id) const {
        return std::find(m_selected.begin(), m_selected.end(), id) != m_selected.end();
    }

    bool empty() const { return m_selected.empty(); }

    dodoe::Uuid primary() const {
        return m_selected.empty() ? dodoe::Uuid{} : m_selected.front();
    }

    void select(dodoe::Uuid id) {
        m_selected.clear();
        if (id.isValid()) m_selected.push_back(id);
        notify();
    }

    void selectMany(std::vector<dodoe::Uuid> ids) {
        m_selected = std::move(ids);
        notify();
    }

    void add(dodoe::Uuid id) {
        if (!id.isValid()) return;
        if (!isSelected(id)) {
            m_selected.push_back(id);
            notify();
        }
    }

    void toggle(dodoe::Uuid id) {
        if (!id.isValid()) return;
        auto it = std::find(m_selected.begin(), m_selected.end(), id);
        if (it != m_selected.end()) {
            m_selected.erase(it);
        } else {
            m_selected.push_back(id);
        }
        notify();
    }

    void remove(dodoe::Uuid id) {
        auto it = std::find(m_selected.begin(), m_selected.end(), id);
        if (it != m_selected.end()) {
            m_selected.erase(it);
            notify();
        }
    }

    void clear() {
        if (!m_selected.empty()) {
            m_selected.clear();
            notify();
        }
    }

    Signal<const std::vector<dodoe::Uuid>&> changed;

private:
    std::vector<dodoe::Uuid> m_selected;

    void notify() { changed.fire(m_selected); }
};

} // namespace cakery
