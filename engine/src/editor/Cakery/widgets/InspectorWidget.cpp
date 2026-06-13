

#include "InspectorWidget.h"
#include "ComponentEditor.h"
#include "GenericComponentEditor.h"

#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/core/meta/component_db.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMenu>

namespace cakery {

static dodoe::ComponentDB& db() { return dodoe::ComponentDB::self(); }

InspectorWidget::InspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(7);

    setupGameObjectHeader();
    mainLayout->addWidget(m_headerWidget);

    setupTagLayerRow();
    mainLayout->addWidget(m_tagLayerWidget);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_editorContainer = new QWidget();
    m_editorContainer->setObjectName("componentsContent");
    m_editorLayout = new QVBoxLayout(m_editorContainer);
    m_editorLayout->setContentsMargins(0, 0, 0, 0);
    m_editorLayout->setSpacing(6);
    m_editorLayout->addStretch();

    m_scrollArea->setWidget(m_editorContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    setupAddComponentButton();
    mainLayout->addWidget(m_addCompWidget);


    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        onEntityNameChanged(m_nameEdit->text());
    });
}

void InspectorWidget::setupGameObjectHeader()
{
    m_headerWidget = new QWidget(this);
    auto* layout = new QHBoxLayout(m_headerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    m_enabledCheck = new QCheckBox(this);
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setToolTip(tr("Enable / Disable GameObject"));
    layout->addWidget(m_enabledCheck);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("GameObject Name"));
    m_nameEdit->setEnabled(false);
    layout->addWidget(m_nameEdit, 1);

    m_staticCheck = new QCheckBox(tr("Static"), this);
    m_staticCheck->setToolTip(tr("Mark as Static"));
    layout->addWidget(m_staticCheck);
}

void InspectorWidget::setupTagLayerRow()
{
    m_tagLayerWidget = new QWidget(this);
    auto* layout = new QHBoxLayout(m_tagLayerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* lblTag = new QLabel(tr("Tag"), this);
    lblTag->setStyleSheet("color: #6272A4;");
    layout->addWidget(lblTag);

    m_comboTag = new QComboBox(this);
    m_comboTag->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_comboTag->addItems({"Player", "Untagged", "MainCamera", "EditorOnly", "Respawn"});
    layout->addWidget(m_comboTag, 1);

    auto* lblLayer = new QLabel(tr("Layer"), this);
    lblLayer->setStyleSheet("color: #6272A4;");
    layout->addWidget(lblLayer);

    m_comboLayer = new QComboBox(this);
    m_comboLayer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_comboLayer->addItems({"Default", "UI", "Player", "Environment", "Ignore Raycast"});
    layout->addWidget(m_comboLayer, 1);
}

void InspectorWidget::setupAddComponentButton()
{
    m_addCompWidget = new QWidget(this);
    auto* layout = new QHBoxLayout(m_addCompWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addStretch();

    m_addBtn = new QPushButton(tr("Add Component"), this);
    m_addBtn->setMinimumHeight(26);
    m_addBtn->setMinimumWidth(180);
    m_addBtn->setEnabled(false);
    layout->addWidget(m_addBtn);

    layout->addStretch();

    connect(m_addBtn, &QPushButton::clicked, this, &InspectorWidget::populateAddComponentMenu);
}

void InspectorWidget::populateAddComponentMenu()
{
    if (!m_hasEntity || !m_entity.valid()) return;

    QMenu menu(this);
    bool hasItems = false;
    for (const auto& [type, typeName] : db().allComponents()) {

        if (typeName == "IDComponent" || typeName == "TagComponent")
            continue;
        if (!db().hasComponent(m_entity, typeName)) {
            QString displayName = QString::fromStdString(typeName);
            displayName.replace("Component", "");

            for (int i = 1; i < displayName.length(); ++i) {
                if (displayName[i].isUpper() && displayName[i-1].isLower()) {
                    displayName.insert(i, ' ');
                    ++i;
                }
            }
            menu.addAction(displayName, this, [this, typeName]() {
                onAddComponent(QString::fromStdString(typeName));
            });
            hasItems = true;
        }
    }

    if (!hasItems) {
        menu.addAction(tr("(No components available)"))->setEnabled(false);
    }

    menu.exec(m_addBtn->mapToGlobal(QPoint(0, m_addBtn->height())));
}


void InspectorWidget::onEntitySelected(dodoe::Entity entity)
{
    m_entity = entity;
    m_hasEntity = true;
    m_nameEdit->setEnabled(true);

    if (entity.hasComponent<dodoe::IDComponent>()) {
        auto& idComp = entity.getComponent<dodoe::IDComponent>();
        m_nameEdit->setText(QString::fromStdString(idComp.name));
    }

    m_addBtn->setEnabled(true);
    refresh();
}

void InspectorWidget::onEntityDeselected()
{
    m_hasEntity = false;
    m_nameEdit->clear();
    m_nameEdit->setEnabled(false);
    m_addBtn->setEnabled(false);
    clearEditors();
}

void InspectorWidget::refresh()
{
    clearEditors();

    if (!m_hasEntity || !m_entity.valid()) return;

    for (const auto& entry : db().entries()) {

        if (entry.name == "IDComponent" || entry.name == "TagComponent")
            continue;

        const QString typeName = QString::fromStdString(entry.name);
        if (db().hasComponent(m_entity, entry.name)) {
            auto* editor = createEditor(typeName, m_entity);
            if (editor) {
                auto* groupBox = wrapEditorInGroupBox(editor, typeName);
                m_editorLayout->insertWidget(m_editorLayout->count() - 1, groupBox);
                m_editors.push_back(editor);
                m_groupBoxes.push_back(groupBox);
            }
        }
    }
}

void InspectorWidget::clearEditors()
{

    for (auto* groupBox : m_groupBoxes) {
        m_editorLayout->removeWidget(groupBox);
        groupBox->deleteLater();
    }
    m_groupBoxes.clear();
    m_editors.clear();
}

ComponentEditor* InspectorWidget::createEditor(const QString& typeName, dodoe::Entity entity)
{
    auto* editor = new GenericComponentEditor(typeName, entity, typeName != "TransformComponent", this);
    connect(editor, &ComponentEditor::removeRequested,
            this, &InspectorWidget::onRemoveComponent);
    return editor;
}

QWidget* InspectorWidget::wrapEditorInGroupBox(ComponentEditor* editor, const QString& typeName)
{

    QString displayName = typeName;
    displayName.replace("Component", "");
    for (int i = 1; i < displayName.length(); ++i) {
        if (displayName[i].isUpper() && displayName[i-1].isLower()) {
            displayName.insert(i, ' ');
            ++i;
        }
    }

    auto* group = new QGroupBox(QString::fromUtf8("▼  ") + displayName, this);
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(8, 16, 8, 8);


    if (editor->canRemove()) {
        auto* headerRow = new QHBoxLayout();
        headerRow->addStretch();
        auto* removeBtn = new QPushButton(QString::fromUtf8("✕"), group);
        removeBtn->setFixedSize(18, 18);
        removeBtn->setToolTip(tr("Remove Component"));
        removeBtn->setFlat(true);
        removeBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #6272A4; border: none; font-size: 11px; }"
            "QPushButton:hover { color: #FF5555; }");
        connect(removeBtn, &QPushButton::clicked, this, [this, typeName]() {
            onRemoveComponent(typeName);
        });
        groupLayout->addLayout(headerRow);
    }

    groupLayout->addWidget(editor);
    return group;
}


void InspectorWidget::onEntityNameChanged(const QString& text)
{
    if (!m_hasEntity || !m_entity.valid()) return;
    if (m_entity.hasComponent<dodoe::IDComponent>())
        m_entity.getComponent<dodoe::IDComponent>().name = text.toStdString();
}

void InspectorWidget::onAddComponent(const QString& typeName)
{
    if (!m_hasEntity || !m_entity.valid() || typeName.isEmpty()) return;
    if (typeName == "TransformComponent") return;

    db().addComponent(m_entity, typeName.toLatin1().constData());
    refresh();
}

void InspectorWidget::onRemoveComponent(const QString& typeName)
{
    if (!m_hasEntity || !m_entity.valid()) return;
    if (typeName == "TransformComponent") return;

    db().removeComponent(m_entity, typeName.toLatin1().constData());
    refresh();
}

}
