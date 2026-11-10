// do@Redlive
#pragma once
#include "dopch.h"

#include "core/application/Result.h"
#include "core/bridge/EditorCommands.h"

namespace cakery {

class EditorSession;
class DomainEventBus;

class EditorCommandBus {
public:
    EditorCommandBus(EditorSession& session, DomainEventBus& events);

    Result execute(EditorOperation command);

private:
    void reportFailure(const Result& result);

    EditorSession& m_session;
    DomainEventBus& m_events;
};

} // namespace cakery
