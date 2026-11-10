// do@Redlive
#pragma once
#include "dopch.h"

#include <QObject>
#include <QString>

namespace cakery {

class ProjectManagerWindow;

class ProjectSelectionController : public QObject {
    Q_OBJECT
public:
    explicit ProjectSelectionController(QObject* parent = nullptr);
    ~ProjectSelectionController() override;

    void show();
    void dismiss();

signals:
    void projectSelected(const QString& projectPath);

private:
    void ensureWindow();

    ProjectManagerWindow* m_window = nullptr;
};

} // namespace cakery
