// do@Redlive

#pragma once

#include "dopch.h"

#include "render_object.h"
#include "../framework/primitive_scene_info.h"
#include "../mesh_draw/view_mesh_draw_context.h"
#include "../mesh_draw/mesh_data.h"

namespace dodoe {

    class DrawCommandList;

    struct MeshUploadData {
        String name{};
        DynamicArray<Vector3f> position_data{};
        DynamicArray<Vector2f> texcoord_data{};
        DynamicArray<UInt32> normal_data{};
        DynamicArray<UInt32> index_data{};
    };

    class PrimitiveRenderObject : public RenderObject {
    protected:
        MeshUploadData m_upload_data{};
        DynamicArray<MeshLODData> m_lods{};
        DynamicArray<Ref<Material>> m_override_materials{};
        PrimitiveMobility m_mobility{PrimitiveMobility::Static};
        Bool m_visible{true};
        Bool m_cast_shadow{true};

    public:
        void setUploadData(const MeshUploadData& upload_data) { m_upload_data = upload_data; }
        void setLODData(const DynamicArray<MeshLODData>& lods) { m_lods = lods; }
        void setOverrideMaterials(const DynamicArray<Ref<Material>>& override_materials) { m_override_materials = override_materials; }
        void setMobility(const PrimitiveMobility mobility) { m_mobility = mobility; }
        void setVisible(const Bool visible) { m_visible = visible; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }

        [[nodiscard]] const DynamicArray<MeshLODData>& getLODData() const { return m_lods; }
        [[nodiscard]] const MeshUploadData& getUploadData() const { return m_upload_data; }
        [[nodiscard]] const DynamicArray<Ref<Material>>& getOverrideMaterials() const { return m_override_materials; }
        [[nodiscard]] PrimitiveMobility getMobility() const { return m_mobility; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }
        [[nodiscard]] const String& getMeshName() const { return m_upload_data.name; }

        [[nodiscard]] virtual UInt32 getInstanceCount() const;
        virtual void appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const;

        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
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

        virtual void createResources(GfxDeviceHandle device, DrawCommandList& cmd_list);

    protected:
        [[nodiscard]] const MeshLODData* activeLOD() const;
    };

} // dodoe
