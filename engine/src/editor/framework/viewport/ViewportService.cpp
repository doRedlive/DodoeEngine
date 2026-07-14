// do@Redlive

#include "ViewportService.h"
#include "framework/EditorContext.h"
#include "framework/camera/EditorCamera.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_view/render_viewport.h"
#include "runtime/core/channel/render_channel.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"

namespace cakery {

EditorViewport* ViewportService::registerViewport(ViewportKind kind, void* hostHandle,
                                                   int w, int h, float dpr)
{
    auto* renderSys = m_ctx.systemContext()->getRenderSystem();
    if (!renderSys) return nullptr;

    dodoe::RenderViewportCreateInfo info;
    info.logical = dodoe::Vector2f(static_cast<float>(w), static_cast<float>(h));
    info.pixel   = dodoe::Vector2i(static_cast<int>(w * dpr), static_cast<int>(h * dpr));
    info.window  = dodoe::Vector2i(w, h);

    auto backend = dodoe::RenderViewport::Create(info);
    if (!backend) return nullptr;

    auto vp = std::make_unique<EditorViewport>();
    vp->kind    = kind;
    vp->backend = backend.get();
    auto* ptr = vp.get();
    m_viewports.push_back(std::move(vp));

    renderSys->getRenderViewports().push_back(std::move(backend));
    return ptr;
}

void ViewportService::unregisterViewport(EditorViewport* vp)
{
    if (!vp || !vp->backend) return;

    auto* renderSys = m_ctx.systemContext()->getRenderSystem();
    if (renderSys) {
        auto& viewports = renderSys->getRenderViewports();
        auto it = std::find_if(viewports.begin(), viewports.end(),
                               [vp](const auto& e) { return e.get() == vp->backend; });
        if (it != viewports.end()) {
            auto scope = std::move(*it);
            viewports.erase(it);
            dodoe::RenderViewport::Destroy(scope);
        }
    }

    m_viewports.erase(
        std::remove_if(m_viewports.begin(), m_viewports.end(),
                      [vp](const auto& e) { return e.get() == vp; }),
        m_viewports.end());
}

void ViewportService::onResized(EditorViewport* vp, int w, int h, float dpr)
{
    if (!vp || !vp->backend) return;
    vp->backend->setLogicalSize(dodoe::Vector2f(static_cast<float>(w), static_cast<float>(h)));
    vp->backend->setPixelSize(dodoe::Vector2i(static_cast<int>(w * dpr), static_cast<int>(h * dpr)));
    vp->backend->setWindowSize(dodoe::Vector2i(w, h));
}

void ViewportService::updateAndRenderAll(float dt)
{
    (void)dt;

    for (auto& vp : m_viewports) {
        if (!vp->backend) continue;

        if (vp->kind == ViewportKind::Scene) {
            dodoe::Matrix4f view = m_ctx.camera().view();
            dodoe::Matrix4f proj = m_ctx.camera().projection();
            vp->backend->setCameraOverride(view, proj);
        } else {
            vp->backend->clearCameraOverride();
        }
    }
}

void ViewportService::setGameAspect(EditorViewport* vp, float aspect)
{
    if (vp) vp->aspect = aspect;
}

} // namespace cakery
