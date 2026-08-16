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

namespace dodoe {
class FieldAccessor;
class Asset;
}

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
    void rebuildForEntity(dodoe::UUID uuid);
    void rebuildForAsset(dodoe::UUID uuid);
    void renderFields(const std::string& typeName, void* objPtr, dodoe::UUID ownerUuid, bool isAsset = false);
    void applyFieldAttributes(dodoe::FieldAccessor* fields, int count, const std::string& typeName);
    void addPlaceholder(const QString& text);
    void showAssetMeta(dodoe::Asset* asset);

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

    dodoe::UUID m_currentEntity;
    dodoe::UUID m_currentAsset;
};

} // namespace cakery
