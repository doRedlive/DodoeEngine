// do@Redlive
#pragma once
#include "dopch.h"

#include <QObject>
#include <QString>

namespace cakery {

class ProjectManagerWindow;
class ProjectConfig;

class ProjectSelectionController : public QObject {
    Q_OBJECT
public:
    explicit ProjectSelectionController(ProjectConfig& config, QObject* parent = nullptr);
    ~ProjectSelectionController() override;

    void show();
    void dismiss();

signals:
    void projectSelected(const QString& projectPath);

private:
    void ensureWindow();

    ProjectManagerWindow* m_window = nullptr;
    ProjectConfig& m_config;
};

} // namespace cakery
