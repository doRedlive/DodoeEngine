

#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <vector>
#include "runtime/function/world/entity.h"

namespace cakery {

class ComponentEditor;

class InspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit InspectorWidget(QWidget* parent = nullptr);

public slots:
    void onEntitySelected(dodoe::Entity entity);
    void onEntityDeselected();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onEntityNameChanged(const QString& text);
    void onAddComponent(const QString& typeName);
    void onRemoveComponent(const QString& typeName);

private:
    void refresh();
    void clearEditors();
    ComponentEditor* createEditor(const QString& typeName, dodoe::Entity entity);
    QWidget* wrapEditorInGroupBox(ComponentEditor* editor, const QString& typeName);
    void populateAddComponentMenu();
    void setupGameObjectHeader();
    void setupTagLayerRow();
    void setupAddComponentButton();
    void importDroppedAssets(const QMimeData* mime);


    QWidget* m_headerWidget = nullptr;
    QCheckBox* m_enabledCheck = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QCheckBox* m_staticCheck = nullptr;


    QWidget* m_tagLayerWidget = nullptr;
    QComboBox* m_comboTag = nullptr;
    QComboBox* m_comboLayer = nullptr;


    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_editorContainer = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;


    QWidget* m_addCompWidget = nullptr;
    QPushButton* m_addBtn = nullptr;


    dodoe::Entity m_entity{};
    std::vector<ComponentEditor*> m_editors;
    std::vector<QWidget*> m_groupBoxes;
    bool m_hasEntity = false;
};

}
