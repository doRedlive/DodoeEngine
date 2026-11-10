// do@Redlive
#pragma once
#include "dopch.h"

#include <QByteArray>
#include <QString>

namespace ads {
class CDockManager;
}

namespace cakery {

class EditorLayoutController {
public:
    EditorLayoutController(ads::CDockManager* dockManager, const QString& fileName);

    void captureDefault();
    void restore();
    void save() const;
    void reset();

private:
    ads::CDockManager* m_dockManager;
    QByteArray m_defaultState;
    QString m_statePath;
};

} // namespace cakery
