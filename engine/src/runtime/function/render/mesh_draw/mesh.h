#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"
#include "runtime/resource/parser/mesh_blob.h"

namespace dodoe {

    class MeshAsset;
    class Skeleton;

    class DODOE_API Mesh : public Object {
        String m_path{};
        String m_name{};
        DynamicArray<MeshLODData> m_lods{};
        Vector3f m_bounds_min{0.0f};
        Vector3f m_bounds_max{0.0f};
        Ref<MeshData> m_source{};
        PPtr<Skeleton> m_skeleton{};
        bool m_skinned{false};

    public:
        Mesh() = default;
        explicit Mesh(const ObjectID& id)
            : Object(id) {}
        ~Mesh() override;

        [[nodiscard]] const char* getObjectTypeName() const override { return "Mesh"; }

        void setPath(const String& path) { m_path = path; }
        void setName(const String& name) { m_name = name; }
        void setLODData(const DynamicArray<MeshLODData>& lods) { m_lods = lods; }
        void setBounds(const Vector3f& bounds_min, const Vector3f& bounds_max) {
            m_bounds_min = bounds_min;
            m_bounds_max = bounds_max;
        }
        void setSourceData(Ref<MeshData> data) { m_source = std::move(data); }
        void setSkeleton(const PPtr<Skeleton>& skeleton) { m_skeleton = skeleton; }
        void setSkinned(bool skinned) { m_skinned = skinned; }

        [[nodiscard]] const String& getPath() const { return m_path; }
        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] const DynamicArray<MeshLODData>& getLODData() const { return m_lods; }
        [[nodiscard]] const Vector3f& getBoundsMin() const { return m_bounds_min; }
        [[nodiscard]] const Vector3f& getBoundsMax() const { return m_bounds_max; }
        [[nodiscard]] const Ref<MeshData>& getSourceData() const { return m_source; }
        [[nodiscard]] const PPtr<Skeleton>& getSkeleton() const { return m_skeleton; }
        [[nodiscard]] bool isSkinned() const { return m_skinned; }

        [[nodiscard]] static Mesh* Create(const ObjectID& ref, MeshAsset& asset);
        static void Shutdown();
    };

} // namespace dodoe
