// do@Redlive

#pragma once

#include <QApplication>
#include <QDir>
#include <QIcon>

namespace cakery {

inline QIcon editorIcon(const QString& fileName)
{
    const QString path = QDir(QApplication::applicationDirPath())
        .filePath(QStringLiteral("resources/pictures/Icons/") + fileName);
    return QIcon(path);
}

} // namespace cakery
