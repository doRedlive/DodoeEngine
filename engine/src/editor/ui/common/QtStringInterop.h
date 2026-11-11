// do@Redlive

#pragma once

#include <QByteArray>
#include <QString>

#include <string>

#include "dopch.h"

namespace cakery {

inline String ToDodoeString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return String(utf8.constData(), static_cast<Size_t>(utf8.size()));
}

inline String ToDodoeString(const char* value)
{
    return String(value == nullptr ? "" : value);
}

inline String ToDodoeString(const std::string& value)
{
    return String(value.c_str(), static_cast<Size_t>(value.size()));
}

inline QString ToQString(const String& value)
{
    return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

inline QString ToQString(const std::string& value)
{
    return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

inline QString ToQString(const char* value)
{
    return QString::fromUtf8(value == nullptr ? "" : value);
}

} // namespace cakery
