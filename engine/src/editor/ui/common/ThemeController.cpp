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
    qApp->setProperty("editorTheme", normalized);

    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Active, QPalette::Highlight, QColor(0xbd, 0x93, 0xf9));
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(0x6f, 0x5a, 0x96));
    palette.setColor(QPalette::Active, QPalette::HighlightedText, QColor(0x28, 0x2a, 0x36));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(0xf8, 0xf8, 0xf2));
    qApp->setPalette(palette);

    qApp->setStyleSheet(styleSheet);
}

} // namespace cakery
