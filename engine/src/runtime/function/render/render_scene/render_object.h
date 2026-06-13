#pragma once

#include "dopch.h"

#include "framework/primitive_scene_info.h"
#include "mesh_draw/view_mesh_draw_context.h"

namespace dodoe {

    enum class RenderObjectType : UInt8 {
        StaticMesh,
        Foliage
    };

    enum class RenderObjectDirtyFlags : UInt32 {
        None = 0,
        Mesh = 1 << 0,
        Materials = 1 << 1,
        State = 1 << 2,
        ProxyData = 1 << 3,
        All = Mesh | Materials | State | ProxyData
    };

    inline RenderObjectDirtyFlags operator|(const RenderObjectDirtyFlags lhs, const RenderObjectDirtyFlags rhs) {
        return static_cast<RenderObjectDirtyFlags>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline RenderObjectDirtyFlags& operator|=(RenderObjectDirtyFlags& lhs, const RenderObjectDirtyFlags rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const RenderObjectDirtyFlags lhs, const RenderObjectDirtyFlags rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

    class RenderObject {
    protected:
        Ref<Mesh> m_mesh{};
        DynamicArray<Ref<Material>> m_override_materials{};
        PrimitiveMobility m_mobility{PrimitiveMobility::Static};
        Bool m_visible{true};
        Bool m_cast_shadow{true};

    public:
        virtual ~RenderObject() = default;

        void setMesh(const Ref<Mesh>& mesh) { m_mesh = mesh; }
        void setOverrideMaterials(const DynamicArray<Ref<Material>>& override_materials) { m_override_materials = override_materials; }
        void setMobility(const PrimitiveMobility mobility) { m_mobility = mobility; }
        void setVisible(const Bool visible) { m_visible = visible; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }

        [[nodiscard]] const Ref<Mesh>& getMesh() const { return m_mesh; }
        [[nodiscard]] const DynamicArray<Ref<Material>>& getOverrideMaterials() const { return m_override_materials; }
        [[nodiscard]] PrimitiveMobility getMobility() const { return m_mobility; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }

        [[nodiscard]] virtual RenderObjectType getRenderObjectType() const = 0;
        [[nodiscard]] virtual UInt32 getInstanceCount() const;
        virtual void appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const;

        [[nodiscard]] virtual RenderObjectDirtyFlags diff(const RenderObject& previous) const;
        [[nodiscard]] virtual DynamicArray<Ref<Material>> resolveMaterials() const;
        [[nodiscard]] virtual DynamicArray<PrimitiveSceneInfo::Section> buildSections(const DynamicArray<Ref<Material>>& resolved_materials) const;
        [[nodiscard]] virtual DynamicArray<MeshBatch> buildMeshBatches(
            Identifier primitive_id,
            const DynamicArray<Ref<Material>>& resolved_materials,
            UInt32 first_instance) const;
        [[nodiscard]] virtual PrimitiveSceneInfo buildSceneInfo(
            Identifier primitive_id,
            const Matrix4f& world_transform,
            const Vector3f& bounds_min,
            const Vector3f& bounds_max) const;
    };

} // dodoe
