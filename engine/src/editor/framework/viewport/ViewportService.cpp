// do@Redlive

#include "ViewportService.h"
#include "framework/EditorContext.h"

namespace cakery {

EditorViewport* ViewportService::registerViewport(ViewportKind kind, void* hostHandle,
                                                   int w, int h, float dpr)
{
    (void)hostHandle; (void)w; (void)h; (void)dpr;
    auto vp = std::make_unique<EditorViewport>();
    vp->kind = kind;
    auto* ptr = vp.get();
    m_viewports.push_back(std::move(vp));
    return ptr;
}

void ViewportService::unregisterViewport(EditorViewport* vp)
{
    m_viewports.erase(
        std::remove_if(m_viewports.begin(), m_viewports.end(),
                      [vp](const auto& e) { return e.get() == vp; }),
        m_viewports.end());
}

void ViewportService::onResized(EditorViewport* vp, int w, int h, float dpr)
{
    (void)vp; (void)w; (void)h; (void)dpr;
}

void ViewportService::updateAndRenderAll(float /*dt*/)
{
}

void ViewportService::setGameAspect(EditorViewport* vp, float aspect)
{
    if (vp) vp->aspect = aspect;
}

} // namespace cakery
