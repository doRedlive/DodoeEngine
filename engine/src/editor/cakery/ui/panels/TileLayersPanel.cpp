// do@Redlive

#include "TileLayersPanel.h"

#include "cakery/ui/EditorIcons.h"
#include "cakery/ui/EditorWorkspaceContext.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace cakery {

namespace {

class LayerRowWidget final : public QWidget {
public:
    explicit LayerRowWidget(QString name, bool visible, float opacity, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(26);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(2, 0, 4, 0);
        layout->setSpacing(4);

        m_eye = new QToolButton(this);
        m_eye->setAutoRaise(true);
        m_eye->setIcon(editorIcon(visible ? QStringLiteral("eye.svg") : QStringLiteral("eye-off.svg")));
        m_eye->setToolTip(QObject::tr("Toggle layer visibility"));
        m_eye->setFixedSize(18, 18);
        layout->addWidget(m_eye);

        m_name = new QLabel(name, this);
        m_name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(m_name, 1);

        m_opacity = new QSlider(Qt::Horizontal, this);
        m_opacity->setRange(0, 100);
        m_opacity->setValue(static_cast<int>(opacity * 100.0f));
        m_opacity->setFixedWidth(72);
        m_opacity->setToolTip(QObject::tr("Layer opacity"));
        layout->addWidget(m_opacity);

        connect(m_eye, &QToolButton::clicked, this, [this]() {
            if (onVisibleToggle) onVisibleToggle(!m_visible);
        });
        connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
            if (onOpacityChanged) onOpacityChanged(static_cast<float>(value) / 100.0f);
        });
    }

    void setVisibleState(bool visible)
    {
        m_visible = visible;
        m_eye->setIcon(editorIcon(visible ? QStringLiteral("eye.svg") : QStringLiteral("eye-off.svg")));
    }

    std::function<void(bool)> onVisibleToggle;
    std::function<void(float)> onOpacityChanged;
    std::function<void()> onRename;

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && onRename) {
            onRename();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    QToolButton* m_eye = nullptr;
    QLabel* m_name = nullptr;
    QSlider* m_opacity = nullptr;
    bool m_visible = true;
};

} // namespace

TileLayersPanel::TileLayersPanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent)
    , m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 0);
    toolbarLayout->setSpacing(4);

    auto* addButton = new QToolButton(toolbar);
    addButton->setText(tr("Add"));
    addButton->setAutoRaise(true);
    connect(addButton, &QToolButton::clicked, this, &TileLayersPanel::onAddLayer);
    toolbarLayout->addWidget(addButton);

    auto* removeButton = new QToolButton(toolbar);
    removeButton->setText(tr("Remove"));
    removeButton->setAutoRaise(true);
    connect(removeButton, &QToolButton::clicked, this, &TileLayersPanel::onRemoveLayer);
    toolbarLayout->addWidget(removeButton);

    auto* upButton = new QToolButton(toolbar);
    upButton->setText(tr("Up"));
    upButton->setAutoRaise(true);
    connect(upButton, &QToolButton::clicked, this, [this]() { onMoveLayer(true); });
    toolbarLayout->addWidget(upButton);

    auto* downButton = new QToolButton(toolbar);
    downButton->setText(tr("Down"));
    downButton->setAutoRaise(true);
    connect(downButton, &QToolButton::clicked, this, [this]() { onMoveLayer(false); });
    toolbarLayout->addWidget(downButton);

    toolbarLayout->addStretch();
    layout->addWidget(toolbar);

    m_list = new QListWidget(this);
    m_list->setSpacing(1);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) {
            onActivateLayer(item->data(Qt::UserRole).toULongLong());
        }
    });

    m_selectionConnection = m_context.session().selection().subscribe([this]() { refresh(); });
    m_documentConnection = m_context.session().documentModel().subscribe([this]() { refresh(); });
    m_tileModeConnection = ScopedConnection(
        m_context.session().tileEditModeChanged,
        m_context.session().tileEditModeChanged.connect([this](bool) { refresh(); }));

    refresh();
}

TileLayersPanel::~TileLayersPanel()
{
}

void TileLayersPanel::refresh()
{
    m_list->clear();
    nlohmann::json state;
    if (!m_context.session().queryTilemapState(std::string(), state)) {
        m_state = nlohmann::json();
        return;
    }
    m_state = std::move(state);

    const std::uint64_t activeLayer = m_state.contains("active_layer")
        ? m_state["active_layer"].get<std::uint64_t>()
        : 0;
    QListWidgetItem* activeItem = nullptr;
    if (m_state.contains("layers") && m_state["layers"].is_array()) {
        for (const auto& entry : m_state["layers"]) {
            const std::uint64_t uuid = entry.value("uuid", std::uint64_t(0));
            const std::string name = entry.value("name", std::string());
            const bool visible = entry.value("visible", true);
            const float opacity = entry.value("opacity", 1.0f);

            auto* item = new QListWidgetItem(m_list);
            item->setData(Qt::UserRole, static_cast<qulonglong>(uuid));
            auto* row = new LayerRowWidget(QString::fromStdString(name), visible, opacity);
            row->onVisibleToggle = [this, uuid](bool v) { onToggleVisible(uuid, v); };
            row->onOpacityChanged = [this, uuid](float o) { onOpacityChanged(uuid, o); };
            row->onRename = [this, uuid, name]() {
                onRenameLayer(uuid, QString::fromStdString(name));
            };
            item->setSizeHint(row->sizeHint());
            m_list->setItemWidget(item, row);
            if (uuid == activeLayer) {
                activeItem = item;
            }
        }
    }
    if (activeItem) {
        m_list->setCurrentItem(activeItem);
    }
}

void TileLayersPanel::onAddLayer()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Layer"), tr("Layer name:"),
                                               QLineEdit::Normal, tr("Layer"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    m_context.session().execute(EditorCommandMessage{"tilemap.layer_add", name.trimmed().toStdString()});
}

void TileLayersPanel::onRemoveLayer()
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const std::uint64_t uuid = selected.first()->data(Qt::UserRole).toULongLong();
    m_context.session().execute(EditorCommandMessage{"tilemap.layer_remove", std::to_string(uuid)});
}

void TileLayersPanel::onMoveLayer(bool up)
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const std::uint64_t uuid = selected.first()->data(Qt::UserRole).toULongLong();
    m_context.session().execute(EditorCommandMessage{
        "tilemap.layer_move", std::to_string(uuid) + "," + (up ? "up" : "down")});
}

void TileLayersPanel::onActivateLayer(std::uint64_t uuid)
{
    m_context.session().execute(EditorCommandMessage{"tilemap.layer_active", std::to_string(uuid)});
}

void TileLayersPanel::onToggleVisible(std::uint64_t uuid, bool visible)
{
    m_context.session().execute(EditorCommandMessage{
        "tilemap.layer_visible", std::to_string(uuid) + "," + (visible ? "1" : "0")});
}

void TileLayersPanel::onOpacityChanged(std::uint64_t uuid, float opacity)
{
    m_context.session().execute(EditorCommandMessage{
        "tilemap.layer_opacity",
        std::to_string(uuid) + "," + QString::number(opacity, 'f', 2).toStdString()});
}

void TileLayersPanel::onRenameLayer(std::uint64_t uuid, const QString& currentName)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Layer"), tr("Layer name:"),
                                               QLineEdit::Normal, currentName, &ok);
    if (!ok || name.trimmed().isEmpty() || name.trimmed() == currentName) {
        return;
    }
    m_context.session().execute(EditorCommandMessage{
        "tilemap.layer_rename", std::to_string(uuid) + "," + name.trimmed().toStdString()});
}

} // namespace cakery
