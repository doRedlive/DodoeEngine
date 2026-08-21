#include "TilePalettePanel.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/CreateTilemapCommand.h"
#include "framework/command/commands/CreateTileLayerCommand.h"
#include "framework/core/UuidResolve.h"
#include "framework/selection/SelectionManager.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/components/tilemap/tile_layer_component.h"

#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QActionGroup>
#include <QScrollArea>
#include <QPainter>
#include <QImage>
#include <QMouseEvent>

#include <algorithm>

namespace cakery {

class TilesetPreview : public QWidget {
public:
    TilesetPreview(EditorContext& ctx, QWidget* parent = nullptr)
        : QWidget(parent), m_ctx(ctx)
    {
        setMinimumSize(200, 200);
    }

    dodoe::UUID tilemap() const { return m_tilemap; }

    void setTilemap(dodoe::UUID uuid)
    {
        if (uuid == m_tilemap) return;
        m_tilemap = uuid;
        m_image = QImage();

        auto* scene = m_ctx.activeScene();
        if (scene && uuid.isValid()) {
            auto entity = ResolveEntity(scene, uuid);
            if (entity.valid() && entity.hasComponent<dodoe::TilemapComponent>()) {
                auto& tm = entity.getComponent<dodoe::TilemapComponent>();
                if (!tm.tilesets.empty() && tm.tilesets[0]) {
                    m_image.load(tm.tilesets[0]->image_path.c_str());
                    m_firstGid = tm.tilesets[0]->first_gid;
                    m_tileWidth = tm.tilesets[0]->tile_width;
                    m_tileHeight = tm.tilesets[0]->tile_height;
                    m_columns = tm.tilesets[0]->columns;
                }
            }
        }
        setMinimumSize(m_image.isNull() ? QSize(200, 200) : m_image.size());
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x2a, 0x2a, 0x2a));
        p.setPen(QColor(0x88, 0x88, 0x88));

        if (!m_tilemap.isValid()) {
            p.drawText(rect(), Qt::AlignCenter, "Select a tilemap to edit");
            return;
        }
        if (m_image.isNull()) {
            p.drawText(rect(), Qt::AlignCenter, "No tileset");
            return;
        }

        p.drawImage(0, 0, m_image);

        // 网格
        int cols = columns();
        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        for (int x = 0; x <= m_image.width(); x += (int)m_tileWidth) {
            p.drawLine(x, 0, x, m_image.height());
        }
        for (int y = 0; y <= m_image.height(); y += (int)m_tileHeight) {
            p.drawLine(0, y, m_image.width(), y);
        }

        // 当前 brush 高亮
        const auto& brush = m_ctx.tilePaint().brush();
        if (!brush.empty() && brush.gids[0] >= m_firstGid) {
            int gid = (int)brush.gids[0] - (int)m_firstGid;
            int col = gid % cols;
            int row = gid / cols;
            p.setPen(QPen(QColor(255, 255, 255, 220), 2));
            p.drawRect(col * (int)m_tileWidth, row * (int)m_tileHeight,
                       (int)m_tileWidth, (int)m_tileHeight);
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (m_image.isNull()) return;

        int cols = columns();
        int col = (int)(e->position().x() / (float)m_tileWidth);
        int row = (int)(e->position().y() / (float)m_tileHeight);
        if (col < 0 || row < 0 || col >= cols) return;

        TileBrush brush;
        brush.w = 1;
        brush.h = 1;
        brush.gids = { m_firstGid + (dodoe::UInt32)(row * cols + col) };
        m_ctx.tilePaint().setBrush(std::move(brush));
        update();
    }

private:
    int columns() const
    {
        if (m_columns > 0) return (int)m_columns;
        if (m_image.isNull()) return 1;
        return std::max(1, m_image.width() / (int)m_tileWidth);
    }

    EditorContext& m_ctx;
    dodoe::UUID m_tilemap;
    QImage m_image;
    dodoe::UInt32 m_firstGid = 1;
    dodoe::UInt32 m_tileWidth = 16;
    dodoe::UInt32 m_tileHeight = 16;
    dodoe::UInt32 m_columns = 0;
};

TilePalettePanel::TilePalettePanel(EditorContext& ctx, QWidget* parent)
    : Panel(ctx, parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildToolbar();

    m_preview = new TilesetPreview(m_ctx, this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_preview);
    scroll->setStyleSheet("background-color: #2a2a2a;");
    m_paletteView = scroll;
    layout->addWidget(m_paletteView, 1);

    buildLayerList();
    layout->addWidget(m_layerList);

    m_infoLabel = new QLabel("Select a tilemap to edit", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet("color: #888; font-size: 12px; padding: 4px;");
    layout->addWidget(m_infoLabel);

    auto h1 = m_ctx.selection().changed.connect([this](const auto&) { refreshAll(); });
    m_connections.emplace_back(m_ctx.selection().changed, h1);

    auto h2 = m_ctx.commands().changed.connect([this]() { refreshAll(); });
    m_connections.emplace_back(m_ctx.commands().changed, h2);

    refreshAll();
}

void TilePalettePanel::buildToolbar()
{
    m_toolbar = new QToolBar("Tile Tools", this);
    m_toolbar->setMovable(false);

    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    auto addTool = [this](const QString& text, TileTool tool) {
        auto* action = m_toolbar->addAction(text);
        action->setCheckable(true);
        m_toolGroup->addAction(action);
        connect(action, &QAction::triggered, [this, tool]() {
            m_ctx.tilePaint().setTool(tool);
        });
        return action;
    };

    auto* brushAction = addTool("Brush", TileTool::Brush);
    brushAction->setChecked(true);
    addTool("Erase", TileTool::Erase);
    addTool("Fill", TileTool::Fill);
    addTool("Rect", TileTool::Rect);
    addTool("Line", TileTool::Line);
    addTool("Picker", TileTool::Picker);

    m_toolbar->addSeparator();

    auto* newMapAction = m_toolbar->addAction("New Tilemap");
    connect(newMapAction, &QAction::triggered, this, &TilePalettePanel::onCreateTilemap);

    auto* newLayerAction = m_toolbar->addAction("New Layer");
    connect(newLayerAction, &QAction::triggered, this, &TilePalettePanel::onCreateLayer);

    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        layout->insertWidget(0, m_toolbar);
    }
}

void TilePalettePanel::buildLayerList()
{
    m_layerList = new QListWidget(this);
    m_layerList->setMaximumHeight(120);
    connect(m_layerList, &QListWidget::currentRowChanged, [this](int) {
        auto* item = m_layerList->currentItem();
        if (!item) return;
        auto uuid = dodoe::UUID(item->data(Qt::UserRole).toULongLong());
        m_ctx.tilePaint().setActiveLayer(uuid);
        updatePaletteView();
    });
}

void TilePalettePanel::refreshAll()
{
    // 同步选中实体到绘制服务（tilemap/层自动激活）
    auto selected = m_ctx.selection().primary();
    if (selected.isValid()) {
        m_ctx.tilePaint().setActiveEntity(selected);
    }
    updateLayerList();
    updatePaletteView();
}

void TilePalettePanel::updateLayerList()
{
    m_layerList->blockSignals(true);
    m_layerList->clear();

    dodoe::UUID tilemap = m_ctx.tilePaint().activeTilemap();
    dodoe::UUID activeLayer = m_ctx.tilePaint().activeLayer();

    auto* scene = m_ctx.activeScene();
    if (scene && tilemap.isValid()) {
        auto entity = ResolveEntity(scene, tilemap);
        if (entity.valid() && entity.hasComponent<dodoe::HierarchyComponent>()) {
            for (auto child : entity.getComponent<dodoe::HierarchyComponent>().children) {
                if (!child.valid() || !child.hasComponent<dodoe::TileLayerComponent>()) continue;
                auto& layer = child.getComponent<dodoe::TileLayerComponent>();
                auto* item = new QListWidgetItem(layer.layer_name.c_str(), m_layerList);
                item->setData(Qt::UserRole, static_cast<qulonglong>(static_cast<uint64_t>(child.uuid())));
                if (child.uuid() == activeLayer) {
                    m_layerList->setCurrentItem(item);
                }
            }
        }
    }
    m_layerList->blockSignals(false);
}

void TilePalettePanel::updatePaletteView()
{
    auto tilemap = m_ctx.tilePaint().activeTilemap();
    auto layer = m_ctx.tilePaint().activeLayer();

    if (!(tilemap == m_preview->tilemap())) {
        m_preview->setTilemap(tilemap);
    }

    if (tilemap.isValid() && layer.isValid()) {
        auto* scene = m_ctx.activeScene();
        if (scene) {
            auto entity = ResolveEntity(scene, layer);
            if (entity.valid() && entity.hasComponent<dodoe::TileLayerComponent>()) {
                auto& lc = entity.getComponent<dodoe::TileLayerComponent>();
                m_infoLabel->setText(QString("%1 x %2")
                                         .arg((int)lc.layer_width)
                                         .arg((int)lc.layer_height));
                return;
            }
        }
        m_infoLabel->setText("No layer");
        return;
    }
    m_infoLabel->setText("Select a tilemap to edit");
}

void TilePalettePanel::onCreateTilemap()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Tilemap", "Name:", QLineEdit::Normal, "Tilemap", &ok);
    if (!ok || name.isEmpty()) return;
    int width = QInputDialog::getInt(this, "New Tilemap", "Width (tiles):", 16, 1, 4096, 1, &ok);
    if (!ok) return;
    int height = QInputDialog::getInt(this, "New Tilemap", "Height (tiles):", 16, 1, 4096, 1, &ok);
    if (!ok) return;

    auto cmd = std::make_unique<CreateTilemapCommand>(
        dodoe::String(name.toUtf8().constData()),
        static_cast<dodoe::UInt32>(width), static_cast<dodoe::UInt32>(height));
    auto* executed = m_ctx.commands().execute(std::move(cmd));
    if (!executed) return;
    auto* created = static_cast<CreateTilemapCommand*>(executed);
    m_ctx.tilePaint().setActiveEntity(created->created());
    m_ctx.selection().select(created->created());
    refreshAll();
}

void TilePalettePanel::onCreateLayer()
{
    dodoe::UUID tilemap = m_ctx.tilePaint().activeTilemap();
    if (!tilemap.isValid()) return;

    bool ok = false;
    QString name = QInputDialog::getText(this, "New Layer", "Name:", QLineEdit::Normal, "Layer", &ok);
    if (!ok || name.isEmpty()) return;
    int width = QInputDialog::getInt(this, "New Layer", "Width (tiles):", 16, 1, 4096, 1, &ok);
    if (!ok) return;
    int height = QInputDialog::getInt(this, "New Layer", "Height (tiles):", 16, 1, 4096, 1, &ok);
    if (!ok) return;

    auto cmd = std::make_unique<CreateTileLayerCommand>(
        tilemap, dodoe::String(name.toUtf8().constData()),
        static_cast<dodoe::UInt32>(width), static_cast<dodoe::UInt32>(height));
    auto* executed = m_ctx.commands().execute(std::move(cmd));
    if (!executed) return;
    auto* created = static_cast<CreateTileLayerCommand*>(executed);
    m_ctx.tilePaint().setActiveLayer(created->created());
    refreshAll();
}

} // namespace cakery
