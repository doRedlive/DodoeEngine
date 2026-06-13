// do@Redlive

#include "dopch.h"
#include "runtime/core/layer/layer.h"

namespace sandbox {

    class SandboxLayer : public dodoe::Layer {
    public:
        explicit SandboxLayer(const std::string& name);
        ~SandboxLayer() override = default;

        void attach() override;
        void detach() override;
        void updateTick(float delta_time) override;
        void renderTick() override;
    };

} // sandbox

