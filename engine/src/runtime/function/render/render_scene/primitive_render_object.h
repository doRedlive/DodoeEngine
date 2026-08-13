#pragma once

#include "dopch.h"

#include "render_object.h"
#include "primitive_scene_info.h"
#include "../mesh_draw/mesh_draw_types.h"
#include "../mesh_draw/mesh_data.h"

namespace dodoe {

    class DrawCommandList;
    class Mesh;
    class Material;

    class PrimitiveRenderObject : public RenderObject {
    protected:
        const Mesh* m_mesh{nullptr};
        Int32 m_section_index{-1};
        DynamicArray<PPtr<Material>> m_override_materials{};
        PrimitiveMobility m_mobility{PrimitiveMobility::Static};
        Bool m_visible{true};
        Bool m_cast_shadow{true};

    public:
        void setMesh(const Mesh* mesh, const Int32 section_index = -1);
        void setOverrideMaterials(const DynamicArray<PPtr<Material>>& override_materials) { m_override_materials = override_materials; }
        void setMobility(const PrimitiveMobility mobility) { m_mobility = mobility; }
        void setVisible(const Bool visible) { m_visible = visible; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }

        [[nodiscard]] const Mesh* getMesh() const { return m_mesh; }
        [[nodiscard]] Int32 getSectionIndex() const { return m_section_index; }
        [[nodiscard]] const DynamicArray<PPtr<Material>>& getOverrideMaterials() const { return m_override_materials; }
        [[nodiscard]] PrimitiveMobility getMobility() const { return m_mobility; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }

        [[nodiscard]] virtual UInt32 getInstanceCount() const;
        virtual void appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const;

        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
        [[nodiscard]] virtual DynamicArray<PPtr<Material>> resolveMaterials() const;
        [[nodiscard]] virtual DynamicArray<SubMesh> buildSections(const DynamicArray<PPtr<Material>>& resolved_materials) const;
        [[nodiscard]] virtual DynamicArray<MeshBatch> buildMeshBatches(
            Identifier primitive_id,
            const DynamicArray<PPtr<Material>>& resolved_materials,
            UInt32 first_instance) const;
        [[nodiscard]] virtual PrimitiveSceneInfo buildSceneInfo(
            Identifier primitive_id,
            const Matrix4f& world_transform,
            const Vector3f& bounds_min,
            const Vector3f& bounds_max) const;

        virtual bool createResources(DrawCommandList& cmd_list);

    protected:
        [[nodiscard]] const MeshLODData* activeLOD() const;
    };

} // dodoe
