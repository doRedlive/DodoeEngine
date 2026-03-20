//
// Created by GreenMuffin on xxxx/xx/xx.
//

#include "cakery_layer.h"
#include "sandbox_layer.h"

#include "cakery_helper.h"

namespace cakery {
    class CakeryApp final : public dodoe::Application {
    public:
        CakeryApp(const dodoe::ApplicationSpecification& spec) : dodoe::Application(spec) {
            push_layer(new SandboxLayer("Sandbox"));
            // push_layer(new CakeryLayer("Cakery"));
        }

        ~CakeryApp() override = default;
    };

}

namespace dodoe {
    class Application;
    Application* create_application(ApplicationCommandLineArgs cli_args) {

        ApplicationSpecification cakery_spec;
        cakery_spec.name = "dodoe";
        cakery_spec.custom_titlebar = true;
        cakery_spec.window_resizeable = true;
        cakery_spec.width = 1600;
        cakery_spec.height = 900;
        cakery_spec.cli_args = cli_args;

        return new cakery::CakeryApp(cakery_spec);
    }
}

