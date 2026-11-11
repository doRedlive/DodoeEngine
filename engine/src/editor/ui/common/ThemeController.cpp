// do@Redlive

#include "dopch.h"

#include "ThemeController.h"

#include "ui/common/QtStringInterop.h"
#include "services/EditorResourceLocator.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>

namespace cakery {

ThemeController::ThemeController(EditorResourceLocator& resources)
    : m_resources(resources)
{
}

void ThemeController::apply(const String& themeName)
{
    const QString requested = ToQString(themeName).trimmed();
    const QString normalized = requested.isEmpty()
        ? QStringLiteral("cakery-dark")
        : requested;
    const auto basePath = m_resources.resolve("editor://themes/cakery-dark.qss");
    QFile baseFile(ToQString(basePath.string()));
    if (!baseFile.open(QFile::ReadOnly | QFile::Text)) {
        return;
    }

    QString styleSheet = QString::fromUtf8(baseFile.readAll());
    if (normalized != QStringLiteral("cakery-dark")) {
        const auto overridePath = m_resources.resolve(
            (String("editor://themes/") + ToDodoeString(normalized) + ".qss").c_str());
        QFile overrideFile(ToQString(overridePath.string()));
        if (overrideFile.open(QFile::ReadOnly | QFile::Text)) {
            styleSheet += QStringLiteral("\n") + QString::fromUtf8(overrideFile.readAll());
        }
    }
    if (normalized == QStringLiteral("cakery-dark") || normalized == QStringLiteral("cakery-color")) {
        QFile workspaceFile(ToQString(m_resources.resolve("editor://themes/cakery-workspace.qss").string()));
        if (workspaceFile.open(QFile::ReadOnly | QFile::Text)) {
            styleSheet += QStringLiteral("\n") + QString::fromUtf8(workspaceFile.readAll());
        }
    }
    qApp->setProperty("editorTheme", normalized);

    QPalette palette = qApp->palette();
    const bool light = normalized == QStringLiteral("cakery-light");
    // Native-painted children (color wells, headers and viewports) also need
    // the theme palette; a stylesheet alone leaves system-colored surfaces.
    palette.setColor(QPalette::Window, QColor(light ? "#f4f7fb" : "#21222c"));
    palette.setColor(QPalette::Base, QColor(light ? "#ffffff" : "#282a36"));
    palette.setColor(QPalette::AlternateBase, QColor(light ? "#eaf1f8" : "#21222c"));
    palette.setColor(QPalette::WindowText, QColor(light ? "#263447" : "#f8f8f2"));
    palette.setColor(QPalette::Text, QColor(light ? "#263447" : "#f8f8f2"));
    palette.setColor(QPalette::Button, QColor(light ? "#eaf1f8" : "#343746"));
    palette.setColor(QPalette::ButtonText, QColor(light ? "#263447" : "#f8f8f2"));
    palette.setColor(QPalette::ToolTipBase, QColor(light ? "#ffffff" : "#282a36"));
    palette.setColor(QPalette::ToolTipText, QColor(light ? "#263447" : "#f8f8f2"));
    palette.setColor(QPalette::Light, QColor(light ? "#ffffff" : "#6272a4"));
    palette.setColor(QPalette::Midlight, QColor(light ? "#eaf1f8" : "#44475a"));
    palette.setColor(QPalette::Mid, QColor(light ? "#b9cbe0" : "#44475a"));
    palette.setColor(QPalette::Dark, QColor(light ? "#8b9db3" : "#191a21"));
    palette.setColor(QPalette::Shadow, QColor(light ? "#b9cbe0" : "#191a21"));
    palette.setColor(QPalette::Link, QColor(light ? "#536fc0" : "#8be9fd"));
    palette.setColor(QPalette::LinkVisited, QColor("#bd93f9"));
    palette.setColor(QPalette::BrightText, QColor("#ff5555"));
    palette.setColor(QPalette::PlaceholderText, QColor(light ? "#718198" : "#9b9db4"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, QColor(light ? "#8b9db3" : "#6272a4"));
    }
    palette.setColor(QPalette::Active, QPalette::Highlight, QColor(0xbd, 0x93, 0xf9));
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0x6f, 0x5a, 0x96));
    palette.setColor(QPalette::Active, QPalette::HighlightedText, QColor(0x28, 0x2a, 0x36));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(0xf8, 0xf8, 0xf2));
    qApp->setPalette(palette);

    qApp->setStyleSheet(styleSheet);
}

} // namespace cakery
