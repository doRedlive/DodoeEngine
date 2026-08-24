// do@Redlive

#include "InspectorPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/inspector/EditorJsonWidget.h"
#include "core/document/EditorDocumentModel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace cakery {

namespace {

struct ComponentTemplate {
    const char* typeName;
    nlohmann::json defaultValue;
};

const std::vector<ComponentTemplate>& ComponentTemplates() {
    static const std::vector<ComponentTemplate> templates = {
        {"TagComponent", {{"tag", "default"}}},
        {"TransformComponent",
            {{"position", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"scale", {1.0, 1.0, 1.0}}}},
        {"CameraComponent",
            {{"type", 0}, {"zoom", 1.0}, {"fov", 60.0}, {"near_plane", 0.01}, {"far_plane", 1000.0},
             {"aspect_ratio", 16.0 / 9.0}, {"background", {1.0, 1.0, 1.0, 1.0}}}},
        {"SpriteRendererComponent",
            {{"flip", false}, {"pivot", {0.0, 0.0}}, {"depth", 0.0}, {"color", {1.0, 1.0, 1.0, 1.0}}}},
        {"RectRendererComponent",
            {{"size", {1.0, 1.0}}, {"color", {1.0, 1.0, 1.0, 1.0}}, {"thickness", 0.0}}},
        {"CircleRendererComponent",
            {{"radius", 1.0}, {"color", {1.0, 1.0, 1.0, 1.0}}, {"segments", 32}, {"thickness", 0.0}}},
        {"LineRendererComponent",
            {{"direction", {1.0, 0.0}}, {"length", 1.0}, {"thickness", 1.0}, {"color", {1.0, 1.0, 1.0, 1.0}}}},
        {"Rigidbody2dComponent", {{"type", 0}, {"gravity_scale", 1.0}, {"fixed_rotation", false}}},
        {"BoxCollider2dComponent",
            {{"offset", {0.0, 0.0}}, {"size", {10.0, 10.0}}, {"is_sensor", false}, {"layer", 1},
             {"mask", 4294967295u}, {"density", 1.0}, {"friction", 0.5}, {"restitution", 0.0}}},
        {"BoxColliderComponent",
            {{"offset", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"size", {1.0, 1.0, 1.0}},
             {"is_sensor", false}, {"layer", 1}, {"mask", 4294967295u}, {"density", 1.0},
             {"friction", 0.5}, {"restitution", 0.0}}},
        {"PointLightComponent",
            {{"color", {1.0, 1.0, 1.0, 1.0}}, {"intensity", 1.0}, {"radius", 0.0}, {"range", 10.0}}},
        {"HierarchyComponent", {{"parent_uuid", 0}, {"child_count", 0}}},
    };
    return templates;
}

} // namespace

InspectorPanel::InspectorPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent), m_context(context)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* container = new QWidget();
    m_layout = new QVBoxLayout(container);
    m_layout->setContentsMargins(6, 6, 6, 6);
    scroll->setWidget(container);
    outer->addWidget(scroll);

    m_context.session().documentModel().subscribe([this]() { onDocumentChanged(); });
    m_context.session().selection().subscribe([this]() { refresh(); });
    refresh();
}

void InspectorPanel::refresh()
{
    QLayoutItem* child = nullptr;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    const std::uint64_t uuid = m_context.session().selection().selected();
    const EditorEntity* entity = uuid ? m_context.session().documentModel().findEntity(uuid) : nullptr;
    if (!entity) {
        auto* label = new QLabel(tr("No entity selected"), this);
        label->setStyleSheet(QStringLiteral("color: #777;"));
        m_layout->addWidget(label);
        m_layout->addStretch();
        return;
    }

    m_nameEdit = new QLineEdit(QString::fromStdString(entity->name), this);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        onRenameEntity(m_nameEdit->text());
    });
    m_layout->addWidget(m_nameEdit);

    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        const EditorComponent& component = entity->nativeComponents[i];
        auto* group = new QGroupBox(QString::fromStdString(component.typeName), this);
        auto* groupLayout = new QVBoxLayout(group);

        auto* editor = new EditorJsonWidget(component.value, group);
        connect(editor, &EditorJsonWidget::valueChanged, this, [this, uuid, i, editor]() {
            commitComponentValue(uuid, i, editor->value());
        });
        groupLayout->addWidget(editor);

        auto* remove = new QPushButton(tr("Remove Component"), group);
        connect(remove, &QPushButton::clicked, this, [this, uuid, i]() {
            m_context.session().removeComponent(uuid, i);
        });
        groupLayout->addWidget(remove);

        m_layout->addWidget(group);
    }

    auto* addRow = new QWidget(this);
    auto* addLayout = new QHBoxLayout(addRow);
    addLayout->setContentsMargins(0, 0, 0, 0);
    m_componentCombo = new QComboBox(addRow);
    for (const auto& entry : ComponentTemplates()) {
        m_componentCombo->addItem(QString::fromUtf8(entry.typeName));
    }
    auto* add = new QPushButton(tr("Add Component"), addRow);
    connect(add, &QPushButton::clicked, this, &InspectorPanel::onAddComponent);
    addLayout->addWidget(m_componentCombo, 1);
    addLayout->addWidget(add);
    m_layout->addWidget(addRow);

    m_layout->addStretch();
}

void InspectorPanel::onDocumentChanged()
{
    if (m_editing) {
        return;
    }
    refresh();
}

void InspectorPanel::onRenameEntity(const QString& name)
{
    const std::uint64_t uuid = m_context.session().selection().selected();
    if (uuid) {
        m_context.session().renameEntity(uuid, name.toStdString());
    }
}

void InspectorPanel::onAddComponent()
{
    const std::uint64_t uuid = m_context.session().selection().selected();
    if (!uuid || !m_componentCombo) {
        return;
    }
    const std::string typeName = m_componentCombo->currentText().toStdString();
    for (const auto& entry : ComponentTemplates()) {
        if (entry.typeName == typeName) {
            m_context.session().addComponent(uuid, EditorComponent{typeName, entry.defaultValue});
            return;
        }
    }
}

void InspectorPanel::commitComponentValue(std::uint64_t uuid, std::size_t index, const nlohmann::json& value)
{
    m_editing = true;
    m_context.session().updateComponent(uuid, index, value);
    m_editing = false;
}

} // namespace cakery
