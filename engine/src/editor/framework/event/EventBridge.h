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

    Signal<dodoe::Uuid>              entityCreated;
    Signal<dodoe::Uuid>              entityDestroyed;
    Signal<dodoe::Uuid>              hierarchyChanged;
    Signal<dodoe::Uuid, std::string> componentChanged;

private:
    EditorContext& m_ctx;
};

} // namespace cakery
