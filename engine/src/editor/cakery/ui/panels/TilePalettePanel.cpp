// do@Redlive

#include "TilePalettePanel.h"

#include "cakery/ui/EditorWorkspaceContext.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
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
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <filesystem>

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

nlohmann::json createTilemapDialog(QWidget* parent)
{
    bool ok = false;
    const QString name = QInputDialog::getText(parent, QObject::tr("New Tilemap"), QObject::tr("Name:"),
                                               QLineEdit::Normal, QObject::tr("Tilemap"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return nlohmann::json();
    }

    SizeFieldsDialog mapSize(QObject::tr("Tilemap Size"), QObject::tr("Map width (tiles):"),
                             QObject::tr("Map height (tiles):"), 40, 30, parent);
    if (mapSize.exec() != QDialog::Accepted) {
        return nlohmann::json();
    }

    SizeFieldsDialog tileSize(QObject::tr("Tile Size"), QObject::tr("Tile width (px):"),
                              QObject::tr("Tile height (px):"), 16, 16, parent);
    if (tileSize.exec() != QDialog::Accepted) {
        return nlohmann::json();
    }

    nlohmann::json payload;
    payload["name"] = name.trimmed().toStdString();
    payload["width"] = static_cast<unsigned int>(mapSize.width());
    payload["height"] = static_cast<unsigned int>(mapSize.height());
    payload["tile_width"] = static_cast<unsigned int>(tileSize.width());
    payload["tile_height"] = static_cast<unsigned int>(tileSize.height());
    return payload;
}

nlohmann::json addTilesetDialog(QWidget* parent)
{
    const QString image = QFileDialog::getOpenFileName(
        parent, QObject::tr("Select Tileset Image"), QString(),
        QObject::tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (image.isEmpty()) {
        return nlohmann::json();
    }

    SizeFieldsDialog tileSize(QObject::tr("Tileset Tile Size"), QObject::tr("Tile width (px):"),
                              QObject::tr("Tile height (px):"), 16, 16, parent);
    if (tileSize.exec() != QDialog::Accepted) {
        return nlohmann::json();
    }

    nlohmann::json payload;
    payload["image"] = image.toStdString();
    payload["tile_width"] = static_cast<unsigned int>(tileSize.width());
    payload["tile_height"] = static_cast<unsigned int>(tileSize.height());
    payload["margin"] = 0;
    payload["spacing"] = 0;
    return payload;
}

} // namespace

TileTilesetView::TileTilesetView(QPixmap image, std::uint32_t tileWidth, std::uint32_t tileHeight,
                                 std::uint32_t columns, std::uint32_t firstGid, QWidget* parent)
    : QWidget(parent)
    , m_image(std::move(image))
    , m_tileWidth(tileWidth)
    , m_tileHeight(tileHeight)
    , m_columns(columns)
    , m_firstGid(firstGid)
{
    setMouseTracking(true);
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
    const int x = m_selectionCell.x() * static_cast<int>(m_tileWidth);
    const int y = m_selectionCell.y() * static_cast<int>(m_tileHeight);
    const int w = m_selectionW * static_cast<int>(m_tileWidth);
    const int h = m_selectionH * static_cast<int>(m_tileHeight);
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

    painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
    for (std::uint32_t x = 1; x < m_columns; ++x) {
        painter.drawLine(static_cast<int>(x * m_tileWidth), 0,
                         static_cast<int>(x * m_tileWidth), m_image.height());
    }
    const std::uint32_t rows = static_cast<std::uint32_t>(m_image.height()) / m_tileHeight;
    for (std::uint32_t y = 1; y < rows; ++y) {
        painter.drawLine(0, static_cast<int>(y * m_tileHeight),
                         m_image.width(), static_cast<int>(y * m_tileHeight));
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
    const int cx = event->position().x() / static_cast<int>(m_tileWidth);
    const int cy = event->position().y() / static_cast<int>(m_tileHeight);
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
    const int cx = event->position().x() / static_cast<int>(m_tileWidth);
    const int cy = event->position().y() / static_cast<int>(m_tileHeight);
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
    std::vector<std::uint32_t> gids;
    gids.reserve(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t gid = m_firstGid + static_cast<std::uint32_t>((y0 + y) * static_cast<int>(m_columns) + (x0 + x));
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

    auto* removeButton = new QToolButton(toolbar);
    removeButton->setText(tr("Remove Tileset"));
    removeButton->setAutoRaise(true);
    connect(removeButton, &QToolButton::clicked, this, &TilePalettePanel::onRemoveTileset);
    toolbarLayout->addWidget(removeButton);
    toolbarLayout->addStretch();
    layout->addWidget(toolbar);

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
    nlohmann::json state;
    if (!m_context.session().queryTilemapState(std::string(), state)) {
        m_state = nlohmann::json();
        m_statusLabel->setText(tr("No tilemap selected. Select a tilemap in the Hierarchy, or create a new one."));
        rebuildTilesets();
        return;
    }
    m_state = std::move(state);
    const std::string tilemapName = m_state.contains("tilemap")
        ? m_state["tilemap"].value("name", std::string())
        : std::string();
    m_statusLabel->setText(QStringLiteral("Tilemap: %1\n%2 tileset(s)")
        .arg(QString::fromStdString(tilemapName))
        .arg(static_cast<qulonglong>(m_state["tilesets"].size())));
    rebuildTilesets();
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
        const std::string url = entry.value("image_path", std::string());
        const std::string imagePath = resolveImagePath(url);
        QPixmap pixmap;
        if (!imagePath.empty()) {
            pixmap.load(QString::fromStdString(imagePath));
        }
        const std::uint32_t tileW = entry.value("tile_width", 16u);
        const std::uint32_t tileH = entry.value("tile_height", 16u);
        const std::uint32_t columns = entry.value("columns", 0u);
        const std::uint32_t firstGid = entry.value("first_gid", 1u);
        const std::string name = entry.value("name", std::string());

        auto* view = new TileTilesetView(pixmap, tileW, tileH, columns, firstGid);
        view->setToolTip(QString::fromStdString(name));
        auto* label = new QLabel(QString::fromStdString(name), m_scroll);
        label->setStyleSheet(QStringLiteral("color: #C8C8C8; font-weight: bold; padding-top: 4px;"));
        m_tilesetsLayout->insertWidget(m_tilesetsLayout->count() - 1, label);
        m_tilesetsLayout->insertWidget(m_tilesetsLayout->count() - 1, view);
        view->onBrushSelected = [this](int w, int h, std::vector<std::uint32_t> gids) {
            nlohmann::json brush;
            brush["w"] = w;
            brush["h"] = h;
            brush["gids"] = nlohmann::json::array();
            for (std::uint32_t gid : gids) {
                brush["gids"].push_back(gid);
            }
            m_context.session().execute(EditorCommandMessage{"tilemap.brush", brush.dump()});
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
    const std::uint32_t firstGid = m_state["brush"]["gids"][0].get<std::uint32_t>();
    const int brushW = m_state["brush"].value("w", 1);
    const int brushH = m_state["brush"].value("h", 1);

    for (TileTilesetView* view : m_tilesetViews) {
        if (firstGid < view->firstGid() || firstGid >= view->firstGid() + view->tileCount()) {
            continue;
        }
        const std::uint32_t local = firstGid - view->firstGid();
        const int cellX = static_cast<int>(local % view->columns());
        const int cellY = static_cast<int>(local / view->columns());
        view->setSelection(cellX, cellY, brushW, brushH);
        break;
    }
}

std::string TilePalettePanel::resolveImagePath(const std::string& url) const
{
    if (url.empty()) {
        return {};
    }
    std::filesystem::path path(url);
    if (path.is_absolute()) {
        return path.string();
    }
    const std::filesystem::path assetRoot = m_context.session().assetRoot();
    if (!assetRoot.empty()) {
        const std::filesystem::path candidate = assetRoot / url;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    const std::filesystem::path project(m_context.project().rootPath);
    if (!project.empty()) {
        const std::filesystem::path candidate = project / url;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return url;
}

void TilePalettePanel::onNewTilemap()
{
    const nlohmann::json payload = createTilemapDialog(this);
    if (payload.is_null()) {
        return;
    }
    m_context.session().execute(EditorCommandMessage{"tilemap.create", payload.dump()});
}

void TilePalettePanel::onAddTileset()
{
    if (!m_state.contains("tilemap")) {
        QMessageBox::information(this, tr("Add Tileset"), tr("Select or create a tilemap first."));
        return;
    }
    const nlohmann::json payload = addTilesetDialog(this);
    if (payload.is_null()) {
        return;
    }
    m_context.session().execute(EditorCommandMessage{"tilemap.add_tileset", payload.dump()});
}

void TilePalettePanel::onRemoveTileset()
{
    if (!m_state.contains("tilesets") || m_state["tilesets"].empty()) {
        return;
    }
    QStringList names;
    for (const auto& entry : m_state["tilesets"]) {
        names << QString::fromStdString(entry.value("name", std::string()));
    }
    bool ok = false;
    const QString selected = QInputDialog::getItem(this, tr("Remove Tileset"), tr("Tileset:"),
                                                   names, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }
    for (const auto& entry : m_state["tilesets"]) {
        if (QString::fromStdString(entry.value("name", std::string())) == selected) {
            const std::uint64_t assetId = entry.value("asset_id", std::uint64_t(0));
            m_context.session().execute(EditorCommandMessage{"tilemap.remove_tileset",
                                                              std::to_string(assetId)});
            break;
        }
    }
}

} // namespace cakery
