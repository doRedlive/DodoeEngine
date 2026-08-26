// do@Redlive

#pragma once

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QIcon>
#include <QPixmap>
#include <QSize>

#include "services/EditorConfig.h"

namespace cakery {

inline QIcon editorIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/pictures/Icons/") + fileName);
    return QIcon(path);
}

inline QIcon editorThemedIcon(const QString& fileName)
{
    const QIcon source = editorIcon(fileName);
    if (EditorConfig::self().themeName() == "cakery-light") {
        return source;
    }

    QIcon tinted;
    const QSize iconSizes[] = {QSize(16, 16), QSize(24, 24), QSize(32, 32)};
    for (const QSize& size : iconSizes) {
        const QPixmap sourcePixmap = source.pixmap(size);
        if (sourcePixmap.isNull()) {
            continue;
        }

        QImage image = sourcePixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const int alpha = qAlpha(image.pixel(x, y));
                if (alpha != 0) {
                    image.setPixel(x, y, qRgba(240, 240, 240, alpha));
                }
            }
        }
        tinted.addPixmap(QPixmap::fromImage(image));
    }

    return tinted.isNull() ? source : tinted;
}

} // namespace cakery
