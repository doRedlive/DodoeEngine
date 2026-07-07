// do@Redlive

#include "sandbox_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "runtime/function/script/script_system.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/service/world/scene_importer.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/utils/tags.h"


namespace sandbox {

    SandboxLayer::SandboxLayer(const std::string& name)
        : dodoe::Layer(name) {
    }

    void SandboxLayer::attach() {
        auto* world = dodoe::GetWorld();
        auto scene = world->getCurrentScene();

        dodoe::SceneImporter::ImportModel("engine/res/models/backpack/backpack.obj");

    }
    
    void SandboxLayer::detach() {
        
    }
    
    void SandboxLayer::updateTick(const float delta_time) {

    }

    void SandboxLayer::renderTick() {
    }

} // namespace sandbox