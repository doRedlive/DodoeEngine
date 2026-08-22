// do@Redlive

#pragma once

#include <QWidget>

#include <cstddef>
#include <cstdint>

#include <nlohmann/json.hpp>

class QComboBox;
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
    void onAddComponent();
    void commitComponentValue(std::uint64_t uuid, std::size_t index, const nlohmann::json& value);

    EditorWorkspaceContext& m_context;
    QVBoxLayout* m_layout = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_componentCombo = nullptr;
    bool m_editing = false;
};

} // namespace cakery
