// do@Redlive

#include "dopch.h"

#include "InspectorPanel.h"

#include "presentation/inspector/InspectorViewModel.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/EditorIcons.h"
#include "ui/common/QtStringInterop.h"
#include "ui/inspector/EditorJsonWidget.h"
#include "ui/inspector/EditorRemoteWidget.h"

#include <set>

#include <QAction>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSize>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QClipboard>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cakery {

namespace {

class AssetImagePreview final : public QWidget {
public:
    explicit AssetImagePreview(QImage image, QWidget* parent = nullptr)
        : QWidget(parent), m_image(std::move(image))
    {
        setObjectName(QStringLiteral("inspectorAssetPreview"));
        setMinimumHeight(140);
        setMaximumHeight(220);
        setToolTip(QStringLiteral("%1 x %2 px").arg(m_image.width()).arg(m_image.height()));
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        constexpr int cell = 8;
        painter.setPen(Qt::NoPen);
        for (int y = 0; y < height(); y += cell) {
            for (int x = 0; x < width(); x += cell) {
                if (((x / cell) + (y / cell)) % 2 == 0) {
                    painter.fillRect(x, y, cell, cell, palette().color(QPalette::AlternateBase));
                }
            }
        }
        if (m_image.isNull()) {
            return;
        }
        QSize target = m_image.size();
        if (target.width() > width() || target.height() > height()) {
            target.scale(QSize(width(), height()), Qt::KeepAspectRatio);
        }
        const QPoint topLeft((width() - target.width()) / 2, (height() - target.height()) / 2);
        painter.drawImage(QRect(topLeft, target), m_image);
    }

private:
    QImage m_image;
};

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
    : QWidget(parent),
      m_viewModel(create_scope<InspectorViewModel>(
          context.documents(), context.selection(), context.assets(), context.events()))
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setFrameShape(QFrame::NoFrame);
    m_scroll = scroll;
    auto* container = new QWidget();
    container->setObjectName(QStringLiteral("inspectorContent"));
    m_layout = new QVBoxLayout(container);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(8);
    scroll->setWidget(container);
    outer->addWidget(scroll);

    m_viewModelSubscription = ScopedConnection(
        m_viewModel->changed, m_viewModel->changed.connect([this]() { onViewModelChanged(); }));
    refresh();
}

InspectorPanel::~InspectorPanel()
{
    for (auto* editor : findChildren<EditorJsonWidget*>()) editor->disconnect(this);
    m_viewModel->endInteractiveEdit();
}

void InspectorPanel::onViewModelChanged()
{
    if (m_editing) {
        return;
    }
    m_browserPath.clear();
    refresh();
}

void InspectorPanel::inspectPath(const QString& path)
{
    m_browserPath = path;
    refresh();
}

void InspectorPanel::refresh()
{
    if (m_filter) {
        m_filterText = m_filter->text();
        m_filterFocused = m_filter->hasFocus();
    }
    if (m_scroll) {
        m_scrollPosition = m_scroll->verticalScrollBar()->value();
    }
    const auto restoreViewState = [this]() {
        if (m_scrollPosition <= 0 && !m_filterFocused) {
            return;
        }
        QTimer::singleShot(0, this, [this]() {
            if (m_scroll && m_scrollPosition > 0) {
                m_scroll->verticalScrollBar()->setValue(m_scrollPosition);
            }
            if (m_filterFocused && m_filter) {
                m_filter->setFocus(Qt::OtherFocusReason);
            }
        });
    };

    QLayoutItem* child = nullptr;
    while ((child = m_layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    const InspectorAssetModel& assetModel = m_viewModel->asset();
    if (assetModel.valid) {
        const AssetBrowserEntry& asset = assetModel.asset;
        const QString path = ToQString(asset.path);
        const QFileInfo info(path);
        auto* heading = new QWidget(this);
        auto* headingLayout = new QHBoxLayout(heading);
        headingLayout->setContentsMargins(0, 0, 0, 4);
        auto* icon = new QLabel(heading);
        icon->setObjectName(QStringLiteral("inspectorObjectIcon"));
        icon->setFixedSize(40, 40);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(editorAssetIcon(ToQString(asset.type), path).pixmap(28, 28));
        headingLayout->addWidget(icon);
        auto* title = new QLabel(ToQString(asset.name), heading);
        title->setObjectName(QStringLiteral("inspectorAssetName"));
        title->setWordWrap(true);
        title->setTextFormat(Qt::PlainText);
        headingLayout->addWidget(title, 1);
        auto* options = new QToolButton(heading);
        options->setIcon(editorThemedIcon(QStringLiteral("ellipsis.svg")));
        options->setToolTip(tr("Asset actions"));
        options->setPopupMode(QToolButton::InstantPopup);
        auto* menu = new QMenu(options);
        menu->addAction(tr("Show in Explorer"), this, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        menu->addAction(tr("Copy Path"), this, [path]() { QApplication::clipboard()->setText(path); });
        menu->addAction(tr("Copy GUID"), this, [guid = asset.uuid]() {
            QApplication::clipboard()->setText(QString::number(static_cast<qulonglong>(guid)));
        });
        options->setMenu(menu);
        headingLayout->addWidget(options);
        m_layout->addWidget(heading);

        auto* meta = new QLabel(tr("%1  |  %2 KB").arg(ToQString(asset.type))
            .arg(info.size() / 1024.0, 0, 'f', 1), this);
        meta->setObjectName(QStringLiteral("inspectorAssetMeta"));
        meta->setToolTip(path);
        m_layout->addWidget(meta);

        QImage image(ToQString(asset.path));
        if (!image.isNull()) {
            meta->setText(meta->text() + tr("  |  %1 x %2").arg(image.width()).arg(image.height()));
            auto* preview = new AssetImagePreview(std::move(image), this);
            m_layout->addWidget(preview);
        }

        auto* detailsToggle = new QToolButton(this);
        detailsToggle->setText(tr("Technical details"));
        detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        detailsToggle->setArrowType(Qt::RightArrow);
        detailsToggle->setCheckable(true);
        auto* details = new QGroupBox(this);
        details->hide();
        connect(detailsToggle, &QToolButton::toggled, details, [details, detailsToggle](bool open) {
            details->setVisible(open);
            detailsToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        });
        auto* detailsLayout = new QFormLayout(details);
        const auto addDetailRow = [details, detailsLayout](const QString& label, const QString& value) {
            auto* valueLabel = new QLabel(value, details);
            valueLabel->setWordWrap(true);
            valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            detailsLayout->addRow(label, valueLabel);
        };
        addDetailRow(tr("Path"), ToQString(asset.path));
        addDetailRow(tr("GUID"), QString::number(static_cast<qulonglong>(asset.uuid)));

        if (assetModel.hasImportSettings) {
            const AssetImportSettings& importSettings = assetModel.importSettings;
            auto* importGroup = new QGroupBox(tr("Import Settings"), this);
            auto* importLayout = new QVBoxLayout(importGroup);
            addDetailRow(tr("Importer"), ToQString(importSettings.importer));
            auto* settingsEditor = new EditorJsonWidget(importSettings.settings, importGroup);
            importLayout->addWidget(settingsEditor);
            auto* apply = new QPushButton(tr("Apply"), importGroup);
            apply->setObjectName(QStringLiteral("projectPrimaryButton"));
            apply->setEnabled(false);
            auto* revert = new QPushButton(tr("Revert"), importGroup);
            revert->setEnabled(false);
            const Json originalSettings = importSettings.settings;
            connect(settingsEditor, &EditorJsonWidget::valueChanged, importGroup,
                [settingsEditor, originalSettings, apply, revert]() {
                    const bool changed = settingsEditor->value() != originalSettings;
                    apply->setEnabled(changed);
                    revert->setEnabled(changed);
                });
            connect(revert, &QPushButton::clicked, settingsEditor,
                [settingsEditor, originalSettings, apply, revert]() {
                    settingsEditor->setValue(originalSettings);
                    apply->setEnabled(false);
                    revert->setEnabled(false);
                });
            connect(apply, &QPushButton::clicked, this, [this, settingsEditor]() {
                m_viewModel->applyAssetSettings(settingsEditor->value());
            });
            auto* buttons = new QHBoxLayout();
            buttons->addStretch();
            buttons->addWidget(revert);
            buttons->addWidget(apply);
            importLayout->addLayout(buttons);
            m_layout->addWidget(importGroup);
        }
        m_layout->addWidget(detailsToggle);
        m_layout->addWidget(details);
        m_layout->addStretch();
        restoreViewState();
        return;
    }

    const InspectorEntityModel& entity = m_viewModel->entity();
    if (!entity.valid) {
        if (!m_browserPath.isEmpty()) {
            const QFileInfo info(m_browserPath);
            auto* icon = new QLabel(this);
            icon->setAlignment(Qt::AlignCenter);
            icon->setPixmap(editorAssetIcon({}, m_browserPath, info.isDir()).pixmap(64, 64));
            icon->setMinimumHeight(88);
            m_layout->addWidget(icon);
            auto* title = new QLabel(info.fileName(), this);
            title->setObjectName(QStringLiteral("inspectorAssetName"));
            title->setTextFormat(Qt::PlainText);
            title->setWordWrap(true);
            m_layout->addWidget(title);
            auto* meta = new QLabel(info.isDir()
                ? tr("Folder  |  %1 items").arg(QDir(m_browserPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size())
                : tr("%1 file  |  %2 KB").arg(info.suffix().toUpper()).arg(info.size() / 1024.0, 0, 'f', 1), this);
            meta->setObjectName(QStringLiteral("inspectorAssetMeta"));
            meta->setToolTip(m_browserPath);
            m_layout->addWidget(meta);
            auto* reveal = new QPushButton(tr("Show in Explorer"), this);
            connect(reveal, &QPushButton::clicked, this, [path = m_browserPath]() {
                const QFileInfo file(path);
                QDesktopServices::openUrl(QUrl::fromLocalFile(file.isDir() ? path : file.absolutePath()));
            });
            m_layout->addWidget(reveal);
            m_layout->addStretch();
            return;
        }
        auto* label = new QLabel(tr("Select an object or asset to inspect."), this);
        label->setObjectName(QStringLiteral("inspectorObjectSubtitle"));
        label->setWordWrap(true);
        m_layout->addWidget(label);
        m_layout->addStretch();
        restoreViewState();
        return;
    }

    auto* selectionBar = new QWidget(this);
    selectionBar->setObjectName(QStringLiteral("inspectorSelectionBar"));
    auto* selectionLayout = new QHBoxLayout(selectionBar);
    selectionLayout->setContentsMargins(2, 2, 2, 4);
    selectionLayout->setSpacing(6);

    auto* objectIcon = new QLabel(selectionBar);
    objectIcon->setObjectName(QStringLiteral("inspectorObjectIcon"));
    objectIcon->setFixedSize(26, 26);
    objectIcon->setAlignment(Qt::AlignCenter);
    objectIcon->setPixmap(editorThemedIcon(QStringLiteral("box.svg")).pixmap(16, 16));
    selectionLayout->addWidget(objectIcon);

    m_nameEdit = new QLineEdit(ToQString(entity.name), selectionBar);
    m_nameEdit->setObjectName(QStringLiteral("inspectorObjectName"));
    m_nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this]() {
        onRenameEntity(m_nameEdit->text());
    });
    selectionLayout->addWidget(m_nameEdit, 1);

    auto* objectMenu = new QMenu(selectionBar);
    QAction* removeEntityAction = objectMenu->addAction(tr("Remove GameObject"));
    connect(removeEntityAction, &QAction::triggered, this, [this]() {
        m_viewModel->deleteSelectedEntity();
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
    filter->setClearButtonEnabled(true);
    if (!m_filterText.isEmpty()) {
        filter->setText(m_filterText);
    }
    m_filter = filter;
    m_layout->addWidget(filter);

    std::set<String> commonTypes(entity.commonNativeTypes.begin(), entity.commonNativeTypes.end());

    if (!entity.multiSelection.empty()) {
        auto* multiLabel = new QLabel(tr("%1 GameObjects selected").arg(entity.multiSelection.size() + 1), this);
        multiLabel->setObjectName(QStringLiteral("inspectorMultiSelection"));
        m_layout->addWidget(multiLabel);
    }

    DynamicArray<QWidget*> componentSections;

    const auto addComponentSection = [&](const InspectorComponentModel& component,
                                          Size_t index, bool managed) {
        if (!managed && component.typeName == "IDComponent") {
            return;
        }
        if (!entity.multiSelection.empty() && !managed &&
            commonTypes.find(component.typeName) == commonTypes.end()) {
            return;
        }
        const QString componentName = inspectorSectionName(ToQString(component.typeName));
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
        headerRowLayout->setContentsMargins(2, 1, 3, 1);
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
        connect(pasteAction, &QAction::triggered, this, [this, index, managed]() {
            commitComponentValue(index, managed, m_componentClipboardValue);
            QTimer::singleShot(0, this, [this]() { refresh(); });
        });
        connect(sectionMenu, &QMenu::aboutToShow, this, [this, pasteAction, typeName = component.typeName]() {
            pasteAction->setEnabled(!typeName.empty() && m_componentClipboardType == typeName);
        });
        connect(copyAction, &QAction::triggered, this, [this, index, managed]() {
            const auto& current = m_viewModel->entity();
            const auto& components = managed ? current.managedComponents : current.nativeComponents;
            if (index >= components.size()) return;
            m_componentClipboardType = components[index].typeName;
            m_componentClipboardValue = components[index].value;
        });
        sectionMenu->addSeparator();
        QAction* resetAction = sectionMenu->addAction(tr("Reset to Default"));
        Json defaultValue;
        const bool hasDefault = m_viewModel->defaultComponentValue(component.typeName, defaultValue);
        resetAction->setEnabled(hasDefault);
        if (hasDefault) {
            connect(resetAction, &QAction::triggered, this,
                    [this, index, managed, defaultValue]() {
                        commitComponentValue(index, managed, defaultValue);
                        QTimer::singleShot(0, this, [this]() { refresh(); });
                    });
        }
        if (!managed) {
            sectionMenu->addSeparator();
            QAction* moveUpAction = sectionMenu->addAction(tr("Move Up"));
            moveUpAction->setEnabled(index > 0);
            connect(moveUpAction, &QAction::triggered, this, [this, index]() {
                if (m_viewModel->moveComponent(index, -1) &&
                    index > 0 && index < m_componentExpanded.size()) {
                    const bool wasExpanded = m_componentExpanded[index];
                    m_componentExpanded[index] = m_componentExpanded[index - 1];
                    m_componentExpanded[index - 1] = wasExpanded;
                }
            });
            QAction* moveDownAction = sectionMenu->addAction(tr("Move Down"));
            connect(moveDownAction, &QAction::triggered, this, [this, index]() {
                if (m_viewModel->moveComponent(index, 1) &&
                    index + 1 < m_componentExpanded.size()) {
                    const bool wasExpanded = m_componentExpanded[index];
                    m_componentExpanded[index] = m_componentExpanded[index + 1];
                    m_componentExpanded[index + 1] = wasExpanded;
                }
            });
        }
        sectionMenu->addSeparator();
        QAction* removeAction = sectionMenu->addAction(
            managed ? tr("Remove Managed Component") : tr("Remove Component"));
        removeAction->setEnabled(managed || component.typeName != "TransformComponent");
        connect(removeAction, &QAction::triggered, this, [this, index, managed]() {
            if (managed) {
                if (index < m_managedComponentExpanded.size()) {
                    m_managedComponentExpanded.erase(
                        m_managedComponentExpanded.begin() + static_cast<std::ptrdiff_t>(index));
                }
                m_viewModel->removeComponent(index, true);
            } else {
                if (index < m_componentExpanded.size()) {
                    m_componentExpanded.erase(
                        m_componentExpanded.begin() + static_cast<std::ptrdiff_t>(index));
                }
                m_viewModel->removeComponent(index, false);
            }
        });

        auto* menuBtn = new QToolButton(headerRow);
        menuBtn->setObjectName(QStringLiteral("inspectorSectionMenu"));
        menuBtn->setIcon(editorIcon(QStringLiteral("ellipsis.svg")));
        menuBtn->setIconSize(QSize(16, 16));
        menuBtn->setToolTip(tr("Component options"));
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
        bodyLayout->setContentsMargins(4, 5, 4, 2);
        bodyLayout->setSpacing(4);

        if (!managed && component.hasCustomUI) {
            const String typeName = component.typeName;
            EditorRemoteWidget::PropertyContext property;
            property.value = component.value;
            property.fields = component.reflectedFields;
            property.onChanged = [this, index, managed](const Json& value) {
                commitComponentValue(index, managed, value);
            };
            auto* remote = new EditorRemoteWidget(
                [this, typeName](Json& out) {
                    return m_viewModel->getCustomInspectorUI(typeName, out);
                },
                [this, typeName](const String& controlId, const String& eventName,
                                 const Json& value) {
                    m_viewModel->dispatchInspectorEvent(typeName, controlId, eventName, value);
                },
                std::move(property),
                body);
            bodyLayout->addWidget(remote);
        } else {
            auto* editor = component.reflectedFields.empty()
                ? new EditorJsonWidget(component.value, body)
                : new EditorJsonWidget(component.value, component.reflectedFields, entity.assets, body);
            connect(editor, &EditorJsonWidget::interactiveEditStarted, this,
                [this]() { m_viewModel->beginInteractiveEdit(); });
            connect(editor, &EditorJsonWidget::interactiveEditFinished, this,
                [this]() { m_viewModel->endInteractiveEdit(); });
            connect(editor, &EditorJsonWidget::fieldChanged, this,
                    [this, index, managed, editor](const QString& path) {
                commitComponentValue(index, managed, editor->value(), path);
            });
            bodyLayout->addWidget(editor);
        }

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

    for (Size_t i = 0; i < entity.nativeComponents.size(); ++i) {
        addComponentSection(entity.nativeComponents[i], i, false);
    }
    for (Size_t i = 0; i < entity.managedComponents.size(); ++i) {
        addComponentSection(entity.managedComponents[i], i, true);
    }

    m_componentExpanded.resize(entity.nativeComponents.size(), true);
    m_managedComponentExpanded.resize(entity.managedComponents.size(), true);

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
    UnorderedSet<String> existingTypes;
    for (const InspectorComponentModel& component : entity.nativeComponents) {
        existingTypes.insert(component.typeName);
    }
    DynamicArray<QAction*> typeActions;
    for (const String& typeName : m_viewModel->componentTypes()) {
        if (existingTypes.contains(typeName)) {
            continue;
        }
        QAction* action = addMenu->addAction(ToQString(typeName));
        typeActions.push_back(action);
        connect(action, &QAction::triggered, this, [this, typeName]() {
            m_viewModel->addComponent(typeName);
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
    restoreViewState();
}

void InspectorPanel::onRenameEntity(const QString& name)
{
    m_viewModel->renameEntity(ToDodoeString(name));
}

void InspectorPanel::commitComponentValue(Size_t index, bool managed, const Json& value,
                                          const QString& fieldPath)
{
    m_editing = true;
    m_viewModel->commitComponentValue(
        index, managed, value, fieldPath.isEmpty() ? String() : ToDodoeString(fieldPath));
    m_editing = false;
}

} // namespace cakery
