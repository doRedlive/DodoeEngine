// do@Redlive

#include "ProjectConfig.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>

namespace cakery {

QString ProjectEntry::displayDate() const
{
    if (isMissing) return QCoreApplication::translate("ProjectConfig", "Missing");
    qint64 ticks = lastModifiedUnix > 0 ? lastModifiedUnix : lastOpenedUnix;
    if (ticks == 0) return "";
    QDateTime dt = QDateTime::fromSecsSinceEpoch(ticks);
    return dt.toString("yyyy-MM-dd HH:mm");
}

ProjectConfig& ProjectConfig::getInstance()
{
    static ProjectConfig s_instance;
    return s_instance;
}

QString ProjectConfig::configPath() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("project_manager_config.json");
}

void ProjectConfig::load()
{
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject root = doc.object();
    m_data.version = root["Version"].toInt(1);
    m_data.lastOpened = root["LastOpened"].toString();
    m_data.lastSortOption = root["LastSortOption"].toString();

    m_data.projects.clear();
    for (const auto& val : root["Projects"].toArray()) {
        QJsonObject obj = val.toObject();
        ProjectEntry entry;
        entry.name = obj["Name"].toString();
        entry.projectFile = obj["ProjectFile"].toString();
        entry.pinned = obj["Pinned"].toBool();
        entry.lastOpenedUnix = static_cast<qint64>(obj["LastOpenedUnix"].toDouble());
        entry.lastModifiedUnix = static_cast<qint64>(obj["LastModifiedUnix"].toDouble());
        entry.iconPath = obj["IconPath"].toString();
        entry.description = obj["Description"].toString();
        entry.engineVersion = obj["EngineVersion"].toString();

        entry.isMissing = !QFileInfo::exists(entry.projectFile);
        if (!entry.isMissing) {
            QFileInfo fi(entry.projectFile);
            entry.lastModifiedUnix = fi.lastModified().toSecsSinceEpoch();
            entry.projectPath = fi.absolutePath();
        }
        m_data.projects.append(entry);
    }
}

void ProjectConfig::save()
{
    QDir().mkpath(QFileInfo(configPath()).absolutePath());

    QJsonArray projectsArr;
    for (const auto& p : m_data.projects) {
        QJsonObject obj;
        obj["Name"] = p.name;
        obj["ProjectFile"] = p.projectFile;
        obj["Pinned"] = p.pinned;
        obj["LastOpenedUnix"] = p.lastOpenedUnix;
        obj["LastModifiedUnix"] = p.lastModifiedUnix;
        obj["IconPath"] = p.iconPath;
        obj["Description"] = p.description;
        obj["EngineVersion"] = p.engineVersion;
        projectsArr.append(obj);
    }

    QJsonObject root;
    root["Version"] = m_data.version;
    root["Projects"] = projectsArr;
    root["LastOpened"] = m_data.lastOpened;
    root["LastSortOption"] = m_data.lastSortOption;

    QFile file(configPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void ProjectConfig::addProject(const ProjectEntry& entry)
{
    for (auto& p : m_data.projects) {
        if (p.projectFile == entry.projectFile) {
            p.name = entry.name;
            p.lastOpenedUnix = entry.lastOpenedUnix;
            if (entry.lastModifiedUnix > 0) p.lastModifiedUnix = entry.lastModifiedUnix;
            if (!entry.projectPath.isEmpty()) p.projectPath = entry.projectPath;
            return;
        }
    }
    m_data.projects.append(entry);
}

void ProjectConfig::removeProject(const QString& projectFile)
{
    m_data.projects.removeIf([&](const ProjectEntry& p) {
        return p.projectFile == projectFile;
    });
}

void ProjectConfig::markOpened(const QString& projectFile)
{
    m_data.lastOpened = projectFile;
    for (auto& p : m_data.projects) {
        if (p.projectFile == projectFile) {
            p.lastOpenedUnix = QDateTime::currentSecsSinceEpoch();
            return;
        }
    }
}

void ProjectConfig::setLastSortOption(const QString& option)
{
    m_data.lastSortOption = option;
}

} // namespace cakery
