// do@Redlive
#pragma once
#include "dopch.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QIcon>
#include <QHash>
#include <QPixmap>
#include <QSize>
#include <QStringList>

namespace cakery {

inline QIcon editorIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/pictures/Icons/") + fileName);
    return QIcon(path);
}

inline QString editorThemeName()
{
    const QVariant theme = QApplication::instance()->property("editorTheme");
    return theme.isValid() && theme.canConvert<QString>()
        ? theme.toString()
        : QStringLiteral("cakery-dark");
}

inline QColor editorIconAccent(const QString& fileName)
{
    // Color the actual SVG pixels: QSS text colors do not tint QIcon artwork.
    if (QStringList{"play.svg", "plus.svg", "code.svg", "map.svg"}.contains(fileName)) return QColor("#50fa7b");
    if (QStringList{"pause.svg", "folder.svg", "audio.svg"}.contains(fileName)) return QColor("#f1fa8c");
    if (QStringList{"square.svg", "rotate-3d.svg", "image.svg"}.contains(fileName)) return QColor("#ff79c6");
    if (QStringList{"move-3d.svg", "scene.svg", "viewport-2d.svg", "viewport-3d.svg", "copy.svg"}.contains(fileName)) return QColor("#8be9fd");
    if (QStringList{"scale-3d.svg", "box.svg", "undo.svg", "redo.svg"}.contains(fileName)) return QColor("#ffb86c");
    if (QStringList{"grid.svg", "material.svg"}.contains(fileName)) return QColor("#bd93f9");
    if (fileName == QStringLiteral("trash.svg")) return QColor("#ff5555");
    return QColor("#f8f8f2");
}

inline QIcon editorThemedIcon(const QString& fileName)
{
    const QIcon source = editorIcon(fileName);
    if (editorThemeName() == QStringLiteral("cakery-light")) {
        return source;
    }

    static QHash<QString, QIcon> cache;
    const QString cacheKey = editorThemeName() + QLatin1Char('/') + fileName;
    const auto cached = cache.constFind(cacheKey);
    if (cached != cache.constEnd()) return cached.value();

    QIcon tinted;
    const QColor accent = editorIconAccent(fileName);
    const QSize iconSizes[] = {QSize(16, 16), QSize(20, 20), QSize(24, 24), QSize(32, 32), QSize(64, 64)};
    for (const QSize& size : iconSizes) {
        const QPixmap sourcePixmap = source.pixmap(size);
        if (sourcePixmap.isNull()) {
            continue;
        }

        for (const QIcon::Mode mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled}) {
            const QColor color = mode == QIcon::Disabled ? QColor("#6272a4") : accent;
            QImage image = sourcePixmap.toImage().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const int alpha = qAlpha(image.pixel(x, y));
                    if (alpha != 0) image.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), alpha));
                }
            }
            const QPixmap pixmap = QPixmap::fromImage(image);
            tinted.addPixmap(pixmap, mode, QIcon::Off);
            tinted.addPixmap(pixmap, mode, QIcon::On);
        }
    }

    if (tinted.isNull()) return source;
    cache.insert(cacheKey, tinted);
    return tinted;
}

inline QIcon editorAssetIcon(const QString& type, const QString& path, bool directory = false)
{
    if (directory) return editorThemedIcon(QStringLiteral("folder.svg"));
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("cs")) return editorThemedIcon(QStringLiteral("code.svg"));
    if (type == QStringLiteral("Texture") || type == QStringLiteral("Sprite")
        || QStringList{"png", "jpg", "jpeg", "bmp", "tga", "psd", "hdr"}.contains(ext))
        return editorThemedIcon(QStringLiteral("image.svg"));
    if (type == QStringLiteral("Scene") || ext == QStringLiteral("doscn"))
        return editorThemedIcon(QStringLiteral("scene.svg"));
    if (type == QStringLiteral("Mesh") || type == QStringLiteral("Prefab")
        || QStringList{"fbx", "obj", "gltf", "glb", "prefab"}.contains(ext))
        return editorThemedIcon(QStringLiteral("box.svg"));
    if (type == QStringLiteral("Material") || ext == QStringLiteral("domat"))
        return editorThemedIcon(QStringLiteral("material.svg"));
    if (QStringList{"wav", "ogg", "mp3", "flac"}.contains(ext))
        return editorThemedIcon(QStringLiteral("audio.svg"));
    if (ext == QStringLiteral("tsx") || type == QStringLiteral("Tileset"))
        return editorThemedIcon(QStringLiteral("grid.svg"));
    if (QStringList{"tmj", "tmx"}.contains(ext)) return editorThemedIcon(QStringLiteral("map.svg"));
    if (QStringList{"shader", "hlsl", "glsl", "json"}.contains(ext)) return editorThemedIcon(QStringLiteral("code.svg"));
    return editorThemedIcon(QStringLiteral("file.svg"));
}

} // namespace cakery
