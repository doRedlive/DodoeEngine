// do@Redlive
#pragma once
#include "dopch.h"

#include "core/PlayState.h"
#include "core/Signal.h"

namespace cakery {

class EditorSession;

class PlayModeController {
public:
    explicit PlayModeController(EditorSession& session);
    ~PlayModeController();

    PlayState state() const;
    bool documentEdited() const;

    void play();
    void pauseOrResume();
    void stop(bool keepChanges);

    Signal<PlayState> stateChanged;

private:
    void onStateChanged(PlayState state);

    EditorSession& m_session;
    ScopedConnection m_subscription;
};

} // namespace cakery
