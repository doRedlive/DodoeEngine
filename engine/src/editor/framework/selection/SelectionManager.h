// do@Redlive

#pragma once

#include <vector>
#include <algorithm>

#include "runtime/core/utils/uuid.h"
#include "framework/core/Signal.h"

namespace cakery {

enum class SelectionTarget { None, Entity, Asset };

class SelectionManager {
public:
    const std::vector<dodoe::UUID>& selected() const { return m_selected; }

    bool isSelected(dodoe::UUID id) const {
        return std::find(m_selected.begin(), m_selected.end(), id) != m_selected.end();
    }

    bool empty() const { return m_selected.empty(); }

    dodoe::UUID primary() const {
        return m_selected.empty() ? dodoe::UUID{} : m_selected.front();
    }

    void select(dodoe::UUID id) {
        m_selected.clear();
        if (id.isValid()) m_selected.push_back(id);
        m_target = SelectionTarget::Entity;
        notify();
    }

    void selectAsset(dodoe::UUID id) {
        m_selected.clear();
        if (id.isValid()) m_selected.push_back(id);
        m_target = SelectionTarget::Asset;
        notify();
    }

    SelectionTarget target() const { return m_target; }

    void selectMany(std::vector<dodoe::UUID> ids) {
        m_selected = std::move(ids);
        notify();
    }

    void add(dodoe::UUID id) {
        if (!id.isValid()) return;
        if (!isSelected(id)) {
            m_selected.push_back(id);
            notify();
        }
    }

    void toggle(dodoe::UUID id) {
        if (!id.isValid()) return;
        auto it = std::find(m_selected.begin(), m_selected.end(), id);
        if (it != m_selected.end()) {
            m_selected.erase(it);
        } else {
            m_selected.push_back(id);
        }
        notify();
    }

    void remove(dodoe::UUID id) {
        auto it = std::find(m_selected.begin(), m_selected.end(), id);
        if (it != m_selected.end()) {
            m_selected.erase(it);
            notify();
        }
    }

    void clear() {
        if (!m_selected.empty()) {
            m_selected.clear();
            m_target = SelectionTarget::None;
            notify();
        }
    }

    Signal<const std::vector<dodoe::UUID>&> changed;

private:
    std::vector<dodoe::UUID> m_selected;
    SelectionTarget m_target{SelectionTarget::None};

    void notify() { changed.fire(m_selected); }
};

} // namespace cakery
