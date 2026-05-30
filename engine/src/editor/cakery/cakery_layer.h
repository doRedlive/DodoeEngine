// do@Redlive

#pragma once

#include "dopch.h"

#include "cakery/framework/editor_panel_manager.h"

#include "runtime/core/layer/layer.h"
#include "runtime/function/window/window.h"

namespace cakery {
    class CakeryLayer final : public dodoe::Layer {
    public:
        explicit CakeryLayer(const std::string& name);
        ~CakeryLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;

    private:
        void enterEditor();
        [[nodiscard]] EditorPanelContext buildPanelContext(bool workspace_active);

        dodoe::Window* m_window{ nullptr };
        std::string m_base_title{};
        bool m_editor_initialized{false};

        EditorPanelManager m_panel_manager{};
    };

} // cakery
