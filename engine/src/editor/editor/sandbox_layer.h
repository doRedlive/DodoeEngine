//
// Created by GreenMuffin on 2026/3/5.
//

#ifndef CAKERY_SANDBOX_LAYER_H
#define CAKERY_SANDBOX_LAYER_H

#include "dopch.h"
#include "runtime/core/layer/layer.h"

namespace cakery {

    class SandboxLayer : public dodoe::Layer {
    public:
        explicit SandboxLayer(const std::string& name);
        ~SandboxLayer() override = default;

        void on_attach() override;
        void on_detach() override;
        void on_update(float delta_time) override;
        void on_render() override;

    private:
    };

} // cakery

#endif // !CAKERY_SANDBOX_LAYER_H
