// do@Redlive

#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <string>

namespace ads {
    class CDockManager;
    class CDockWidget;
}

namespace cakery {

class EditorContext;

class LayoutManager {
public:
    LayoutManager(ads::CDockManager* dm, EditorContext& ctx);
    ~LayoutManager();

    void applyDefault();
    void applyPreset(const std::string& name);

    void saveNamed(const QString& name);
    void loadNamed(const QString& name);
    void deleteNamed(const QString& name);
    QStringList namedLayouts() const;

    void restoreSession();
    void saveSession();

private:
    ads::CDockWidget* createDock(const std::string& id, const std::string& factory);

    ads::CDockManager* m_dm;
    EditorContext& m_ctx;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cakery
