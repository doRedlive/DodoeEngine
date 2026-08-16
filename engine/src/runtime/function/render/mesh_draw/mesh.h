#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"

namespace dodoe {

    class DODOE_API Mesh : public Object {
        String m_path{};
        String m_name{};
        DynamicArray<MeshLODData> m_lods{};
        Vector3f m_bounds_min{0.0f};
        Vector3f m_bounds_max{0.0f};

    public:
        Mesh() = default;
        explicit Mesh(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Mesh"; }

        void setPath(const String& path) { m_path = path; }
        void setName(const String& name) { m_name = name; }
        void setLODData(const DynamicArray<MeshLODData>& lods) { m_lods = lods; }
        void setBounds(const Vector3f& bounds_min, const Vector3f& bounds_max) {
            m_bounds_min = bounds_min;
            m_bounds_max = bounds_max;
        }

        [[nodiscard]] const String& getPath() const { return m_path; }
        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] const DynamicArray<MeshLODData>& getLODData() const { return m_lods; }
        [[nodiscard]] const Vector3f& getBoundsMin() const { return m_bounds_min; }
        [[nodiscard]] const Vector3f& getBoundsMax() const { return m_bounds_max; }

        [[nodiscard]] static Mesh* Create(const ObjectID& ref, const String& path);
        static void Shutdown();
    };

} // namespace dodoe
