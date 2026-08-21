// do@Redlive

#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>

namespace cakery {

struct ProjectEntry {
    QString name;
    QString projectFile;
    bool pinned = false;
    qint64 lastOpenedUnix = 0;
    qint64 lastModifiedUnix = 0;
    QString iconPath;
    QStringList tags;
    QString description;
    QString engineVersion;

    bool isMissing = false;
    QString projectPath;

    double rowOpacity() const { return isMissing ? 0.5 : 1.0; }
    QString displayDate() const;
    bool operator==(const ProjectEntry& other) const { return projectFile == other.projectFile; }
};

struct ProjectConfigData {
    int version = 1;
    QList<ProjectEntry> projects;
    QString lastOpened;
    QString lastSortOption;
};

class ProjectConfig {
public:
    static ProjectConfig& getInstance();

    QList<ProjectEntry> projects() const { return m_data.projects; }
    QString lastOpened() const { return m_data.lastOpened; }

    void load();
    void save();
    void addProject(const ProjectEntry& entry);
    void removeProject(const QString& projectFile);
    void markOpened(const QString& projectFile);
    void setLastSortOption(const QString& option);

private:
    ProjectConfig() = default;
    QString configPath() const;

    ProjectConfigData m_data;
};

} // namespace cakery
