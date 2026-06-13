

#pragma once

#include "ComponentEditor.h"

namespace cakery {

class GenericComponentEditor : public ComponentEditor {
    Q_OBJECT
public:
    explicit GenericComponentEditor(const QString& typeName, dodoe::Entity entity,
                                    bool canRemove, QWidget* parent = nullptr);
    void refresh() override;
};

}
