// do@Redlive

#include "InspectorPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/inspector/EditorJsonWidget.h"
#include "core/document/EditorDocumentModel.h"

#include <QAction>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

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

QString inspectorSectionName(const QString& typeName)
{
    QString name = typeName;
    constexpr auto suffix = "Component";
    if (name.endsWith(QLatin1String(suffix))) {
        name.chop(static_cast<int>(std::char_traits<char>::length(suffix)));
    }
    return name;
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
    container->setObjectName(QStringLiteral("inspectorContent"));
    m_layout = new QVBoxLayout(container);
    m_layout->setContentsMargins(16, 16, 16, 16);
    m_layout->setSpacing(8);
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
        label->setStyleSheet(QStringLiteral("color: #A0A0A0;"));
        m_layout->addWidget(label);
        m_layout->addStretch();
        return;
    }

    auto* selectionBar = new QWidget(this);
    selectionBar->setObjectName(QStringLiteral("inspectorSelectionBar"));
    auto* selectionLayout = new QHBoxLayout(selectionBar);
    selectionLayout->setContentsMargins(10, 8, 6, 8);
    selectionLayout->setSpacing(8);

    m_nameEdit = new QLineEdit(QString::fromStdString(entity->name), selectionBar);
    m_nameEdit->setObjectName(QStringLiteral("inspectorObjectName"));
    m_nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        onRenameEntity(m_nameEdit->text());
    });
    selectionLayout->addWidget(m_nameEdit, 1);

    auto* objectMenu = new QMenu(selectionBar);
    QAction* removeEntityAction = objectMenu->addAction(tr("Remove Entity"));
    connect(removeEntityAction, &QAction::triggered, this, [this, uuid]() {
        m_context.session().deleteEntity(uuid);
    });

    auto* objectMenuBtn = new QToolButton(selectionBar);
    objectMenuBtn->setObjectName(QStringLiteral("inspectorObjectMenu"));
    objectMenuBtn->setText(QStringLiteral("⋯"));
    objectMenuBtn->setCursor(Qt::PointingHandCursor);
    connect(objectMenuBtn, &QToolButton::clicked, objectMenuBtn, [objectMenuBtn, objectMenu]() {
        objectMenu->popup(objectMenuBtn->mapToGlobal(QPoint(0, objectMenuBtn->height())));
    });
    selectionLayout->addWidget(objectMenuBtn);
    m_layout->addWidget(selectionBar);

    auto* filter = new QLineEdit(this);
    filter->setObjectName(QStringLiteral("inspectorFilter"));
    filter->setPlaceholderText(tr("Filter properties"));
    m_layout->addWidget(filter);

    std::vector<QWidget*> componentSections;

    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        const EditorComponent& component = entity->nativeComponents[i];
        const QString componentName = inspectorSectionName(QString::fromStdString(component.typeName));
        const bool expanded = (i < m_componentExpanded.size()) ? m_componentExpanded[i] : true;
        auto* section = new QWidget(this);
        section->setObjectName(QStringLiteral("inspectorSection"));
        auto* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(0);

        auto* headerRow = new QWidget(section);
        headerRow->setObjectName(QStringLiteral("inspectorSectionHeaderRow"));
        auto* headerRowLayout = new QHBoxLayout(headerRow);
        headerRowLayout->setContentsMargins(0, 0, 0, 0);
        headerRowLayout->setSpacing(0);

        auto* header = new QPushButton(headerRow);
        header->setObjectName(QStringLiteral("inspectorSectionHeader"));
        header->setText((expanded ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + componentName);
        header->setCheckable(true);
        header->setChecked(expanded);
        header->setFlat(true);
        headerRowLayout->addWidget(header, 1);

        auto* sectionMenu = new QMenu(section);
        QAction* removeAction = sectionMenu->addAction(tr("Remove Component"));
        connect(removeAction, &QAction::triggered, this, [this, uuid, i]() {
            if (i < m_componentExpanded.size()) {
                m_componentExpanded.erase(m_componentExpanded.begin() + static_cast<std::ptrdiff_t>(i));
            }
            m_context.session().removeComponent(uuid, i);
        });

        auto* menuBtn = new QToolButton(headerRow);
        menuBtn->setObjectName(QStringLiteral("inspectorSectionMenu"));
        menuBtn->setText(QStringLiteral("⋯"));
        menuBtn->setCursor(Qt::PointingHandCursor);
        connect(menuBtn, &QToolButton::clicked, menuBtn, [menuBtn, sectionMenu]() {
            sectionMenu->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
        });
        headerRowLayout->addWidget(menuBtn);

        header->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(header, &QWidget::customContextMenuRequested, header, [header, sectionMenu](const QPoint& pos) {
            sectionMenu->popup(header->mapToGlobal(pos));
        });

        sectionLayout->addWidget(headerRow);

        auto* body = new QWidget(section);
        body->setObjectName(QStringLiteral("inspectorSectionBody"));
        body->setVisible(expanded);
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(6, 8, 0, 4);
        bodyLayout->setSpacing(8);

        auto* editor = new EditorJsonWidget(component.value, body);
        connect(editor, &EditorJsonWidget::valueChanged, this, [this, uuid, i, editor]() {
            commitComponentValue(uuid, i, editor->value());
        });
        bodyLayout->addWidget(editor);

        sectionLayout->addWidget(body);
        connect(header, &QPushButton::toggled, this, [this, i, header, body, componentName](bool expanded) {
            header->setText((expanded ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + componentName);
            body->setVisible(expanded);
            if (m_componentExpanded.size() <= i) {
                m_componentExpanded.resize(i + 1, true);
            }
            m_componentExpanded[i] = expanded;
        });

        componentSections.push_back(section);
        m_layout->addWidget(section);
    }

    m_componentExpanded.resize(entity->nativeComponents.size(), true);

    connect(filter, &QLineEdit::textChanged, this, [componentSections](const QString& text) {
        for (QWidget* section : componentSections) {
            section->setVisible(text.isEmpty()
                || section->findChild<QPushButton*>(QStringLiteral("inspectorSectionHeader"))->text()
                    .contains(text, Qt::CaseInsensitive));
        }
    });

    auto* addBtn = new QPushButton(tr("Add Component"), this);
    addBtn->setObjectName(QStringLiteral("inspectorAddButton"));
    addBtn->setCursor(Qt::PointingHandCursor);
    auto* addMenu = new QMenu(addBtn);
    auto* search = new QLineEdit(addMenu);
    search->setObjectName(QStringLiteral("inspectorAddSearch"));
    search->setPlaceholderText(tr("Search components..."));
    auto* searchAction = new QWidgetAction(addMenu);
    searchAction->setDefaultWidget(search);
    addMenu->addAction(searchAction);
    std::vector<QAction*> typeActions;
    for (const auto& entry : ComponentTemplates()) {
        QAction* action = addMenu->addAction(QString::fromUtf8(entry.typeName));
        typeActions.push_back(action);
        connect(action, &QAction::triggered, this, [this, typeName = entry.typeName]() {
            addComponent(typeName);
        });
    }
    connect(search, &QLineEdit::textChanged, this, [typeActions](const QString& text) {
        for (QAction* action : typeActions) {
            action->setVisible(action->text().contains(text, Qt::CaseInsensitive));
        }
    });
    connect(addBtn, &QPushButton::clicked, this, [addBtn, addMenu, search]() {
        search->clear();
        addMenu->popup(addBtn->mapToGlobal(QPoint(0, -addMenu->sizeHint().height())));
        search->setFocus(Qt::PopupFocusReason);
    });
    m_layout->addWidget(addBtn);

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

void InspectorPanel::addComponent(const std::string& typeName)
{
    const std::uint64_t uuid = m_context.session().selection().selected();
    if (!uuid) {
        return;
    }
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
