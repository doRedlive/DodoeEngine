//
// Created by GreenMuffin on 2026/2/22.
//

#ifndef DODOE_META_SYSTEM
#define DODOE_META_SYSTEM

#include "component_db.h"
#include "runtime/core/world/components.h"
#include "runtime/core/meta/reflection_register.h"

namespace dodoe {

    class MetaSystem {
    public:
        static void initialize() {
            ComponentDB::instance().register_component<TagComponent>("TagComponent");
            ComponentDB::instance().register_component<TransformComponent>("TransformComponent");
            ComponentDB::instance().register_component<SpriteRendererComponent>("SpriteRendererComponent");
            ComponentDB::instance().register_component<ScriptComponent>("ScriptComponent");

            dodoe::do_reflection_register<dodoe::TagComponent>("TagComponent")
                .field<&dodoe::TagComponent::tag>("tag");

            dodoe::do_reflection_register<TransformComponent>("TransformComponent")
                .field<&TransformComponent::position>("position")
                .field<&TransformComponent::rotation>("rotation")
                .field<&TransformComponent::scale>("scale");

            dodoe::do_reflection_register<SpriteRendererComponent>("SpriteRendererComponent")
                .field<&SpriteRendererComponent::texture_path>("texture_path")
                .field<&SpriteRendererComponent::flip>("flip")
                .field<&SpriteRendererComponent::color>("color");

            dodoe::do_reflection_register<ScriptComponent>("ScriptComponent")
                .field<&ScriptComponent::class_name>("class_name");
        }
    };

} // dodoe

#endif//DODOE_META_SYSTEM
