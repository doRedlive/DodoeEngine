// do@Redlive
#pragma once
#include "dopch.h"

#include "core/Signal.h"

namespace cakery {

class ViewModel {
public:
    virtual ~ViewModel() = default;

    Signal<> changed;

protected:
    void notifyChanged() { changed.fire(); }
};

} // namespace cakery
