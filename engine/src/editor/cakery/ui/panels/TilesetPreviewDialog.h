// do@Redlive

#pragma once

#include <QDialog>

#include <nlohmann/json.hpp>

#include <QString>

class QLabel;
class QScrollArea;
class QTableWidget;

namespace cakery {

class TilesetPreviewDialog final : public QDialog {
    Q_OBJECT
public:
    TilesetPreviewDialog(QString path, QString assetRoot, QWidget* parent = nullptr);

private:
    void buildTilesetLayout(const nlohmann::json& json);
    void buildTiledMapLayout(const nlohmann::json& json);
    QString resolveImagePath(const QString& imagePath) const;
    void populatePropertiesTable(const nlohmann::json& tileProperties);

    QString m_path;
    QString m_assetRoot;
    QLabel* m_errorLabel = nullptr;
    QScrollArea* m_imageScroll = nullptr;
    QTableWidget* m_propertiesTable = nullptr;
};

} // namespace cakery
