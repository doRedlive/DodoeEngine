// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_scene/light_render_object.h"

namespace dodoe {

    class SkyLightSystem : public System {
        std::unordered_set<UUID> m_submitted{};

    public:
        ~SkyLightSystem() override;
        void update(Registry& reg, float dt) override;

    private:
        bool syncSkyLight(Entity entity);
        void pruneRemoved(const std::unordered_set<UUID>& active);
        Ref<Texture> loadCubemap(const DynamicArray<String>& paths);
        Scope<SkyLightRenderObject> buildRenderObject(const Ref<Texture>& cubemap, Float intensity);
    };

} // dodoe
