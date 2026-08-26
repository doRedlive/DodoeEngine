// do@Redlive

#pragma once

#include <QString>
#include <QWidget>

#include <functional>

#include <nlohmann/json.hpp>

class QPushButton;

namespace cakery {

class EditorJsonWidget;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    void setFilePath(const QString& path);
    void setFallback(std::function<nlohmann::json()> fallback);
    void reload();

signals:
    void saved();

private:
    void save();

    EditorJsonWidget* m_editor = nullptr;
    QString m_filePath;
    std::function<nlohmann::json()> m_fallback;
};

} // namespace cakery
