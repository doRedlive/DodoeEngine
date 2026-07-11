// do@Redlive

#pragma once

#include "property/PropertyDrawer.h"

namespace cakery {

template <int N>
class VectorDrawer : public PropertyDrawer {
public:
    QWidget* build(const PropertyContext& pc) override;
    void updateValue(const PropertyContext& pc) override;
};

extern template class VectorDrawer<2>;
extern template class VectorDrawer<3>;
extern template class VectorDrawer<4>;

} // namespace cakery
