// do@Redlive

#include "EventBridge.h"
#include "framework/EditorContext.h"

namespace cakery {

EventBridge::EventBridge(EditorContext& ctx) : m_ctx(ctx)
{
    // TODO: subscribe to runtime EventSystem for entity/component events
    // and translate to cakery::Signal emissions
}

EventBridge::~EventBridge()
{
    // TODO: unsubscribe from runtime EventSystem
}

} // namespace cakery
