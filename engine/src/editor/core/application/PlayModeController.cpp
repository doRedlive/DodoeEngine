// do@Redlive

#include "dopch.h"

#include "PlayModeController.h"

#include "core/bridge/EditorCommands.h"
#include "core/EditorSession.h"

namespace cakery {

PlayModeController::PlayModeController(EditorSession& session)
    : m_session(session)
{
    m_subscription = ScopedConnection(
        m_session.playStateChanged,
        m_session.playStateChanged.connect([this](PlayState state) { onStateChanged(state); }));
}

PlayModeController::~PlayModeController() = default;

PlayState PlayModeController::state() const
{
    return m_session.playState();
}

bool PlayModeController::documentEdited() const
{
    return m_session.playDocumentEdited();
}

void PlayModeController::play()
{
    m_session.execute(PlayActionCommand{PlayAction::Play});
}

void PlayModeController::pauseOrResume()
{
    m_session.execute(PlayActionCommand{
        m_session.playState() == PlayState::Paused ? PlayAction::Resume : PlayAction::Pause});
}

void PlayModeController::stop(bool keepChanges)
{
    m_session.stopPlay(keepChanges);
}

void PlayModeController::onStateChanged(PlayState state)
{
    stateChanged.fire(state);
}

} // namespace cakery
