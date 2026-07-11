// do@Redlive

#pragma once

#include <memory>
#include "framework/core/Signal.h"
#include "runtime/resource/res_type/scene_res.h"

namespace cakery {

class EditorContext;

enum class PlayState { Edit, Playing, Paused };

class PlayModeController {
public:
    explicit PlayModeController(EditorContext& ctx) : m_ctx(ctx) {}

    void play();
    void pause();
    void resume();
    void stop();

    PlayState state() const { return m_state; }

    Signal<PlayState> stateChanged;

private:
    EditorContext& m_ctx;
    PlayState m_state = PlayState::Edit;
    std::unique_ptr<dodoe::SceneRes> m_snapshot;
};

} // namespace cakery
