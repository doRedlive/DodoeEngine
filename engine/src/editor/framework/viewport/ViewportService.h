// do@Redlive

#pragma once

#include "runtime/core/math/math.h"
#include <vector>
#include <memory>

namespace dodoe {
    class RenderViewTarget;
}

namespace cakery {

class EditorContext;

enum class ViewportKind { Scene, Game };

struct EditorViewport {
    ViewportKind            kind;
    dodoe::RenderViewTarget* backend = nullptr;
    float aspect = 0.0f;
};

class ViewportService {
public:
    explicit ViewportService(EditorContext& ctx) : m_ctx(ctx) {}

    EditorViewport* registerViewport(ViewportKind kind, void* hostHandle, int w, int h, float dpr);
    void unregisterViewport(EditorViewport* vp);
    void onResized(EditorViewport* vp, int w, int h, float dpr);
    void updateAndRenderAll(float dt);

    void setGameAspect(EditorViewport* vp, float aspect);

private:
    EditorContext& m_ctx;
    std::vector<std::unique_ptr<EditorViewport>> m_viewports;
};

} // namespace cakery
