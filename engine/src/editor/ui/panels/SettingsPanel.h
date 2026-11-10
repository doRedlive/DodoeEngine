// do@Redlive
#pragma once
#include "dopch.h"

#include <QString>
#include <QWidget>

#include <functional>


class QPushButton;

namespace cakery {

class EditorJsonWidget;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    void setFilePath(const QString& path);
    void setFallback(std::function<Json()> fallback);
    void reload();
    const Json& value() const;

signals:
    void saved();

private:
    void save();

    EditorJsonWidget* m_editor = nullptr;
    QString m_filePath;
    std::function<Json()> m_fallback;
};

} // namespace cakery
