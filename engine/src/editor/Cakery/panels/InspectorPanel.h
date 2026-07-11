// do@Redlive

#pragma once

#include "Panel.h"
#include "runtime/core/utils/uuid.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <vector>

namespace cakery {

class PropertyDrawer;

class InspectorPanel : public Panel {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorContext& ctx, QWidget* parent = nullptr);

    void refresh();

private:
    void clearEditors();
    void rebuildForEntity(dodoe::Uuid uuid);

    QLineEdit* m_nameEdit = nullptr;
    QPushButton* m_addBtn = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_editorContainer = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;

    struct DrawerEntry {
        std::string componentName;
        std::string fieldName;
        PropertyDrawer* drawer = nullptr;
        QWidget* widget = nullptr;
    };
    std::vector<DrawerEntry> m_entries;
};

} // namespace cakery
