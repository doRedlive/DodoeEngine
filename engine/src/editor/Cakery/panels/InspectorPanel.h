// do@Redlive

#pragma once

#include "Panel.h"
#include "runtime/core/utils/uuid.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <vector>
#include <string>

namespace cakery {

class PropertyDrawer;
class CustomEditor;

class InspectorPanel : public Panel {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void refresh();

private:
    void clearEditors();
    void rebuildForEntity(dodoe::Uuid uuid);
    void applyFieldAttributes(dodoe::FieldAccessor* fields, int count, const std::string& typeName);

    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_addBtn = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_editorContainer = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;

    struct DrawerEntry {
        std::string componentName;
        std::string fieldName;
        PropertyDrawer* drawer = nullptr;
        CustomEditor* customEditor = nullptr;
        QWidget* widget = nullptr;
    };
    std::vector<DrawerEntry> m_entries;

    dodoe::Uuid m_currentEntity;
};

} // namespace cakery
