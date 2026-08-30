// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/core/layer/layer.h"
#include "sandbox_camera.h"

namespace sandbox {

    class SandboxLayer : public dodoe::Layer {
    public:
        explicit SandboxLayer(const std::string& name);
        ~SandboxLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;

    private:
        std::unique_ptr<SandboxCamera> m_camera;
        SandboxCameraProvider m_provider;
    };

} // sandbox
