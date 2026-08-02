// do@Redlive

#include "application.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/async/task_scheduler.h"

#include <fstream>

namespace dodoe {

    Application* Application::m_instance = nullptr;

    namespace {

        String RenderBackendApiToString(const RenderBackendApiType api) {
            switch (api) {
                case RenderBackendApiType::None:    return "None";
                case RenderBackendApiType::OpenGL:  return "OpenGL";
                case RenderBackendApiType::Vulkan:  return "Vulkan";
                case RenderBackendApiType::DX12:    return "DX12";
            }
            return "None";
        }

        Bool RenderBackendApiFromString(const String& str, RenderBackendApiType& out_api) {
            if (str == "None")   { out_api = RenderBackendApiType::None;   return true; }
            if (str == "OpenGL") { out_api = RenderBackendApiType::OpenGL; return true; }
            if (str == "Vulkan") { out_api = RenderBackendApiType::Vulkan; return true; }
            if (str == "DX12")   { out_api = RenderBackendApiType::DX12;   return true; }
            return false;
        }

        String RenderingPipelineToString(const RenderingPipelineType pipeline) {
            switch (pipeline) {
                case RenderingPipelineType::None:          return "None";
                case RenderingPipelineType::Forward:       return "Forward";
                case RenderingPipelineType::ForwardPlus:   return "ForwardPlus";
                case RenderingPipelineType::Deferred:      return "Deferred";
                case RenderingPipelineType::DeferredPlus:  return "DeferredPlus";
                case RenderingPipelineType::Only2D:        return "Only2D";
            }
            return "None";
        }

        Bool RenderingPipelineFromString(const String& str, RenderingPipelineType& out_pipeline) {
            if (str == "None")         { out_pipeline = RenderingPipelineType::None;         return true; }
            if (str == "Forward")      { out_pipeline = RenderingPipelineType::Forward;      return true; }
            if (str == "ForwardPlus")  { out_pipeline = RenderingPipelineType::ForwardPlus;  return true; }
            if (str == "Deferred")     { out_pipeline = RenderingPipelineType::Deferred;     return true; }
            if (str == "DeferredPlus") { out_pipeline = RenderingPipelineType::DeferredPlus; return true; }
            if (str == "Only2D")       { out_pipeline = RenderingPipelineType::Only2D;       return true; }
            return false;
        }

        String ThreadingModeToString(const ThreadingMode mode) {
            switch (mode) {
                case ThreadingMode::TripleThread: return "TripleThread";
                case ThreadingMode::DualThread:   return "DualThread";
                case ThreadingMode::SingleThread: return "SingleThread";
            }
            return "TripleThread";
        }

        Bool ThreadingModeFromString(const String& str, ThreadingMode& out_mode) {
            if (str == "TripleThread") { out_mode = ThreadingMode::TripleThread; return true; }
            if (str == "DualThread")   { out_mode = ThreadingMode::DualThread;   return true; }
            if (str == "SingleThread") { out_mode = ThreadingMode::SingleThread; return true; }
            return false;
        }

    } // namespace

    Json ApplicationSpecification::toJson() const {
        Json root;
        root["Name"] = name;
        root["Width"] = width;
        root["Height"] = height;
        root["WindowResizeable"] = window_resizeable;
        root["RenderSettings"] = {
            { "API", RenderBackendApiToString(render_settings.api) },
            { "Pipeline", RenderingPipelineToString(render_settings.pipeline) },
            { "ThreadingMode", ThreadingModeToString(render_settings.threading_mode) },
        };
        return root;
    }

    Bool ApplicationSpecification::FromJson(const Json& json, ApplicationSpecification& out_spec) {
        try {
            if (json.contains("Name") && json["Name"].is_string()) {
                out_spec.name = json["Name"].get<String>();
            }
            if (json.contains("Width") && json["Width"].is_number_unsigned()) {
                out_spec.width = json["Width"].get<UInt32>();
            }
            if (json.contains("Height") && json["Height"].is_number_unsigned()) {
                out_spec.height = json["Height"].get<UInt32>();
            }
            if (json.contains("WindowResizeable") && json["WindowResizeable"].is_boolean()) {
                out_spec.window_resizeable = json["WindowResizeable"].get<Bool>();
            }

            if (json.contains("RenderSettings") && json["RenderSettings"].is_object()) {
                const auto& rs = json["RenderSettings"];

                if (rs.contains("API") && rs["API"].is_string()) {
                    RenderBackendApiType api{ RenderBackendApiType::None };
                    if (!RenderBackendApiFromString(rs["API"].get<String>(), api)) {
                        DO_ERROR("ApplicationSpecification: unknown render API \"{}\"", rs["API"].get<String>());
                        return false;
                    }
                    out_spec.render_settings.api = api;
                }
                if (rs.contains("Pipeline") && rs["Pipeline"].is_string()) {
                    RenderingPipelineType pipeline{ RenderingPipelineType::None };
                    if (!RenderingPipelineFromString(rs["Pipeline"].get<String>(), pipeline)) {
                        DO_ERROR("ApplicationSpecification: unknown rendering pipeline \"{}\"", rs["Pipeline"].get<String>());
                        return false;
                    }
                    out_spec.render_settings.pipeline = pipeline;
                }
                if (rs.contains("ThreadingMode") && rs["ThreadingMode"].is_string()) {
                    ThreadingMode mode{ ThreadingMode::TripleThread };
                    if (!ThreadingModeFromString(rs["ThreadingMode"].get<String>(), mode)) {
                        DO_ERROR("ApplicationSpecification: unknown threading mode \"{}\"", rs["ThreadingMode"].get<String>());
                        return false;
                    }
                    out_spec.render_settings.threading_mode = mode;
                }
            }

            return true;
        } catch (const Json::exception& e) {
            DO_ERROR("ApplicationSpecification: failed to parse config: {}", e.what());
            return false;
        }
    }

    Bool ApplicationSpecification::loadFromFile(const FsPath& file_path) {
        Json data;
        try {
            std::ifstream fin(file_path);
            if (!fin.is_open()) {
                DO_ERROR("ApplicationSpecification: failed to open config file: {}", file_path.string());
                return false;
            }
            fin >> data;
        } catch (const Json::exception& e) {
            DO_ERROR("ApplicationSpecification: failed to parse config file {}: {}", file_path.string(), e.what());
            return false;
        }
        return FromJson(data, *this);
    }

    Bool ApplicationSpecification::saveToFile(const FsPath& file_path) const {
        std::ofstream fout(file_path);
        if (!fout.is_open()) {
            DO_ERROR("ApplicationSpecification: failed to open config file for writing: {}", file_path.string());
            return false;
        }
        fout << toJson().dump(4);
        return true;
    }

    Application::Application(const ApplicationSpecification& spec) : m_app_spec(spec) {
        m_context = SystemContext::Create({m_app_spec});
        m_instance = this;
        m_running = true;
    }

    Application::~Application() {
        SystemContext::Destroy(m_context);
        m_instance = nullptr;
        m_running = false;
    }

    SystemContext& Application::context() {
        return *m_context;
    }

    const SystemContext& Application::context() const {
        return *m_context;
    }

    void Application::run() {
        TaskScheduler::Self();

        EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(this);

        m_context->initializeModules();

        m_context->startRuntime();

        m_context->getLayerStack().attach();

        while (m_running) {
            EventSystem::Poll();
            EventSystem::Publish<BeforeOneTickEvent>();
            m_context->tickOneFrame();
            EventSystem::Publish<AfterOneTickEvent>();
            EventSystem::Handle();
        }

        m_context->getLayerStack().detach();

        m_context->stopRuntime();

        m_context->finalizeModules();

        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(this);

        m_context->postShutdown();
    }

    void Application::quit() {
        m_running = false;
    }

} // dodoe