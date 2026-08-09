// do@Redlive

#include "ViewportService.h"
#include "framework/EditorContext.h"
#include "framework/camera/EditorCamera.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_view/render_view_target.h"
#include "runtime/function/render/render_view/render_view_manager.h"
#include "runtime/function/render/render_view/camera_provider.h"
#include "runtime/core/channel/camera_channel.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"

namespace cakery {

EditorViewport* ViewportService::registerViewport(ViewportKind kind, void* hostHandle,
                                                   int w, int h, float dpr)
{
    auto* sysCtx = m_ctx.systemContext();
    if (!sysCtx) return nullptr;
    auto* renderSys = sysCtx->getRenderSystem();
    if (!renderSys) return nullptr;

    auto* viewMgr = renderSys->getViewManager();
    if (!viewMgr) return nullptr;

    dodoe::RenderViewTargetCreateInfo info;
    info.logical = dodoe::Vector2f(static_cast<float>(w), static_cast<float>(h));
    info.pixel   = dodoe::Vector2i(static_cast<int>(w * dpr), static_cast<int>(h * dpr));
    info.window  = dodoe::Vector2i(w, h);

    if (kind == ViewportKind::Scene) {
        info.camera = m_ctx.editorCameraProvider();
    }

    auto target = viewMgr->createViewTarget(info);
    if (!target) return nullptr;

    auto vp = std::make_unique<EditorViewport>();
    vp->kind    = kind;
    vp->backend = target;
    auto* ptr = vp.get();
    m_viewports.push_back(std::move(vp));
    return ptr;
}

void ViewportService::unregisterViewport(EditorViewport* vp)
{
    if (!vp || !vp->backend) return;

    auto* renderSys = m_ctx.systemContext()->getRenderSystem();
    if (renderSys) {
        auto* viewMgr = renderSys->getViewManager();
        if (viewMgr) {
            viewMgr->destroyViewTarget(vp->backend);
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
    vp->backend->resize(
        dodoe::Vector2i(w, h),
        dodoe::Vector2i(static_cast<int>(w * dpr), static_cast<int>(h * dpr)));
}

void ViewportService::updateAndRenderAll(float dt)
{
    (void)dt;

    for (auto& vp : m_viewports) {
        if (!vp->backend) continue;

        if (vp->kind == ViewportKind::Scene) {
            m_ctx.camera().commitToRenderChannel();
        }
    }
}

void ViewportService::setGameAspect(EditorViewport* vp, float aspect)
{
    if (vp) vp->aspect = aspect;
}

} // namespace cakery
