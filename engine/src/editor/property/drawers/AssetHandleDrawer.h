// do@Redlive

#pragma once

#include "property/PropertyDrawer.h"

namespace cakery {

class AssetHandleDrawer : public PropertyDrawer {
public:
    QWidget* build(const PropertyContext& pc) override;
    void updateValue(const PropertyContext& pc) override;
};

} // namespace cakery
