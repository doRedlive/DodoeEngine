// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class QLineEdit;
class QVBoxLayout;

namespace cakery {

class EditorWorkspaceContext;

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void onDocumentChanged();
    void onRenameEntity(const QString& name);
    void addComponent(const std::string& typeName);
    void commitComponentValue(std::uint64_t uuid, std::size_t index, const nlohmann::json& value);

    EditorWorkspaceContext& m_context;
    QVBoxLayout* m_layout = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    std::vector<bool> m_componentExpanded;
    bool m_editing = false;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_selectionSubscription;
};

} // namespace cakery
