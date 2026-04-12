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

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;

    private:
    };

} // cakery

#endif // !CAKERY_SANDBOX_LAYER_H
