// do@Redlive

#include "dopch.h"

#include "EditorCommandBus.h"

#include "core/EditorSession.h"

namespace cakery {

EditorCommandBus::EditorCommandBus(EditorSession& session, DomainEventBus& events)
    : m_session(session), m_events(events)
{
}

Result EditorCommandBus::execute(EditorOperation command)
{
    if (m_session.execute(std::move(command))) {
        return Result::success();
    }
    const Result failure = Result::failure("Command failed", m_session.diagnostic());
    reportFailure(failure);
    return failure;
}

void EditorCommandBus::reportFailure(const Result& result)
{
    NotificationEvent event;
    event.level = "error";
    event.message = result.message;
    event.source = result.detail;
    m_events.publish(event);
}

} // namespace cakery
