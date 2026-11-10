// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSelection.h"
#include "core/Signal.h"

#include <functional>
#include <utility>

namespace cakery {

class SelectionController {
public:
    explicit SelectionController(EditorSelection& selection)
        : m_selection(selection) {}

    void set(UInt64 uuid) { m_selection.set(uuid); }
    void setAsset(UInt64 uuid) { m_selection.setAsset(uuid); }
    void selectMany(const DynamicArray<UInt64>& uuids) { m_selection.selectMany(uuids); }
    void add(UInt64 uuid) { m_selection.add(uuid); }
    void toggle(UInt64 uuid) { m_selection.toggle(uuid); }
    void remove(UInt64 uuid) { m_selection.remove(uuid); }
    void clear() { m_selection.clear(); }

    bool hasSelection() const { return m_selection.hasSelection(); }
    bool empty() const { return m_selection.empty(); }
    bool isSelected(UInt64 uuid) const { return m_selection.isSelected(uuid); }
    UInt64 selected() const { return m_selection.selected(); }
    const DynamicArray<UInt64>& selectedAll() const { return m_selection.selectedAll(); }
    EditorSelection::Target target() const { return m_selection.target(); }

    ScopedConnection subscribe(std::function<void()> onChange)
    {
        return m_selection.subscribe(std::move(onChange));
    }

private:
    EditorSelection& m_selection;
};

} // namespace cakery
