// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class RenderObject;
    class RenderScene;

    class MeshRendererSystem : public System {
        std::unordered_set<Uuid> m_submitted_nodes;

    public:
        ~MeshRendererSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncNode(RenderScene& render_scene, Entity entity);
        bool syncRenderObject(RenderScene& render_scene, Entity entity);
        bool removeDetachedRenderObject(RenderScene& render_scene, Entity entity);
        bool pruneRemovedNodes(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_nodes);

        [[nodiscard]] static bool needsNodeSync(const RenderScene& render_scene, Entity entity, const std::unordered_set<Uuid>& submitted_nodes);
        [[nodiscard]] static bool needsRenderObjectSync(const RenderScene& render_scene, Entity entity);
        [[nodiscard]] static Uuid resolveParentUuid(Entity entity);
        [[nodiscard]] static Scope<RenderObject> buildRenderObject(const MeshRendererComponent& mesh);
    };

} // dodoe
