// do@Redlive

#include "InspectorPanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"
#include "cakery/ui/EditorIcons.h"
#include "cakery/ui/inspector/EditorJsonWidget.h"
#include "core/document/EditorDocumentModel.h"

#include <set>

#include <QAction>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSize>
#include <QToolButton>
#include <QImage>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <string>
#include <unordered_set>
#include <utility>
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
            {{"sprite", {{"asset_id", 0}, {"sub_object_id", 0}}}, {"flip", false},
             {"pivot", {0.0, 0.0}}, {"depth", 0.0}, {"color", {1.0, 1.0, 1.0, 1.0}}}},
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

const nlohmann::json* DefaultComponentValue(const std::string& typeName)
{
    for (const ComponentTemplate& entry : ComponentTemplates()) {
        if (typeName == entry.typeName) {
            return &entry.defaultValue;
        }
    }
    return nullptr;
}

QString inspectorSectionName(const QString& typeName)
{
    QString name = typeName;
    const int namespaceSeparator = qMax(name.lastIndexOf(QLatin1Char('.')),
                                        name.lastIndexOf(QLatin1Char('+')));
    if (namespaceSeparator >= 0) {
        name = name.mid(namespaceSeparator + 1);
    }
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
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorContent"));
    m_layout = new QVBoxLayout(container);
    m_layout->setContentsMargins(16, 16, 16, 16);
    m_layout->setSpacing(8);
    scroll->setWidget(container);
    outer->addWidget(scroll);

    m_documentSubscription = m_context.session().documentModel().subscribe([this]() { onDocumentChanged(); });
    m_selectionSubscription = m_context.session().selection().subscribe([this]() {
        syncSelectedAsset();
        refresh();
    });
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

    if (m_selectedAsset) {
        const AssetBrowserEntry asset = *m_selectedAsset;
        auto* title = new QLabel(QString::fromStdString(asset.name), this);
        title->setObjectName(QStringLiteral("inspectorAssetName"));
        m_layout->addWidget(title);

        QImage image(QString::fromStdString(asset.path));
        if (!image.isNull()) {
            auto* preview = new QLabel(this);
            preview->setObjectName(QStringLiteral("inspectorAssetPreview"));
            preview->setAlignment(Qt::AlignCenter);
            preview->setPixmap(QPixmap::fromImage(image).scaled(
                QSize(220, 180), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_layout->addWidget(preview);
        }

        auto* details = new QGroupBox(tr("Asset"), this);
        auto* detailsLayout = new QFormLayout(details);
        detailsLayout->addRow(tr("Type"), new QLabel(QString::fromStdString(asset.type), details));
        detailsLayout->addRow(tr("Path"), new QLabel(QString::fromStdString(asset.path), details));
        detailsLayout->addRow(tr("GUID"), new QLabel(QString::number(static_cast<qulonglong>(asset.uuid)), details));
        m_layout->addWidget(details);

        AssetImportSettings importSettings;
        if (m_context.session().getAssetImportSettings(asset.path, importSettings)) {
            auto* importGroup = new QGroupBox(tr("Import Settings"), this);
            auto* importLayout = new QVBoxLayout(importGroup);
            auto* importer = new QLabel(tr("Importer: %1").arg(QString::fromStdString(importSettings.importer)), importGroup);
            importer->setObjectName(QStringLiteral("inspectorAssetImporter"));
            importLayout->addWidget(importer);
            auto* settingsEditor = new EditorJsonWidget(importSettings.settings, importGroup);
            importLayout->addWidget(settingsEditor);
            auto* apply = new QPushButton(tr("Apply"), importGroup);
            apply->setObjectName(QStringLiteral("projectPrimaryButton"));
            connect(apply, &QPushButton::clicked, this, [this, asset, settingsEditor]() {
                nlohmann::json payload{{"path", asset.path}, {"settings", settingsEditor->value()}};
                if (m_context.session().execute({"asset.update_settings", payload.dump()})) {
                    m_selectedAsset = asset;
                    refresh();
                }
            });
            importLayout->addWidget(apply);
            m_layout->addWidget(importGroup);
        }
        m_layout->addStretch();
        return;
    }

    const std::uint64_t uuid = m_context.session().selection().selected();
    const EditorEntity* entity = uuid ? m_context.session().documentModel().findEntity(uuid) : nullptr;
    if (!entity) {
        auto* label = new QLabel(tr("No GameObject selected"), this);
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
    QAction* removeEntityAction = objectMenu->addAction(tr("Remove GameObject"));
    connect(removeEntityAction, &QAction::triggered, this, [this, uuid]() {
        m_context.session().deleteEntity(uuid);
    });

    auto* objectMenuBtn = new QToolButton(selectionBar);
    objectMenuBtn->setObjectName(QStringLiteral("inspectorObjectMenu"));
    objectMenuBtn->setIcon(editorIcon(QStringLiteral("chevron-down.svg")));
    objectMenuBtn->setIconSize(QSize(16, 16));
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

    std::vector<AssetBrowserEntry> assets;
    m_context.session().listAssets(assets);

    std::vector<std::uint64_t> multiUuids;
    for (const std::uint64_t selectedUuid : m_context.session().selection().selectedAll()) {
        if (selectedUuid != uuid && m_context.session().documentModel().findEntity(selectedUuid)) {
            multiUuids.push_back(selectedUuid);
        }
    }
    std::set<std::string> commonTypes;
    if (!multiUuids.empty()) {
        for (const EditorComponent& component : entity->nativeComponents) {
            bool common = true;
            for (const std::uint64_t otherUuid : multiUuids) {
                const EditorEntity* other = m_context.session().documentModel().findEntity(otherUuid);
                bool found = false;
                for (const EditorComponent& otherComponent : other->nativeComponents) {
                    if (otherComponent.typeName == component.typeName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    common = false;
                    break;
                }
            }
            if (common) {
                commonTypes.insert(component.typeName);
            }
        }
        auto* multiLabel = new QLabel(tr("%1 GameObjects selected").arg(multiUuids.size() + 1), this);
        multiLabel->setObjectName(QStringLiteral("inspectorMultiSelection"));
        m_layout->addWidget(multiLabel);
    }

    std::vector<QWidget*> componentSections;

    const auto addComponentSection = [&](const EditorComponent& component,
                                          std::size_t index, bool managed) {
        if (!managed && component.typeName == "IDComponent") {
            return;
        }
        if (!multiUuids.empty() && !managed && !commonTypes.contains(component.typeName)) {
            return;
        }
        const QString componentName = inspectorSectionName(QString::fromStdString(component.typeName));
        const auto& expandedState = managed ? m_managedComponentExpanded : m_componentExpanded;
        const bool expanded = (index < expandedState.size()) ? expandedState[index] : true;
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
        header->setText(componentName);
        header->setIcon(editorIcon(expanded ? QStringLiteral("chevron-down.svg")
                                             : QStringLiteral("chevron-right.svg")));
        header->setIconSize(QSize(16, 16));
        header->setCheckable(true);
        header->setChecked(expanded);
        header->setFlat(true);
        headerRowLayout->addWidget(header, 1);

        auto* sectionMenu = new QMenu(section);
        QAction* copyAction = sectionMenu->addAction(tr("Copy Component"));
        QAction* pasteAction = sectionMenu->addAction(tr("Paste Component Values"));
        pasteAction->setEnabled(!component.typeName.empty() &&
                                m_componentClipboardType == component.typeName);
        connect(pasteAction, &QAction::triggered, this, [this, uuid, index, managed, typeName = component.typeName]() {
            if (m_componentClipboardType != typeName) {
                return;
            }
            commitComponentValue(uuid, index, m_componentClipboardValue, managed);
        });
        connect(copyAction, &QAction::triggered, this, [this, typeName = component.typeName,
                                                        value = component.value]() {
            m_componentClipboardType = typeName;
            m_componentClipboardValue = value;
        });
        sectionMenu->addSeparator();
        QAction* resetAction = sectionMenu->addAction(tr("Reset to Default"));
        const nlohmann::json* defaultValues = DefaultComponentValue(component.typeName);
        resetAction->setEnabled(defaultValues != nullptr);
        if (defaultValues) {
            connect(resetAction, &QAction::triggered, this,
                    [this, uuid, index, managed, defaultValue = *defaultValues]() {
                        commitComponentValue(uuid, index, defaultValue, managed);
                    });
        }
        if (!managed) {
            sectionMenu->addSeparator();
            QAction* moveUpAction = sectionMenu->addAction(tr("Move Up"));
            moveUpAction->setEnabled(index > 0);
            connect(moveUpAction, &QAction::triggered, this, [this, uuid, index]() {
                if (m_context.session().moveComponent(uuid, index, -1) &&
                    index > 0 && index < m_componentExpanded.size()) {
                    std::swap(m_componentExpanded[index], m_componentExpanded[index - 1]);
                }
            });
            QAction* moveDownAction = sectionMenu->addAction(tr("Move Down"));
            connect(moveDownAction, &QAction::triggered, this, [this, uuid, index]() {
                if (m_context.session().moveComponent(uuid, index, 1) &&
                    index + 1 < m_componentExpanded.size()) {
                    std::swap(m_componentExpanded[index], m_componentExpanded[index + 1]);
                }
            });
        }
        sectionMenu->addSeparator();
        QAction* removeAction = sectionMenu->addAction(
            managed ? tr("Remove Managed Component") : tr("Remove Component"));
        connect(removeAction, &QAction::triggered, this, [this, uuid, index, managed]() {
            if (managed) {
                if (index < m_managedComponentExpanded.size()) {
                    m_managedComponentExpanded.erase(
                        m_managedComponentExpanded.begin() + static_cast<std::ptrdiff_t>(index));
                }
                m_context.session().removeManagedComponent(uuid, index);
            } else {
                if (index < m_componentExpanded.size()) {
                    m_componentExpanded.erase(
                        m_componentExpanded.begin() + static_cast<std::ptrdiff_t>(index));
                }
                m_context.session().removeComponent(uuid, index);
            }
        });

        auto* menuBtn = new QToolButton(headerRow);
        menuBtn->setObjectName(QStringLiteral("inspectorSectionMenu"));
        menuBtn->setIcon(editorIcon(QStringLiteral("ellipsis.svg")));
        menuBtn->setIconSize(QSize(16, 16));
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

        std::vector<InspectorFieldMetadata> reflectedFields;
        if (!managed) {
            m_context.session().inspectComponent(component.typeName, reflectedFields);
        }
        auto* editor = reflectedFields.empty()
            ? new EditorJsonWidget(component.value, body)
            : new EditorJsonWidget(component.value, std::move(reflectedFields), assets, body);
        connect(editor, &EditorJsonWidget::valueChanged, this, [this, uuid, index, managed, editor]() {
            commitComponentValue(uuid, index, editor->value(), managed);
        });
        bodyLayout->addWidget(editor);

        sectionLayout->addWidget(body);
        connect(header, &QPushButton::toggled, this, [this, index, managed, header, body](bool expanded) {
            header->setIcon(editorIcon(expanded ? QStringLiteral("chevron-down.svg")
                                                 : QStringLiteral("chevron-right.svg")));
            body->setVisible(expanded);
            auto& state = managed ? m_managedComponentExpanded : m_componentExpanded;
            if (state.size() <= index) {
                state.resize(index + 1, true);
            }
            state[index] = expanded;
        });

        componentSections.push_back(section);
        m_layout->addWidget(section);
    };

    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        addComponentSection(entity->nativeComponents[i], i, false);
    }
    for (std::size_t i = 0; i < entity->managedComponents.size(); ++i) {
        addComponentSection(entity->managedComponents[i], i, true);
    }

    m_componentExpanded.resize(entity->nativeComponents.size(), true);
    m_managedComponentExpanded.resize(entity->managedComponents.size(), true);

    connect(filter, &QLineEdit::textChanged, this, [componentSections](const QString& text) {
        const QString needle = text.trimmed();
        for (QWidget* section : componentSections) {
            if (needle.isEmpty()) {
                section->setVisible(true);
                continue;
            }
            const auto* header = section->findChild<QPushButton*>(
                QStringLiteral("inspectorSectionHeader"));
            bool matches = header && header->text().contains(needle, Qt::CaseInsensitive);
            const auto widgets = section->findChildren<QWidget*>();
            for (QWidget* widget : widgets) {
                if (const auto* label = qobject_cast<QLabel*>(widget)) {
                    matches = matches || label->text().contains(needle, Qt::CaseInsensitive) ||
                        label->toolTip().contains(needle, Qt::CaseInsensitive);
                } else if (const auto* button = qobject_cast<QAbstractButton*>(widget)) {
                    matches = matches || button->text().contains(needle, Qt::CaseInsensitive) ||
                        button->toolTip().contains(needle, Qt::CaseInsensitive);
                } else if (const auto* edit = qobject_cast<QLineEdit*>(widget)) {
                    matches = matches || edit->text().contains(needle, Qt::CaseInsensitive) ||
                        edit->toolTip().contains(needle, Qt::CaseInsensitive);
                }
                if (matches) {
                    break;
                }
            }
            section->setVisible(matches);
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
    std::unordered_set<std::string> existingTypes;
    for (const auto& component : entity->nativeComponents) {
        existingTypes.insert(component.typeName);
    }
    std::vector<QAction*> typeActions;
    for (const auto& entry : ComponentTemplates()) {
        if (existingTypes.contains(entry.typeName)) {
            continue;
        }
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

void InspectorPanel::syncSelectedAsset()
{
    const EditorSelection& selection = m_context.session().selection();
    if (selection.target() != EditorSelection::Target::Asset) {
        m_selectedAsset.reset();
        return;
    }
    if (m_selectedAsset && m_selectedAsset->uuid == selection.selected()) {
        return;
    }
    m_selectedAsset.reset();
    std::vector<AssetBrowserEntry> entries;
    if (!m_context.session().listAssets(entries)) {
        return;
    }
    for (const AssetBrowserEntry& entry : entries) {
        if (entry.uuid == selection.selected()) {
            m_selectedAsset = entry;
            return;
        }
    }
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
    const EditorEntity* entity = m_context.session().documentModel().findEntity(uuid);
    if (!entity) {
        return;
    }
    for (const auto& component : entity->nativeComponents) {
        if (component.typeName == typeName) {
            return;
        }
    }
    for (const auto& entry : ComponentTemplates()) {
        if (entry.typeName == typeName) {
            m_context.session().addComponent(uuid, EditorComponent{typeName, entry.defaultValue});
            return;
        }
    }
}

void InspectorPanel::commitComponentValue(std::uint64_t uuid, std::size_t index,
                                           const nlohmann::json& value, bool managed)
{
    m_editing = true;
    const EditorSelection& selection = m_context.session().selection();
    const EditorEntity* primary = m_context.session().documentModel().findEntity(uuid);
    if (selection.selectedAll().size() > 1 && uuid == selection.selected() && primary) {
        const std::vector<EditorComponent>& components =
            managed ? primary->managedComponents : primary->nativeComponents;
        const std::string typeName = index < components.size()
            ? components[index].typeName : std::string();
        m_context.session().updateComponentOnEntities(selection.selectedAll(), typeName, value, managed);
    } else if (managed) {
        m_context.session().updateManagedComponent(uuid, index, value);
    } else {
        m_context.session().updateComponent(uuid, index, value);
    }
    m_editing = false;
}

} // namespace cakery
