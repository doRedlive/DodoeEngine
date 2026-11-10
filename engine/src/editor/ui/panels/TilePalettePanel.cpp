// do@Redlive

#include "dopch.h"

#include "ui/panels/TilePalettePanel.h"

#include "ui/common/EditorIcons.h"
#include "ui/common/EditorWorkspaceContext.h"
#include "ui/common/QtStringInterop.h"
#include "core/document/EditorDocument.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string_view>

namespace cakery {

namespace {

class SizeFieldsDialog final : public QDialog {
public:
    explicit SizeFieldsDialog(const QString& title, const QString& widthLabel, const QString& heightLabel,
                              int defaultWidth, int defaultHeight, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(title);
        auto* form = new QFormLayout(this);
        m_width = new QSpinBox(this);
        m_width->setRange(1, 10000);
        m_width->setValue(defaultWidth);
        m_height = new QSpinBox(this);
        m_height->setRange(1, 10000);
        m_height->setValue(defaultHeight);
        form->addRow(widthLabel, m_width);
        form->addRow(heightLabel, m_height);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }

    int width() const { return m_width->value(); }
    int height() const { return m_height->value(); }

private:
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
};

Optional<TilemapCreateCommand> createTilemapDialog(QWidget* parent)
{
    bool ok = false;
    const QString name = QInputDialog::getText(parent, QObject::tr("New Tilemap"), QObject::tr("Name:"),
                                               QLineEdit::Normal, QObject::tr("Tilemap"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return std::nullopt;
    }

    SizeFieldsDialog mapSize(QObject::tr("Tilemap Size"), QObject::tr("Map width (tiles):"),
                             QObject::tr("Map height (tiles):"), 40, 30, parent);
    if (mapSize.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    SizeFieldsDialog tileSize(QObject::tr("Tile Size"), QObject::tr("Tile width (px):"),
                              QObject::tr("Tile height (px):"), 16, 16, parent);
    if (tileSize.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    TilemapCreateCommand command;
    command.name = ToDodoeString(name.trimmed());
    command.width = static_cast<UInt32>(mapSize.width());
    command.height = static_cast<UInt32>(mapSize.height());
    command.tileWidth = static_cast<UInt32>(tileSize.width());
    command.tileHeight = static_cast<UInt32>(tileSize.height());
    return command;
}

Optional<TilemapAddTilesetCommand> addTilesetDialog(QWidget* parent)
{
    const QString image = QFileDialog::getOpenFileName(
        parent, QObject::tr("Select Tileset Image"), QString(),
        QObject::tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (image.isEmpty()) {
        return std::nullopt;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Tileset Tile Size"));
    auto* form = new QFormLayout(&dialog);
    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 1024);
    widthSpin->setValue(16);
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 1024);
    heightSpin->setValue(16);
    auto* marginSpin = new QSpinBox(&dialog);
    marginSpin->setRange(0, 256);
    marginSpin->setValue(0);
    auto* spacingSpin = new QSpinBox(&dialog);
    spacingSpin->setRange(0, 256);
    spacingSpin->setValue(0);
    form->addRow(QObject::tr("Tile width (px):"), widthSpin);
    form->addRow(QObject::tr("Tile height (px):"), heightSpin);
    form->addRow(QObject::tr("Margin (px):"), marginSpin);
    form->addRow(QObject::tr("Spacing (px):"), spacingSpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    TilemapAddTilesetCommand command;
    command.image = ToDodoeString(image);
    command.tileWidth = static_cast<UInt32>(widthSpin->value());
    command.tileHeight = static_cast<UInt32>(heightSpin->value());
    command.margin = static_cast<UInt32>(marginSpin->value());
    command.spacing = static_cast<UInt32>(spacingSpin->value());
    return command;
}

Optional<TilemapEditTilesetCommand> editTilesetDialog(QWidget* parent, const Json& entry)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Edit Tileset"));
    auto* form = new QFormLayout(&dialog);
    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 1024);
    widthSpin->setValue(entry.value("tile_width", 16));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 1024);
    heightSpin->setValue(entry.value("tile_height", 16));
    auto* marginSpin = new QSpinBox(&dialog);
    marginSpin->setRange(0, 256);
    marginSpin->setValue(entry.value("margin", 0));
    auto* spacingSpin = new QSpinBox(&dialog);
    spacingSpin->setRange(0, 256);
    spacingSpin->setValue(entry.value("spacing", 0));
    form->addRow(QObject::tr("Tile width (px):"), widthSpin);
    form->addRow(QObject::tr("Tile height (px):"), heightSpin);
    form->addRow(QObject::tr("Margin (px):"), marginSpin);
    form->addRow(QObject::tr("Spacing (px):"), spacingSpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    TilemapEditTilesetCommand command;
    command.assetId = entry.value("asset_id", UInt64(0));
    command.tileWidth = static_cast<UInt32>(widthSpin->value());
    command.tileHeight = static_cast<UInt32>(heightSpin->value());
    command.margin = static_cast<UInt32>(marginSpin->value());
    command.spacing = static_cast<UInt32>(spacingSpin->value());
    return command;
}

} // namespace

TileTilesetView::TileTilesetView(QPixmap image, UInt32 tileWidth, UInt32 tileHeight,
                                  UInt32 columns, UInt32 firstGid,
                                  UInt32 margin, UInt32 spacing, QWidget* parent)
    : QWidget(parent)
    , m_image(std::move(image))
    , m_tileWidth(tileWidth)
    , m_tileHeight(tileHeight)
    , m_columns(columns)
    , m_firstGid(firstGid)
    , m_margin(margin)
    , m_spacing(spacing)
{
    setMouseTracking(true);
}

int TileTilesetView::cellAtX(const QPoint& pos) const
{
    const int step = static_cast<int>(m_tileWidth + m_spacing);
    if (step <= 0) return 0;
    const int cell = (static_cast<int>(pos.x()) - static_cast<int>(m_margin)) / step;
    return cell < 0 ? 0 : cell;
}

int TileTilesetView::cellAtY(const QPoint& pos) const
{
    const int step = static_cast<int>(m_tileHeight + m_spacing);
    if (step <= 0) return 0;
    const int cell = (static_cast<int>(pos.y()) - static_cast<int>(m_margin)) / step;
    return cell < 0 ? 0 : cell;
}

QSize TileTilesetView::minimumSizeHint() const
{
    return m_image.isNull() ? QSize(160, 160) : m_image.size();
}

QSize TileTilesetView::sizeHint() const
{
    return minimumSizeHint();
}

void TileTilesetView::setSelection(int cellX, int cellY, int cellW, int cellH)
{
    m_selectionCell = QPoint(cellX, cellY);
    m_selectionW = cellW;
    m_selectionH = cellH;
    update();
}

QRect TileTilesetView::selectionRect() const
{
    const int stepX = static_cast<int>(m_tileWidth + m_spacing);
    const int stepY = static_cast<int>(m_tileHeight + m_spacing);
    const int x = static_cast<int>(m_margin) + m_selectionCell.x() * stepX;
    const int y = static_cast<int>(m_margin) + m_selectionCell.y() * stepY;
    const int w = m_selectionW * static_cast<int>(m_tileWidth) + (m_selectionW - 1) * static_cast<int>(m_spacing);
    const int h = m_selectionH * static_cast<int>(m_tileHeight) + (m_selectionH - 1) * static_cast<int>(m_spacing);
    return QRect(x, y, w, h);
}

void TileTilesetView::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#222222"));

    if (m_image.isNull()) {
        painter.setPen(QColor("#A0A0A0"));
        painter.drawText(rect(), Qt::AlignCenter, QObject::tr("Tileset image missing"));
        return;
    }

    painter.drawPixmap(0, 0, m_image);

    const int stepX = static_cast<int>(m_tileWidth + m_spacing);
    const int stepY = static_cast<int>(m_tileHeight + m_spacing);
    const int gridW = static_cast<int>(m_image.width());
    const int gridH = static_cast<int>(m_image.height());
    const int marginX = static_cast<int>(m_margin);
    const int marginY = static_cast<int>(m_margin);
    if (stepX > 0 && stepY > 0) {
        painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
        for (UInt32 x = 1; x < m_columns; ++x) {
            const int px = marginX + static_cast<int>(x) * stepX - static_cast<int>(m_spacing) / 2;
            if (px <= 0 || px >= gridW) continue;
            painter.drawLine(px, 0, px, gridH);
        }
        const UInt32 rows = tileCount() / (m_columns == 0 ? 1 : m_columns);
        for (UInt32 y = 1; y < rows; ++y) {
            const int py = marginY + static_cast<int>(y) * stepY - static_cast<int>(m_spacing) / 2;
            if (py <= 0 || py >= gridH) continue;
            painter.drawLine(0, py, gridW, py);
        }
    }

    const QRect selection = selectionRect();
    if (selection.isValid()) {
        painter.fillRect(selection, QColor(255, 255, 255, 50));
        painter.setPen(QPen(QColor("#4CAF50"), 2));
        painter.drawRect(selection.adjusted(0, 0, -1, -1));
    }
}

void TileTilesetView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_image.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int cx = cellAtX(event->position().toPoint());
    const int cy = cellAtY(event->position().toPoint());
    m_selectStart = QPoint(cx, cy);
    m_selectEnd = m_selectStart;
    m_selecting = true;
    setSelection(cx, cy, 1, 1);
    event->accept();
}

void TileTilesetView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_selecting) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const int cx = cellAtX(event->position().toPoint());
    const int cy = cellAtY(event->position().toPoint());
    m_selectEnd = QPoint(cx, cy);
    const int x0 = std::min(m_selectStart.x(), m_selectEnd.x());
    const int y0 = std::min(m_selectStart.y(), m_selectEnd.y());
    const int w = std::abs(m_selectEnd.x() - m_selectStart.x()) + 1;
    const int h = std::abs(m_selectEnd.y() - m_selectStart.y()) + 1;
    setSelection(x0, y0, w, h);
    event->accept();
}

void TileTilesetView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_selecting || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_selecting = false;
    emitBrush();
    event->accept();
}

void TileTilesetView::emitBrush()
{
    if (!onBrushSelected) {
        return;
    }
    const int x0 = std::min(m_selectStart.x(), m_selectEnd.x());
    const int y0 = std::min(m_selectStart.y(), m_selectEnd.y());
    const int w = std::abs(m_selectEnd.x() - m_selectStart.x()) + 1;
    const int h = std::abs(m_selectEnd.y() - m_selectStart.y()) + 1;
    DynamicArray<UInt32> gids;
    gids.reserve(static_cast<Size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const UInt32 gid = m_firstGid + static_cast<UInt32>((y0 + y) * static_cast<int>(m_columns) + (x0 + x));
            gids.push_back(gid);
        }
    }
    onBrushSelected(w, h, std::move(gids));
}

TilePalettePanel::TilePalettePanel(EditorWorkspaceContext& context, QWidget* parent)
    : QWidget(parent)
    , m_context(context)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("tilePaletteToolbar"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(18, 18));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolGroup = new QActionGroup(m_toolBar);
    m_toolGroup->setExclusive(true);
    struct TileToolEntry {
        const char* tooltip;
        const char* icon;
        TileToolId tool;
        const char* shortcut;
    };
    const TileToolEntry tools[] = {
        {QT_TR_NOOP("Select"), "tile-select.svg", TileToolId::Select, "S"},
        {QT_TR_NOOP("Brush"), "tile-brush.svg", TileToolId::Brush, "B"},
        {QT_TR_NOOP("Box Fill"), "tile-rect.svg", TileToolId::Rect, "U"},
        {QT_TR_NOOP("Fill"), "tile-fill.svg", TileToolId::Fill, "G"},
        {QT_TR_NOOP("Eraser"), "tile-eraser.svg", TileToolId::Erase, "D"},
        {QT_TR_NOOP("Line"), "tile-line.svg", TileToolId::Line, "L"},
        {QT_TR_NOOP("Picker"), "tile-picker.svg", TileToolId::Picker, "I"},
    };
    for (const TileToolEntry& tool : tools) {
        auto* action = m_toolBar->addAction(editorThemedIcon(QString::fromLatin1(tool.icon)), QString());
        action->setCheckable(true);
        action->setEnabled(false);
        action->setToolTip(tr(tool.tooltip));
        action->setShortcut(QKeySequence::fromString(QString::fromLatin1(tool.shortcut)));
        m_toolGroup->addAction(action);
        if (tool.tool == TileToolId::Select) {
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, toolId = tool.tool]() {
            m_context.session().execute(TilemapToolCommand{toolId});
        });
    }
    layout->addWidget(m_toolBar);

    auto* targetRow = new QWidget(this);
    auto* targetLayout = new QHBoxLayout(targetRow);
    targetLayout->setContentsMargins(4, 0, 4, 0);
    targetLayout->setSpacing(6);
    auto* targetLabel = new QLabel(tr("Active Target"), targetRow);
    targetLayout->addWidget(targetLabel);
    m_targetCombo = new QComboBox(targetRow);
    m_targetCombo->setObjectName(QStringLiteral("tilePaletteTarget"));
    m_targetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    targetLayout->addWidget(m_targetCombo, 1);
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TilePalettePanel::setActiveTarget);
    layout->addWidget(targetRow);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(4, 4, 4, 0);
    toolbarLayout->setSpacing(4);

    m_newTilemapButton = new QToolButton(toolbar);
    m_newTilemapButton->setText(tr("New Tilemap"));
    m_newTilemapButton->setAutoRaise(true);
    connect(m_newTilemapButton, &QToolButton::clicked, this, &TilePalettePanel::onNewTilemap);
    toolbarLayout->addWidget(m_newTilemapButton);

    m_addTilesetButton = new QToolButton(toolbar);
    m_addTilesetButton->setText(tr("Add Tileset"));
    m_addTilesetButton->setAutoRaise(true);
    connect(m_addTilesetButton, &QToolButton::clicked, this, &TilePalettePanel::onAddTileset);
    toolbarLayout->addWidget(m_addTilesetButton);

    m_removeTilesetButton = new QToolButton(toolbar);
    m_removeTilesetButton->setText(tr("Remove Tileset"));
    m_removeTilesetButton->setAutoRaise(true);
    connect(m_removeTilesetButton, &QToolButton::clicked, this, &TilePalettePanel::onRemoveTileset);
    toolbarLayout->addWidget(m_removeTilesetButton);

    m_resizeTilemapButton = new QToolButton(toolbar);
    m_resizeTilemapButton->setText(tr("Resize Map"));
    m_resizeTilemapButton->setAutoRaise(true);
    connect(m_resizeTilemapButton, &QToolButton::clicked, this, &TilePalettePanel::onResizeTilemap);
    toolbarLayout->addWidget(m_resizeTilemapButton);

    m_editTilesetButton = new QToolButton(toolbar);
    m_editTilesetButton->setText(tr("Edit Tileset"));
    m_editTilesetButton->setAutoRaise(true);
    connect(m_editTilesetButton, &QToolButton::clicked, this, &TilePalettePanel::onEditTileset);
    toolbarLayout->addWidget(m_editTilesetButton);
    toolbarLayout->addStretch();
    layout->addWidget(toolbar);

    auto* brushRow = new QWidget(this);
    auto* brushLayout = new QHBoxLayout(brushRow);
    brushLayout->setContentsMargins(4, 0, 4, 0);
    brushLayout->setSpacing(6);
    m_brushLabel = new QLabel(brushRow);
    m_brushLabel->setObjectName(QStringLiteral("tilePaletteBrushInspector"));
    brushLayout->addWidget(m_brushLabel);
    brushLayout->addStretch();

    auto* flipXButton = new QToolButton(brushRow);
    flipXButton->setText(QStringLiteral("H"));
    flipXButton->setAutoRaise(true);
    flipXButton->setToolTip(tr("Flip Brush Horizontally (X)"));
    brushLayout->addWidget(flipXButton);

    auto* flipYButton = new QToolButton(brushRow);
    flipYButton->setText(QStringLiteral("V"));
    flipYButton->setAutoRaise(true);
    flipYButton->setToolTip(tr("Flip Brush Vertically (Y)"));
    brushLayout->addWidget(flipYButton);

    auto* rotateButton = new QToolButton(brushRow);
    rotateButton->setText(QStringLiteral("R"));
    rotateButton->setAutoRaise(true);
    rotateButton->setToolTip(tr("Rotate Brush (Z)"));
    brushLayout->addWidget(rotateButton);

    m_randomBrushButton = new QToolButton(brushRow);
    m_randomBrushButton->setText(tr("Rand"));
    m_randomBrushButton->setAutoRaise(true);
    m_randomBrushButton->setCheckable(true);
    m_randomBrushButton->setToolTip(tr("Random Brush"));
    brushLayout->addWidget(m_randomBrushButton);
    layout->addWidget(brushRow);

    const auto addBrushAction = [this](const QKeySequence& key, EditorOperation command) {
        auto* action = new QAction(this);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, [this, command]() {
            m_context.session().execute(command);
        });
    };
    addBrushAction(QKeySequence(Qt::Key_X), TilemapBrushFlipCommand{true});
    addBrushAction(QKeySequence(Qt::Key_Y), TilemapBrushFlipCommand{false});
    addBrushAction(QKeySequence(Qt::Key_Z), TilemapBrushRotateCommand{});
    addBrushAction(QKeySequence(Qt::Key_Delete), TilemapSelectionDeleteCommand{});
    addBrushAction(QKeySequence::Copy, TilemapSelectionCopyCommand{});
    addBrushAction(QKeySequence::Paste, TilemapSelectionPasteCommand{});

    connect(flipXButton, &QToolButton::clicked, this, [this]() {
        m_context.session().execute(TilemapBrushFlipCommand{true});
    });
    connect(flipYButton, &QToolButton::clicked, this, [this]() {
        m_context.session().execute(TilemapBrushFlipCommand{false});
    });
    connect(rotateButton, &QToolButton::clicked, this, [this]() {
        m_context.session().execute(TilemapBrushRotateCommand{});
    });
    connect(m_randomBrushButton, &QToolButton::toggled, this, [this](bool checked) {
        m_context.session().execute(TilemapRandomBrushCommand{checked});
    });

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #A0A0A0; padding: 4px;"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* content = new QWidget(m_scroll);
    m_tilesetsLayout = new QVBoxLayout(content);
    m_tilesetsLayout->setContentsMargins(4, 4, 4, 4);
    m_tilesetsLayout->setSpacing(8);
    m_tilesetsLayout->addStretch();
    m_scroll->setWidget(content);
    layout->addWidget(m_scroll, 1);

    m_selectionConnection = m_context.session().selection().subscribe([this]() { refresh(); });
    m_documentConnection = m_context.session().documentModel().subscribe([this]() { refresh(); });
    m_tileModeConnection = ScopedConnection(
        m_context.session().tileEditModeChanged,
        m_context.session().tileEditModeChanged.connect([this](bool) { refresh(); }));

    refresh();
}

TilePalettePanel::~TilePalettePanel()
{
    for (TileTilesetView* view : m_tilesetViews) {
        delete view;
    }
    m_tilesetViews.clear();
}

void TilePalettePanel::refresh()
{
    Json state;
    if (!m_context.session().queryTilemapState(String(), state)) {
        m_state = Json();
        rebuildTargetOptions(0);
        updateToolState(false);
        if (m_randomBrushButton) {
            const QSignalBlocker blocker(m_randomBrushButton);
            m_randomBrushButton->setChecked(false);
        }
        if (m_brushLabel) {
            m_brushLabel->setText(tr("Active Brush: None"));
        }
        m_statusLabel->setText(tr("No tilemap selected. Select a tilemap in the Hierarchy, or create a new one."));
        rebuildTilesets();
        return;
    }
    m_state = std::move(state);
    const UInt64 activeUuid = m_state.contains("tilemap")
        ? m_state["tilemap"].value("uuid", UInt64(0))
        : 0;
    rebuildTargetOptions(activeUuid);
    updateToolState(activeUuid != 0);
    if (m_randomBrushButton) {
        const QSignalBlocker blocker(m_randomBrushButton);
        m_randomBrushButton->setChecked(m_state.value("random_brush", false));
    }
    if (m_brushLabel) {
        const Json brush = m_state.contains("brush") && m_state["brush"].is_object()
            ? m_state["brush"] : Json::object();
        const int brushW = brush.value("w", 1);
        const int brushH = brush.value("h", 1);
        const Size_t tileCount = brush.contains("gids") && brush["gids"].is_array()
            ? brush["gids"].size() : 0;
        m_brushLabel->setText(QStringLiteral("Active Brush: %1 x %2 (%3 tiles)")
            .arg(brushW).arg(brushH).arg(static_cast<qulonglong>(tileCount)));
    }
    const String tilemapName = m_state.contains("tilemap")
        ? m_state["tilemap"].value("name", String())
        : String();
    m_statusLabel->setText(QStringLiteral("Tilemap: %1\n%2 tileset(s)")
        .arg(ToQString(tilemapName))
        .arg(static_cast<qulonglong>(m_state["tilesets"].size())));
    rebuildTilesets();
}

void TilePalettePanel::rebuildTargetOptions(UInt64 activeUuid)
{
    if (!m_targetCombo) {
        return;
    }
    const QSignalBlocker blocker(m_targetCombo);
    m_targetCombo->clear();
    m_targetCombo->addItem(tr("No Tilemap"), QVariant::fromValue<qulonglong>(0));

    int activeIndex = 0;
    for (const EditorEntity& entity : m_context.session().documentModel().entities()) {
        const bool isTilemap = std::any_of(
            entity.nativeComponents.begin(), entity.nativeComponents.end(),
            [](const EditorComponent& component) { return component.typeName == "TilemapComponent"; });
        if (!isTilemap) {
            continue;
        }
        m_targetCombo->addItem(ToQString(entity.name),
                               QVariant::fromValue<qulonglong>(entity.uuid));
        if (entity.uuid == activeUuid) {
            activeIndex = m_targetCombo->count() - 1;
        }
    }
    m_activeTarget = activeUuid;
    m_targetCombo->setCurrentIndex(activeIndex);
}

void TilePalettePanel::updateToolState(bool active)
{
    if (m_toolGroup) {
        for (QAction* action : m_toolGroup->actions()) {
            action->setEnabled(active);
        }
        if (active && !m_toolGroup->checkedAction() && !m_toolGroup->actions().isEmpty()) {
            m_toolGroup->actions().first()->setChecked(true);
        }
    }
    if (m_addTilesetButton) {
        m_addTilesetButton->setEnabled(active);
    }
    if (m_removeTilesetButton) {
        m_removeTilesetButton->setEnabled(active);
    }
    if (m_editTilesetButton) {
        m_editTilesetButton->setEnabled(active);
    }
    if (m_resizeTilemapButton) {
        m_resizeTilemapButton->setEnabled(active);
    }
    if (m_targetCombo) {
        m_targetCombo->setEnabled(m_targetCombo->count() > 1);
    }
}

void TilePalettePanel::setActiveTarget(int index)
{
    if (!m_targetCombo || index < 0) {
        return;
    }
    const UInt64 uuid = m_targetCombo->itemData(index).toULongLong();
    if (uuid == 0 || uuid == m_activeTarget) {
        return;
    }
    m_activeTarget = uuid;
    m_context.session().execute(TilemapActivateCommand{uuid});
}

void TilePalettePanel::rebuildTilesets()
{
    while (m_tilesetsLayout->count() > 0) {
        QLayoutItem* item = m_tilesetsLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_tilesetViews.clear();
    m_tilesetsLayout->addStretch();

    if (!m_state.contains("tilesets") || !m_state["tilesets"].is_array()) {
        return;
    }
    for (const auto& entry : m_state["tilesets"]) {
        const String url = entry.value("image_path", String());
        const String imagePath = resolveImagePath(url);
        QPixmap pixmap;
        if (!imagePath.empty()) {
            pixmap.load(ToQString(imagePath));
        }
        const UInt32 tileW = entry.value("tile_width", 16u);
        const UInt32 tileH = entry.value("tile_height", 16u);
        const UInt32 columns = entry.value("columns", 0u);
        const UInt32 firstGid = entry.value("first_gid", 1u);
        const UInt32 margin = entry.value("margin", 0u);
        const UInt32 spacing = entry.value("spacing", 0u);
        const String name = entry.value("name", String());

        auto* view = new TileTilesetView(pixmap, tileW, tileH, columns, firstGid, margin, spacing);
        view->setToolTip(ToQString(name));
        auto* label = new QLabel(ToQString(name), m_scroll);
        label->setStyleSheet(QStringLiteral("color: #C8C8C8; font-weight: bold; padding-top: 4px;"));
        m_tilesetsLayout->insertWidget(m_tilesetsLayout->count() - 1, label);
        m_tilesetsLayout->insertWidget(m_tilesetsLayout->count() - 1, view);
        view->onBrushSelected = [this](int w, int h, DynamicArray<UInt32> gids) {
            m_context.session().execute(TilemapBrushCommand{w, h, std::move(gids)});
        };
        m_tilesetViews.push_back(view);
    }
    applyBrushHighlight();
}

void TilePalettePanel::applyBrushHighlight()
{
    for (TileTilesetView* view : m_tilesetViews) {
        view->setSelection(0, 0, 1, 1);
    }
    if (!m_state.contains("brush") || !m_state["brush"].contains("gids") ||
        m_state["brush"]["gids"].empty()) {
        return;
    }
    const UInt32 firstGid =
        m_state["brush"]["gids"][0].get<UInt32>() & 0x0FFFFFFFu;
    const int brushW = m_state["brush"].value("w", 1);
    const int brushH = m_state["brush"].value("h", 1);

    for (TileTilesetView* view : m_tilesetViews) {
        if (firstGid < view->firstGid() || firstGid >= view->firstGid() + view->tileCount()) {
            continue;
        }
        const UInt32 local = firstGid - view->firstGid();
        const int cellX = static_cast<int>(local % view->columns());
        const int cellY = static_cast<int>(local / view->columns());
        view->setSelection(cellX, cellY, brushW, brushH);
        break;
    }
}

String TilePalettePanel::resolveImagePath(const String& url) const
{
    if (url.empty()) {
        return {};
    }
    FsPath path(url);
    if (path.is_absolute()) {
        return String(path.string());
    }
    const FsPath assetRoot = m_context.session().assetRoot();
    if (!assetRoot.empty()) {
        const FsPath candidate = assetRoot / url;
        if (std::filesystem::exists(candidate)) {
            return String(candidate.string());
        }
    }
    const FsPath project(m_context.project().rootPath);
    if (!project.empty()) {
        const FsPath candidate = project / url;
        if (std::filesystem::exists(candidate)) {
            return String(candidate.string());
        }
    }
    return url;
}

void TilePalettePanel::onNewTilemap()
{
    const Optional<TilemapCreateCommand> command = createTilemapDialog(this);
    if (!command.has_value()) {
        return;
    }
    m_context.session().execute(*command);
}

void TilePalettePanel::onAddTileset()
{
    if (!m_state.contains("tilemap")) {
        QMessageBox::information(this, tr("Add Tileset"), tr("Select or create a tilemap first."));
        return;
    }
    const Optional<TilemapAddTilesetCommand> command = addTilesetDialog(this);
    if (!command.has_value()) {
        return;
    }
    m_context.session().execute(*command);
}

void TilePalettePanel::onRemoveTileset()
{
    if (!m_state.contains("tilesets") || m_state["tilesets"].empty()) {
        return;
    }
    QStringList names;
    for (const auto& entry : m_state["tilesets"]) {
        names << ToQString(entry.value("name", String()));
    }
    bool ok = false;
    const QString selected = QInputDialog::getItem(this, tr("Remove Tileset"), tr("Tileset:"),
                                                   names, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }
    for (const auto& entry : m_state["tilesets"]) {
        if (ToQString(entry.value("name", String())) == selected) {
            const UInt64 assetId = entry.value("asset_id", UInt64(0));
            m_context.session().execute(TilemapRemoveTilesetCommand{assetId});
            break;
        }
    }
}

void TilePalettePanel::onEditTileset()
{
    if (!m_state.contains("tilesets") || m_state["tilesets"].empty()) {
        QMessageBox::information(this, tr("Edit Tileset"), tr("The tilemap has no tilesets."));
        return;
    }
    QStringList names;
    for (const auto& entry : m_state["tilesets"]) {
        names << ToQString(entry.value("name", String()));
    }
    bool ok = false;
    const QString selected = QInputDialog::getItem(this, tr("Edit Tileset"), tr("Tileset:"),
                                                   names, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }
    for (const auto& entry : m_state["tilesets"]) {
        if (ToQString(entry.value("name", String())) == selected) {
            const Optional<TilemapEditTilesetCommand> command = editTilesetDialog(this, entry);
            if (command.has_value()) {
                m_context.session().execute(*command);
            }
            break;
        }
    }
}

void TilePalettePanel::onResizeTilemap()
{
    if (!m_state.contains("tilemap")) {
        QMessageBox::information(this, tr("Resize Tilemap"), tr("Select a tilemap first."));
        return;
    }
    const Json& tilemap = m_state["tilemap"];
    const UInt64 uuid = tilemap.value("uuid", UInt64(0));
    if (uuid == 0) {
        return;
    }
    const int currentW = tilemap.value("map_width", 1);
    const int currentH = tilemap.value("map_height", 1);
    SizeFieldsDialog dialog(tr("Resize Tilemap"), tr("Map width (tiles):"),
                            tr("Map height (tiles):"), currentW, currentH, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.width() == currentW && dialog.height() == currentH) {
        return;
    }
    m_context.session().execute(TilemapResizeCommand{
        uuid, static_cast<UInt32>(dialog.width()),
        static_cast<UInt32>(dialog.height())});
}

} // namespace cakery
