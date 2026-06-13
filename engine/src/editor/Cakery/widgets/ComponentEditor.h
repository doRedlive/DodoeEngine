// do@Redlive

#pragma once

#include <QWidget>
#include <QString>
#include "runtime/function/world/entity.h"

namespace cakery {

class ComponentEditor : public QWidget {
    Q_OBJECT
public:
    explicit ComponentEditor(const QString& typeName, dodoe::Entity entity,
                             bool canRemove, QWidget* parent = nullptr);

    QString typeName() const { return m_typeName; }
    bool canRemove() const { return m_canRemove; }

    virtual void refresh() = 0;

signals:
    void removeRequested(const QString& typeName);

protected:
    dodoe::Entity entity() const { return m_entity; }

private:
    QString m_typeName;
    dodoe::Entity m_entity;
    bool m_canRemove;
};

} // namespace cakery
