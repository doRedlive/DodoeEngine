// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"

#include <QWidget>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace cakery {

class EditorContext;

struct InspectorContext {
    EditorContext* ctx         = nullptr;
    dodoe::Uuid    entity;
    std::string    componentName;
    void*          componentPtr = nullptr;
    QWidget*       parent       = nullptr;
};

class CustomEditor {
public:
    virtual ~CustomEditor() = default;
    virtual QWidget* build(const InspectorContext& ic) = 0;
    virtual void refresh(const InspectorContext& ic) = 0;
};

class CustomEditorRegistry {
public:
    static CustomEditorRegistry& self();

    using Factory = std::function<std::unique_ptr<CustomEditor>()>;

    void registerByName(const std::string& editorName, Factory f);
    void mapComponent(const std::string& componentName, const std::string& editorName);
    std::unique_ptr<CustomEditor> create(const std::string& componentName) const;

private:
    CustomEditorRegistry() = default;

    std::unordered_map<std::string, Factory> m_byName;
    std::unordered_map<std::string, std::string> m_comp2name;
};

} // namespace cakery
