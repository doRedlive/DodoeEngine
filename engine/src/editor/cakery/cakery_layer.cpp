// do@Redlive

#include "cakery_layer.h"

#include "cakery/systems/simulation_frame_context_system.h"

#include "cakery/framework/editor_panel_registry.h"
#include "cakery/framework/editor_panel_manager.h"
#include "cakery/panels/console_panel.h"
#include "cakery/panels/dockspace_panel.h"
#include "cakery/panels/hierarchy_panel.h"
#include "cakery/panels/inspector_panel.h"
#include "cakery/panels/project_panel.h"
#include "cakery/panels/project_manager_panel.h"
#include "cakery/panels/title_bar.h"
#include "cakery/panels/viewport_panel.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/input/input.h"
#include "runtime/function/render/framework/camera.h"
#include "runtime/function/render/framework/scene_graph.h"
#include "runtime/function/render/render_resource.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/function/render/framework/scene_importer.h"

using namespace dodoe;

namespace cakery {

    namespace {
        static EditorPanelDescriptor MakeDockspaceDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "dockspace";
            descriptor.title = "";
            descriptor.category = "System";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::None, 0.25f};
            descriptor.default_open = true;
            descriptor.closable = false;
            descriptor.requires_runtime = false;
            descriptor.show_in_view_menu = false;
            descriptor.order = 0;
            return descriptor;
        }

        static EditorPanelDescriptor MakeProjectManagerDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "project_manager";
            descriptor.title = "Project Manager";
            descriptor.category = "System";
            descriptor.stage = EditorPanelStage::Startup;
            descriptor.default_dock = {EditorDockPlacement::None, 0.25f};
            descriptor.default_open = true;
            descriptor.closable = false;
            descriptor.requires_runtime = false;
            descriptor.show_in_view_menu = false;
            descriptor.order = 0;
            return descriptor;
        }

        static EditorPanelDescriptor MakeTitlebarDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "titlebar";
            descriptor.title = "";
            descriptor.category = "System";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::None, 0.25f};
            descriptor.default_open = true;
            descriptor.closable = false;
            descriptor.requires_runtime = false;
            descriptor.show_in_view_menu = false;
            descriptor.order = 1;
            return descriptor;
        }

        static EditorPanelDescriptor MakeHierarchyDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "hierarchy";
            descriptor.title = "Hierarchy";
            descriptor.category = "Scene";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::Left, 0.22f};
            descriptor.default_open = true;
            descriptor.closable = true;
            descriptor.requires_runtime = true;
            descriptor.show_in_view_menu = true;
            descriptor.order = 10;
            return descriptor;
        }

        static EditorPanelDescriptor MakeInspectorDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "inspector";
            descriptor.title = "Inspector";
            descriptor.category = "Scene";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::Right, 0.28f};
            descriptor.default_open = true;
            descriptor.closable = true;
            descriptor.requires_runtime = true;
            descriptor.show_in_view_menu = true;
            descriptor.order = 20;
            return descriptor;
        }

        static EditorPanelDescriptor MakeProjectDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "project";
            descriptor.title = "Content Browser";
            descriptor.category = "Assets";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::Bottom, 0.28f};
            descriptor.default_open = true;
            descriptor.closable = true;
            descriptor.requires_runtime = false;
            descriptor.show_in_view_menu = true;
            descriptor.order = 30;
            return descriptor;
        }

        static EditorPanelDescriptor MakeConsoleDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "console";
            descriptor.title = "Console";
            descriptor.category = "Debug";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::Bottom, 0.28f};
            descriptor.default_open = true;
            descriptor.closable = true;
            descriptor.requires_runtime = false;
            descriptor.show_in_view_menu = true;
            descriptor.order = 40;
            return descriptor;
        }

        static EditorPanelDescriptor MakeViewportDescriptor() {
            EditorPanelDescriptor descriptor{};
            descriptor.id = "viewport";
            descriptor.title = "Viewport";
            descriptor.category = "View";
            descriptor.stage = EditorPanelStage::Workspace;
            descriptor.default_dock = {EditorDockPlacement::Center, 0.25f};
            descriptor.default_open = true;
            descriptor.closable = false;
            descriptor.requires_runtime = true;
            descriptor.show_in_view_menu = true;
            descriptor.order = 50;
            return descriptor;
        }

        static void RegisterPanel(EditorPanelDescriptor descriptor, EditorPanelFactory factory) {
            (void)EditorPanelRegistry::Self().registerPanel(std::move(descriptor), std::move(factory));
        }

        void RegisterDefaultEditorPanels() {
            RegisterPanel(MakeDockspaceDescriptor(), []() {
                return create_scope<DockspacePanel>(MakeDockspaceDescriptor());
            });
            RegisterPanel(MakeProjectManagerDescriptor(), []() {
                return create_scope<ProjectManagerPanel>(MakeProjectManagerDescriptor());
            });
            RegisterPanel(MakeTitlebarDescriptor(), []() {
                return create_scope<Titlebar>(MakeTitlebarDescriptor());
            });
            RegisterPanel(MakeHierarchyDescriptor(), []() {
                return create_scope<HierarchyPanel>(MakeHierarchyDescriptor());
            });
            RegisterPanel(MakeInspectorDescriptor(), []() {
                return create_scope<InspectorPanel>(MakeInspectorDescriptor());
            });
            RegisterPanel(MakeProjectDescriptor(), []() {
                return create_scope<ProjectPanel>(MakeProjectDescriptor());
            });
            RegisterPanel(MakeConsoleDescriptor(), []() {
                return create_scope<ConsolePanel>(MakeConsoleDescriptor());
            });
            RegisterPanel(MakeViewportDescriptor(), []() {
                return create_scope<ViewportPanel>(MakeViewportDescriptor());
            });
        }
    } // 

    CakeryLayer::CakeryLayer(const std::string& name) : Layer(name) { }

    void CakeryLayer::enterEditor() {
        if (m_editor_initialized) {
            return;
        }

        auto& context = Application::Self().context();
        context.startRuntime();
        const auto& world = context.world;
        world->registerSimulationSystem(create_ref<SimulationFrameContextSystem>());

        m_editor_initialized = true;
        auto panel_context = buildPanelContext(true);
        m_panel_manager.setWorkspaceActive(true, panel_context);
    }

    EditorPanelContext CakeryLayer::buildPanelContext(const bool workspace_active) {
        auto& system_context = Application::Self().context();

        EditorPanelContext context{
            system_context,
            m_window,
            &m_panel_manager,
            workspace_active,
            [this]() { enterEditor(); }
        };
        return context;
    }

    void CakeryLayer::attach() {
        m_window = Application::Self().context().window_manager->getWindow();
        m_base_title = Application::Self().specification().name;
        m_editor_initialized = false;

        RegisterDefaultEditorPanels();
        EditorPanelRegistry::Self().instantiatePanels(m_panel_manager);
        m_panel_manager.initialize(buildPanelContext(false));
    }

    void CakeryLayer::detach() {
        if (m_window && m_window->getNativeWindow()) {
            glfwSetWindowTitle(m_window->getNativeWindow(), m_base_title.c_str());
        }
        if (m_editor_initialized) {
            Application::Self().context().finalizeRuntime();
        }
        m_panel_manager.shutdown(buildPanelContext(false));
        m_editor_initialized = false;
    }

    void CakeryLayer::updateTick(const float dt) {
        if (m_window && m_window->getNativeWindow()) {
            char title[64]{};
            std::snprintf(title, sizeof(title), "%s - %.1f FPS", m_base_title.c_str(), dt > 0.0f ? 1.0f / dt : 0.0f);
            glfwSetWindowTitle(m_window->getNativeWindow(), title);
        }

        const auto stage = m_editor_initialized ? EditorPanelStage::Workspace : EditorPanelStage::Startup;
        auto panel_context = buildPanelContext(m_editor_initialized);
        m_panel_manager.update(stage, panel_context, dt);
    }

    void CakeryLayer::renderTick() {
        const auto stage = m_editor_initialized ? EditorPanelStage::Workspace : EditorPanelStage::Startup;
        auto panel_context = buildPanelContext(m_editor_initialized);
        m_panel_manager.draw(stage, panel_context);
    }

} // cakery
