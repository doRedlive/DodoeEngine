// do@Redlive

#include "dopch.h"

#include "TileLayersPanel.h"

#include "presentation/tilelayers/TileLayersViewModel.h"

#include "ui/common/EditorIcons.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/QtStringInterop.h"

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

    m_viewModel = create_scope<TileLayersViewModel>(m_context.session());
    m_viewModelConnection = ScopedConnection(
        m_viewModel->changed,
        m_viewModel->changed.connect([this]() { onViewModelChanged(); }));

    onViewModelChanged();
}

TileLayersPanel::~TileLayersPanel()
{
}

void TileLayersPanel::onViewModelChanged()
{
    refresh();
}

void TileLayersPanel::refresh()
{
    m_list->clear();
    const TileLayersSnapshot& snapshot = m_viewModel->snapshot();
    if (!snapshot.valid) {
        return;
    }

    QListWidgetItem* activeItem = nullptr;
    for (const TileLayerRow& row : snapshot.layers) {
        auto* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, static_cast<qulonglong>(row.uuid));
        auto* rowWidget = new LayerRowWidget(ToQString(row.name), row.visible, row.opacity);
        const UInt64 uuid = row.uuid;
        const String name = row.name;
        rowWidget->onVisibleToggle = [this, uuid](bool v) { onToggleVisible(uuid, v); };
        rowWidget->onOpacityChanged = [this, uuid](float o) { onOpacityChanged(uuid, o); };
        rowWidget->onRename = [this, uuid, name]() {
            onRenameLayer(uuid, ToQString(name));
        };
        item->setSizeHint(rowWidget->sizeHint());
        m_list->setItemWidget(item, rowWidget);
        if (row.uuid == snapshot.activeLayer) {
            activeItem = item;
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
    m_viewModel->addLayer(ToDodoeString(name.trimmed()));
}

void TileLayersPanel::onRemoveLayer()
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const UInt64 uuid = selected.first()->data(Qt::UserRole).toULongLong();
    m_viewModel->removeLayer(uuid);
}

void TileLayersPanel::onMoveLayer(bool up)
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    const UInt64 uuid = selected.first()->data(Qt::UserRole).toULongLong();
    m_viewModel->moveLayer(uuid, up);
}

void TileLayersPanel::onActivateLayer(UInt64 uuid)
{
    m_viewModel->activateLayer(uuid);
}

void TileLayersPanel::onToggleVisible(UInt64 uuid, bool visible)
{
    m_viewModel->setLayerVisible(uuid, visible);
}

void TileLayersPanel::onOpacityChanged(UInt64 uuid, float opacity)
{
    m_viewModel->setLayerOpacity(uuid, opacity);
}

void TileLayersPanel::onRenameLayer(UInt64 uuid, const QString& currentName)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename Layer"), tr("Layer name:"),
                                               QLineEdit::Normal, currentName, &ok);
    if (!ok || name.trimmed().isEmpty() || name.trimmed() == currentName) {
        return;
    }
    m_viewModel->renameLayer(uuid, ToDodoeString(name.trimmed()));
}

} // namespace cakery
