// do@Redlive

#pragma once

#include "dopch.h"

#include "imgui/imgui.h"

namespace dodoe {
    class SystemContext;
    class Window;
}

namespace cakery {

    using dodoe::Bool;
    using dodoe::String;
    using dodoe::Int32;
    using dodoe::UInt32;
    using dodoe::UInt64;
    using dodoe::Size_t;
    using dodoe::DynamicArray;
    using dodoe::UnorderedMap;
    using dodoe::Scope;
    using dodoe::Ref;

    enum class EditorPanelStage {
        Startup,
        Workspace,
        Shared
    };

    enum class EditorDockPlacement {
        None,
        Left,
        Right,
        Bottom,
        Center
    };

    struct EditorPanelDock {
        EditorDockPlacement placement{EditorDockPlacement::None};
        float split_ratio{0.25f};
    };

    struct EditorPanelDescriptor {
        std::string id{};
        std::string title{};
        std::string category{"Panels"};
        EditorPanelStage stage{EditorPanelStage::Workspace};
        EditorPanelDock default_dock{};
        bool default_open{true};
        bool closable{true};
        bool requires_runtime{false};
        bool show_in_view_menu{true};
        int order{0};
    };

    class EditorPanelManager;

    struct EditorPanelContext {
        dodoe::SystemContext& system_context;
        dodoe::Window* window{nullptr};
        EditorPanelManager* panel_manager{nullptr};
        bool workspace_active{false};
        std::function<void()> request_enter_editor{};
    };

    class EditorPanel {
    protected:
        EditorPanelDescriptor m_descriptor;
        Bool m_is_open{true};

    public:
        explicit EditorPanel(EditorPanelDescriptor descriptor);
        virtual ~EditorPanel() = default;

        [[nodiscard]] const EditorPanelDescriptor& getDescriptor() const { return m_descriptor; }
        [[nodiscard]] Bool isOpen() const { return m_is_open; }
        void setOpen(const Bool open) { m_is_open = open; }

        virtual void onAttach(const EditorPanelContext& context);
        virtual void onDetach(const EditorPanelContext& context);
        virtual void onWorkspaceActivated(const EditorPanelContext& context);
        virtual void onWorkspaceDeactivated(const EditorPanelContext& context);
        virtual void onUpdate(const EditorPanelContext& context, float delta_time);
        virtual void onDraw(const EditorPanelContext& context) = 0;
        [[nodiscard]] virtual ImGuiID getDockspaceId() const;

    };

} // cakery
