// do@Redlive

#include "runtime/entry_point.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "cakery_layer.h"

namespace cakery {
    class CakeryApp final : public dodoe::Application {
    public:
        explicit CakeryApp(const dodoe::ApplicationSpecification& spec) : dodoe::Application(spec) {
            m_context->layer_stack.pushLayer(new CakeryLayer("Cakery"));
        }

        ~CakeryApp() override = default;
    };

} // cakery

namespace dodoe {

    Application* CreateApplication(const ApplicationCommandLineArgs cli_args) {
        ApplicationSpecification cakery_spec;
        cakery_spec.name = "Cakery";
        cakery_spec.custom_titlebar = false;
        cakery_spec.window_resizeable = true;
        cakery_spec.width = 1600;
        cakery_spec.height = 900;
        cakery_spec.render_settings.api = RenderBackendApiType::Vulkan;
        cakery_spec.render_settings.pipeline = RenderingPipelineType::Only2D;
        cakery_spec.cli_args = cli_args;
        return new cakery::CakeryApp(cakery_spec);
    }

} // dodoe

