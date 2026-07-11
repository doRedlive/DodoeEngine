// do@Redlive

#pragma once

#include <QString>
#include <QStringList>

namespace ads { class CDockManager; }

namespace cakery {

class LayoutManager {
public:
    explicit LayoutManager(ads::CDockManager* dm) : m_dm(dm) {}

    void applyDefault();
    void saveNamed(const QString& name);
    void loadNamed(const QString& name);
    void deleteNamed(const QString& name);
    QStringList namedLayouts() const;

    void restoreSession();
    void saveSession();

    void applyPreset(const QString& preset);

private:
    ads::CDockManager* m_dm;
};

} // namespace cakery
