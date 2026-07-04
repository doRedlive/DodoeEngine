// do@Redlive

#pragma once

#include <QObject>
#include <memory>

#include "runtime/core/application.h"

namespace dodoe {
    class SystemContext;
    class World;
    class Scene;
    class Camera;
}

namespace cakery {

class EngineManager : public QObject {
    Q_OBJECT
public:
    static EngineManager& getInstance();

    bool initialize(const std::string& projectPath, void* hostHandle, int width, int height);
    void shutdown();
    void tick();

    bool isInitialized() const { return m_initialized; }

    dodoe::SystemContext* getContext() const;
    dodoe::World* getWorld() const;
    dodoe::Scene* getCurrentScene() const;
    dodoe::RenderViewport* getRenderViewport() const;

    void resizeViewport(int width, int height, float devicePixelRatio = 1.0f);

    std::string getVersion() const { return "Dodoe 1.0.0"; }

signals:
    void engineInitialized();
    void fpsUpdated(const QString& fps);
    void engineError(const QString& message);

private:
    EngineManager();
    ~EngineManager() override = default;
    EngineManager(const EngineManager&) = delete;
    EngineManager& operator=(const EngineManager&) = delete;

    void updateFps();

    bool m_initialized = false;

    dodoe::ApplicationSpecification m_spec{};
    std::unique_ptr<dodoe::Application> m_app;
    dodoe::SystemContext* m_context = nullptr;

    int m_frameCount = 0;
    qint64 m_lastFpsTime = 0;
};

} // namespace cakery
