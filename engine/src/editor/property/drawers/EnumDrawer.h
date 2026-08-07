// do@Redlive

#pragma once

#include "property/PropertyDrawer.h"

namespace cakery {

class EnumDrawer : public PropertyDrawer {
public:
    QWidget* build(const PropertyContext& pc) override;
    void updateValue(const PropertyContext& pc) override;

private:
    QWidget* m_widget = nullptr;
};

} // namespace cakery
