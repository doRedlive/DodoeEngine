// do@Redlive

#pragma once

#include <QApplication>
#include <memory>

namespace cakery {

class EditorContext;
class EditorWindow;
class ProjectManagerWindow;

class EditorApplication : public QApplication {
    Q_OBJECT
public:
    EditorApplication(int& argc, char** argv);
    ~EditorApplication() override;

    int run();

private:
    void onProjectSelected(const QString& projectPath);

    std::unique_ptr<EditorContext>    m_ctx;
    ProjectManagerWindow*             m_projectWindow = nullptr;
    EditorWindow*                     m_editorWindow  = nullptr;
};

} // namespace cakery
