// do@Redlive

#include "ui/common/QtStringInterop.h"

#include "dopch.h"

#include "TilesetPreviewDialog.h"

#include "ui/panels/TilePalettePanel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <fstream>
#include <sstream>

namespace cakery {

namespace {

String ReadFileText(const QString& path)
{
    std::ifstream file(ToDodoeString(path).c_str());
    if (!file.is_open()) {
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return String(buffer.str());
}

QLabel* MakeInfoLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

TilesetPreviewDialog::TilesetPreviewDialog(QString path, QString assetRoot, QWidget* parent)
    : QDialog(parent)
    , m_path(std::move(path))
    , m_assetRoot(std::move(assetRoot))
{
    const QFileInfo info(m_path);
    const QString suffix = info.suffix().toLower();
    const bool isMap = suffix == QStringLiteral("tmj") || suffix == QStringLiteral("tmx");

    setWindowTitle(QStringLiteral("%1 - %2").arg(info.fileName(), tr("Asset Preview")));
    setModal(false);
    resize(720, 560);

    auto* layout = new QVBoxLayout(this);
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #E08080;"));
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);

    const String text = ReadFileText(m_path);
    if (text.empty()) {
        m_errorLabel->setText(tr("Could not read file."));
        m_errorLabel->show();
        return;
    }
    Json json;
    try {
        json = Json::parse(text);
    } catch (const Json::exception&) {
        m_errorLabel->setText(tr("This tileset uses the Tiled XML format, which cannot be previewed. Only engine JSON tilesets are supported."));
        m_errorLabel->show();
        return;
    }

    if (isMap) {
        buildTiledMapLayout(json);
    } else {
        buildTilesetLayout(json);
    }
}

void TilesetPreviewDialog::buildTilesetLayout(const Json& json)
{
    const String name = json.value("Name", String());
    const UInt32 tileW = json.value("TileWidth", 0u);
    const UInt32 tileH = json.value("TileHeight", 0u);
    const UInt32 columns = json.value("Columns", 0u);
    const UInt32 tileCount = json.value("TileCount", 0u);
    const UInt32 margin = json.value("Margin", 0u);
    const UInt32 spacing = json.value("Spacing", 0u);
    const UInt32 firstGid = json.value("FirstGid", 1u);
    const String imagePath = json.value("ImagePath", String());

    auto* layout = static_cast<QVBoxLayout*>(this->layout());

    auto* infoRow = new QWidget(this);
    auto* form = new QFormLayout(infoRow);
    form->setContentsMargins(0, 0, 0, 0);
    form->addRow(tr("Name:"), MakeInfoLabel(ToQString(name)));
    form->addRow(tr("Tile size:"), MakeInfoLabel(QStringLiteral("%1 x %2 px").arg(tileW).arg(tileH)));
    form->addRow(tr("Grid:"), MakeInfoLabel(tr("%1 columns, %2 tiles")
        .arg(static_cast<qulonglong>(columns)).arg(static_cast<qulonglong>(tileCount))));
    form->addRow(tr("Margin / Spacing:"), MakeInfoLabel(QStringLiteral("%1 / %2 px").arg(margin).arg(spacing)));
    form->addRow(tr("First GID:"), MakeInfoLabel(QString::number(firstGid)));
    form->addRow(tr("Image:"), MakeInfoLabel(ToQString(imagePath)));
    layout->addWidget(infoRow);

    QString resolved = resolveImagePath(ToQString(imagePath));
    QPixmap pixmap(resolved);
    if (pixmap.isNull()) {
        auto* missing = new QLabel(tr("Tileset image not found."), this);
        missing->setStyleSheet(QStringLiteral("color: #A0A0A0;"));
        layout->addWidget(missing);
    } else {
        auto* view = new TileTilesetView(pixmap, tileW, tileH, columns, firstGid, margin, spacing, this);
        m_imageScroll = new QScrollArea(this);
        m_imageScroll->setWidgetResizable(true);
        m_imageScroll->setWidget(view);
        layout->addWidget(m_imageScroll, 1);
    }

    if (json.contains("TileProperties") && json["TileProperties"].is_object() &&
        !json["TileProperties"].empty()) {
        layout->addWidget(MakeInfoLabel(tr("Tile properties:")));
        populatePropertiesTable(json["TileProperties"]);
    }
}

void TilesetPreviewDialog::buildTiledMapLayout(const Json& json)
{
    const UInt32 mapW = json.value("width", 0u);
    const UInt32 mapH = json.value("height", 0u);
    const UInt32 tileW = json.value("tilewidth", 0u);
    const UInt32 tileH = json.value("tileheight", 0u);
    const String orientation = json.value("orientation", String());

    auto* layout = static_cast<QVBoxLayout*>(this->layout());

    auto* infoRow = new QWidget(this);
    auto* form = new QFormLayout(infoRow);
    form->setContentsMargins(0, 0, 0, 0);
    form->addRow(tr("Map size:"), MakeInfoLabel(QStringLiteral("%1 x %2 tiles").arg(mapW).arg(mapH)));
    form->addRow(tr("Tile size:"), MakeInfoLabel(QStringLiteral("%1 x %2 px").arg(tileW).arg(tileH)));
    form->addRow(tr("Orientation:"), MakeInfoLabel(ToQString(orientation)));
    layout->addWidget(infoRow);

    if (json.contains("tilesets") && json["tilesets"].is_array()) {
        QStringList lines;
        for (const auto& ts : json["tilesets"]) {
            if (!ts.is_object()) continue;
            const UInt32 firstGid = ts.value("firstgid", 0u);
            const String name = ts.value("name", ts.value("source", String()));
            lines << QStringLiteral("[%1] %2").arg(firstGid).arg(ToQString(name));
        }
        if (!lines.isEmpty()) {
            form->addRow(tr("Tilesets:"), MakeInfoLabel(lines.join(QStringLiteral("\n"))));
        }
    }

    if (json.contains("layers") && json["layers"].is_array()) {
        QStringList lines;
        for (const auto& layer : json["layers"]) {
            if (!layer.is_object()) continue;
            const String name = layer.value("name", String());
            const String type = layer.value("type", String());
            const bool visible = layer.value("visible", true);
            lines << QStringLiteral("%1 (%2)%3")
                .arg(ToQString(name), ToQString(type),
                     visible ? QString() : tr(" [hidden]"));
        }
        if (!lines.isEmpty()) {
            form->addRow(tr("Layers:"), MakeInfoLabel(lines.join(QStringLiteral("\n"))));
        }
    }
}

QString TilesetPreviewDialog::resolveImagePath(const QString& imagePath) const
{
    if (imagePath.isEmpty()) {
        return {};
    }
    const QFileInfo info(imagePath);
    if (info.isAbsolute() && info.exists()) {
        return imagePath;
    }
    const QDir assetDir(m_assetRoot);
    const QDir fileDir(QFileInfo(m_path).dir());
    const QString byAsset = assetDir.filePath(imagePath);
    if (QFile::exists(byAsset)) {
        return byAsset;
    }
    const QString byFile = fileDir.filePath(imagePath);
    if (QFile::exists(byFile)) {
        return byFile;
    }
    return byAsset;
}

void TilesetPreviewDialog::populatePropertiesTable(const Json& tileProperties)
{
    m_propertiesTable = new QTableWidget(this);
    m_propertiesTable->setColumnCount(3);
    m_propertiesTable->setHorizontalHeaderLabels({tr("Tile ID"), tr("Property"), tr("Value")});
    m_propertiesTable->verticalHeader()->hide();
    m_propertiesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_propertiesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    int row = 0;
    for (auto tileIt = tileProperties.begin(); tileIt != tileProperties.end(); ++tileIt) {
        if (!tileIt.value().is_object()) continue;
        for (auto propIt = tileIt.value().begin(); propIt != tileIt.value().end(); ++propIt) {
            const String value = propIt.value().is_string()
                ? propIt.value().get<String>()
                : ToString(propIt.value());
            m_propertiesTable->insertRow(row);
            m_propertiesTable->setItem(row, 0, new QTableWidgetItem(ToQString(tileIt.key())));
            m_propertiesTable->setItem(row, 1, new QTableWidgetItem(ToQString(propIt.key())));
            m_propertiesTable->setItem(row, 2, new QTableWidgetItem(ToQString(value)));
            ++row;
        }
    }
    m_propertiesTable->horizontalHeader()->setStretchLastSection(true);
    m_propertiesTable->setMaximumHeight(180);
    this->layout()->addWidget(m_propertiesTable);
}

} // namespace cakery
