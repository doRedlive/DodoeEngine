// do@Redlive

#pragma once

#include "runtime/core/utils/uuid.h"
#include "framework/core/Signal.h"
#include <string>

namespace cakery {

class EditorContext;

class EventBridge {
public:
    explicit EventBridge(EditorContext& ctx);
    ~EventBridge();

    Signal<dodoe::UUID>              entityCreated;
    Signal<dodoe::UUID>              entityDestroyed;
    Signal<dodoe::UUID>              hierarchyChanged;
    Signal<dodoe::UUID, std::string> componentChanged;

private:
    EditorContext& m_ctx;
};

} // namespace cakery
