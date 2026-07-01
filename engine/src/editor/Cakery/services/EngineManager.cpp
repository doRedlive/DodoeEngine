// do@Redlive

#include "EngineManager.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/event/event.h"
#include "runtime/core/thread/task_scheduler.h"
#include "runtime/core/project/project.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_view/render_view.h"

#include <QDateTime>
#include <QDebug>

using namespace dodoe;

namespace cakery {

EngineManager& EngineManager::getInstance()
{
    static EngineManager s_instance;
    return s_instance;
}

EngineManager::EngineManager()
{
    m_spec.name               = "Cakery";
    m_spec.window_resizeable  = true;
    m_spec.render_settings.api      = RenderBackendApiType::DX12;
    m_spec.render_settings.pipeline = RenderingPipelineType::Deferred;
}

bool EngineManager::initialize(const std::string& projectPath, void* hostHandle, int width, int height)
{
    if (m_initialized) {
        LOG_ERROR("Engine Manager already initialized!");
        return true;
    }

    if (projectPath.empty()) {
        LOG_ERROR("Project path is empty");
        return false;
    }

    LOG_INFO("Start with project {}.", projectPath.c_str());

    TaskScheduler::Self();

    m_spec.host_handle = hostHandle;
    m_spec.width  = width;
    m_spec.height = height;

    m_app     = std::make_unique<Application>(m_spec);
    m_context = &m_app->context();

    // Begin: mirrors Application::run() init sequence
    EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

    m_context->initializeModules();
    qDebug() << "[EngineManager] Modules initialized";

    Project::Load(projectPath);

    m_context->startRuntime();
    qDebug() << "[EngineManager] Runtime started";

    m_context->getLayerStack().attach();
    qDebug() << "[EngineManager] LayerStack attached";
    // End: mirrors Application::run()

    m_initialized  = true;
    m_lastFpsTime  = QDateTime::currentMSecsSinceEpoch();

    emit engineInitialized();
    qDebug() << "[EngineManager] Engine fully initialized";

    return true;
}

void EngineManager::shutdown()
{
    if (m_context && m_initialized) {
        // Begin: mirrors Application::run() cleanup sequence (reverse)
        m_context->getLayerStack().detach();
        m_context->stopRuntime();

        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

        m_context->finalizeModules();
        // End: mirrors Application::run()
    }

    m_initialized = false;
    m_context     = nullptr;
    m_app.reset();
}

void EngineManager::tick()
{
    EventSystem::Poll();
    m_context->tickOneFrame();
    EventSystem::Handle();

    updateFps();
}

dodoe::SystemContext* EngineManager::getContext() const
{
    return m_context;
}

dodoe::World* EngineManager::getWorld() const
{
    auto* ctx = getContext();
    return ctx ? ctx->getWorld() : nullptr;
}

dodoe::Scene* EngineManager::getCurrentScene() const
{
    auto* w = getWorld();
    return w ? w->getCurrentScene() : nullptr;
}

dodoe::RenderView* EngineManager::getMainView() const
{
    auto* ctx = getContext();
    if (!ctx || !ctx->getRenderSystem()) return nullptr;
    return &GetRenderSystem()->getViewFamily()->getView(0);
}

void EngineManager::resizeViewport(int width, int height, float devicePixelRatio)
{
    auto* ctx = getContext();
    if (!ctx || !ctx->getWindowManager()) return;

    auto* window = ctx->getWindowManager()->getWindow();
    if (!window) return;

    int pixelWidth  = static_cast<int>(width * devicePixelRatio);
    int pixelHeight = static_cast<int>(height * devicePixelRatio);
    if (pixelWidth  < 1) pixelWidth  = 1;
    if (pixelHeight < 1) pixelHeight = 1;

    window->setSize(pixelWidth, pixelHeight);

    auto* renderSys = ctx->getRenderSystem();
    if (renderSys) {
        auto* vp = renderSys->getViewportManager();
        if (vp) {
            vp->setLogicalSize(Vector2f(static_cast<float>(pixelWidth),
                                        static_cast<float>(pixelHeight)));
        }
    }
}

void EngineManager::updateFps()
{
    m_frameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 dt  = now - m_lastFpsTime;
    if (dt >= 500) {
        double fps = m_frameCount / (dt / 1000.0);
        emit fpsUpdated(QString::number(fps, 'f', 0) + " FPS");
        m_frameCount  = 0;
        m_lastFpsTime = now;
    }
}

} // namespace cakery
