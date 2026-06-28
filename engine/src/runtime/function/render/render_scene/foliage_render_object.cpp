// do@Redlive

#include "foliage_render_object.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    namespace {
        struct ClusterKey {
            Int32 x{0};
            Int32 z{0};

            Bool operator==(const ClusterKey& other) const {
                return x == other.x && z == other.z;
            }
        };

        struct ClusterKeyHasher {
            Size_t operator()(const ClusterKey& key) const {
                return (static_cast<Size_t>(static_cast<UInt32>(key.x)) << 32) ^ static_cast<Size_t>(static_cast<UInt32>(key.z));
            }
        };

        struct ClusterBuildEntry {
            FoliageRenderObject::Cluster cluster{};
            DynamicArray<FoliageRenderInstanceData> instances{};
        };

        Int32 computeClusterCoordinate(const Float value, const Float cluster_size) {
            return static_cast<Int32>(std::floor(value / cluster_size));
        }

        Matrix4f BuildLocalMatrix(const Vector3f& translation, const Vector3f& rotation, const Vector3f& scale) {
            Matrix4f model(1.0f);
            model = Math::Translate(model, translation);
            model = Math::Rotate(model, Math::Radians(rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
            model = Math::Rotate(model, Math::Radians(rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
            model = Math::Rotate(model, Math::Radians(rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
            model = Math::Scale(model, scale);
            return model;
        }

    } // namespace

    void FoliageRenderObject::applyType(const FoliageRenderType& type) {
        setUploadData(type.upload_data);
        setLODData(type.lods);
        setOverrideMaterials(type.override_materials);
        setMobility(type.mobility);
        setVisible(type.visible);
        setCastShadow(type.cast_shadow);
        setInstanceBoundsExtent(type.instance_bounds_extent);
    }

    void FoliageRenderObject::setInstanceBoundsExtent(const Vector3f& instance_bounds_extent) {
        m_instance_bounds_extent = instance_bounds_extent;
        rebuildClusters();
    }

    void FoliageRenderObject::setInstances(const DynamicArray<FoliageRenderInstanceData>& instances) {
        m_instances = instances;
        rebuildClusters();
    }

    UInt32 FoliageRenderObject::getInstanceCount() const {
        return static_cast<UInt32>(m_instances.size());
    }

    void FoliageRenderObject::appendInstanceSceneData(
        DynamicArray<InstanceSceneData>& out_instance_scene_data,
        const Matrix4f& world_transform) const
    {
        for (const auto& instance : m_instances) {
            InstanceSceneData scene_instance{};
            scene_instance.model = buildInstanceWorldTransform(instance, world_transform);
            scene_instance.color_tint = instance.color_tint;
            scene_instance.params = Vector4f(instance.wind_phase, instance.variation, 1.0f, 1.0f);
            out_instance_scene_data.push_back(scene_instance);
        }
    }

    RenderObjectDirtyFlags FoliageRenderObject::diff(const RenderObject& previous) const {
        const RenderObjectDirtyFlags base_dirty_flags = PrimitiveRenderObject::diff(previous);
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return base_dirty_flags;
        }

        const auto& previous_foliage = static_cast<const FoliageRenderObject&>(previous);
        if (m_instance_bounds_extent != previous_foliage.m_instance_bounds_extent || m_instances.size() != previous_foliage.m_instances.size()) {
            return base_dirty_flags | RenderObjectDirtyFlags::ProxyData;
        }

        for (Size_t instance_index = 0; instance_index < m_instances.size(); instance_index++) {
            const auto& lhs = m_instances[instance_index];
            const auto& rhs = previous_foliage.m_instances[instance_index];
            if (lhs.position != rhs.position ||
                lhs.rotation != rhs.rotation ||
                lhs.scale != rhs.scale ||
                lhs.color_tint != rhs.color_tint ||
                lhs.wind_phase != rhs.wind_phase ||
                lhs.variation != rhs.variation)
            {
                return base_dirty_flags | RenderObjectDirtyFlags::ProxyData;
            }
        }

        return base_dirty_flags;
    }

    DynamicArray<MeshBatch> FoliageRenderObject::buildMeshBatches(
        const Identifier primitive_id,
        const DynamicArray<Ref<Material>>& resolved_materials,
        const UInt32 first_instance) const
    {
        const auto* lod = activeLOD();
        if (m_instances.empty() || !lod || !lod->isValid()) {
            return {};
        }

        DynamicArray<MeshBatch> batches{};
        batches.reserve(lod->sections.size() * (m_clusters.empty() ? 1 : m_clusters.size()));
        DynamicArray<Cluster> fallback_clusters{};
        if (m_clusters.empty()) {
            fallback_clusters.push_back(Cluster{0, static_cast<UInt32>(m_instances.size()), -m_instance_bounds_extent, m_instance_bounds_extent});
        }
        const DynamicArray<Cluster>& clusters = m_clusters.empty() ? fallback_clusters : m_clusters;

        for (Size_t section_index = 0; section_index < lod->sections.size(); section_index++) {
            const auto& mesh_section = lod->sections[section_index];
            if (mesh_section.index_count == 0) {
                continue;
            }

            for (const auto& cluster : clusters) {
                if (cluster.instance_count == 0) {
                    continue;
                }

                MeshBatchElement element{};
                element.index_count = mesh_section.index_count;
                element.index_offset = mesh_section.index_offset;
                element.vertex_offset = mesh_section.vertex_offset;
                element.section_index = static_cast<UInt32>(section_index);
                element.uses_instance_range = true;
                element.first_instance = first_instance + cluster.first_instance;
                element.instance_count = cluster.instance_count;
                element.vertex_buffer = lod->buffers.vertex_buffer;
                element.index_buffer = lod->buffers.index_buffer;

                MeshBatch batch{};
                batch.primitive_id = primitive_id;
                batch.material = section_index < resolved_materials.size() ? resolved_materials[section_index] : mesh_section.material;
                batch.pass_mask.setRelevant(MeshPassType::GBuffer, m_visible);
                batch.pass_mask.setRelevant(MeshPassType::DirectionalShadow, m_visible && m_cast_shadow);
                batch.uses_custom_bounds = true;
                batch.bounds_min = cluster.bounds_min;
                batch.bounds_max = cluster.bounds_max;
                batch.elements.push_back(element);
                batches.push_back(std::move(batch));
            }
        }
        return batches;
    }

    PrimitiveSceneInfo FoliageRenderObject::buildSceneInfo(
        const Identifier primitive_id,
        const Matrix4f& world_transform,
        const Vector3f& bounds_min,
        const Vector3f& bounds_max) const
    {
        (void)bounds_min;
        (void)bounds_max;

        Vector3f local_bounds_min{0.0f};
        Vector3f local_bounds_max{0.0f};
        computeLocalBounds(local_bounds_min, local_bounds_max);
        return PrimitiveRenderObject::buildSceneInfo(primitive_id, world_transform, local_bounds_min, local_bounds_max);
    }

    Matrix4f FoliageRenderObject::buildInstanceWorldTransform(
        const FoliageRenderInstanceData& instance,
        const Matrix4f& world_transform) const
    {
        return world_transform * BuildLocalMatrix(instance.position, instance.rotation, instance.scale);
    }

    void FoliageRenderObject::computeLocalBounds(Vector3f& out_bounds_min, Vector3f& out_bounds_max) const {
        if (m_clusters.empty()) {
            out_bounds_min = -m_instance_bounds_extent;
            out_bounds_max = m_instance_bounds_extent;
            return;
        }

        out_bounds_min = m_clusters.front().bounds_min;
        out_bounds_max = m_clusters.front().bounds_max;
        for (const auto& cluster : m_clusters) {
            out_bounds_min = Vector3f(
                (std::min)(out_bounds_min.x, cluster.bounds_min.x),
                (std::min)(out_bounds_min.y, cluster.bounds_min.y),
                (std::min)(out_bounds_min.z, cluster.bounds_min.z)
            );
            out_bounds_max = Vector3f(
                (std::max)(out_bounds_max.x, cluster.bounds_max.x),
                (std::max)(out_bounds_max.y, cluster.bounds_max.y),
                (std::max)(out_bounds_max.z, cluster.bounds_max.z)
            );
        }
    }

    void FoliageRenderObject::rebuildClusters() {
        m_clusters.clear();
        if (m_instances.empty()) {
            return;
        }

        const Float cluster_size_x = (std::max)(m_cluster_grid_size.x, 1.0f);
        const Float cluster_size_z = (std::max)(m_cluster_grid_size.y, 1.0f);
        std::unordered_map<ClusterKey, ClusterBuildEntry, ClusterKeyHasher> cluster_map{};
        cluster_map.reserve(m_instances.size());

        for (const auto& instance : m_instances) {
            ClusterKey key{
                computeClusterCoordinate(instance.position.x, cluster_size_x),
                computeClusterCoordinate(instance.position.z, cluster_size_z)
            };
            auto& entry = cluster_map[key];
            if (entry.instances.empty()) {
                const Vector3f extent = m_instance_bounds_extent * instance.scale;
                entry.cluster.bounds_min = instance.position - extent;
                entry.cluster.bounds_max = instance.position + extent;
            } else {
                const Vector3f extent = m_instance_bounds_extent * instance.scale;
                const Vector3f instance_min = instance.position - extent;
                const Vector3f instance_max = instance.position + extent;
                entry.cluster.bounds_min = Vector3f(
                    (std::min)(entry.cluster.bounds_min.x, instance_min.x),
                    (std::min)(entry.cluster.bounds_min.y, instance_min.y),
                    (std::min)(entry.cluster.bounds_min.z, instance_min.z)
                );
                entry.cluster.bounds_max = Vector3f(
                    (std::max)(entry.cluster.bounds_max.x, instance_max.x),
                    (std::max)(entry.cluster.bounds_max.y, instance_max.y),
                    (std::max)(entry.cluster.bounds_max.z, instance_max.z)
                );
            }
            entry.instances.push_back(instance);
        }

        DynamicArray<ClusterBuildEntry> ordered_clusters{};
        ordered_clusters.reserve(cluster_map.size());
        for (auto& [key, entry] : cluster_map) {
            (void)key;
            ordered_clusters.push_back(std::move(entry));
        }
        std::sort(ordered_clusters.begin(), ordered_clusters.end(), [](const ClusterBuildEntry& lhs, const ClusterBuildEntry& rhs) {
            const Float lhs_key = lhs.cluster.bounds_min.x + lhs.cluster.bounds_min.z;
            const Float rhs_key = rhs.cluster.bounds_min.x + rhs.cluster.bounds_min.z;
            return lhs_key < rhs_key;
        });

        DynamicArray<FoliageRenderInstanceData> ordered_instances{};
        ordered_instances.reserve(m_instances.size());
        UInt32 first_instance = 0;
        for (auto& entry : ordered_clusters) {
            entry.cluster.first_instance = first_instance;
            entry.cluster.instance_count = static_cast<UInt32>(entry.instances.size());
            first_instance += entry.cluster.instance_count;
            for (auto& instance : entry.instances) {
                ordered_instances.push_back(std::move(instance));
            }
            m_clusters.push_back(entry.cluster);
        }

        m_instances = std::move(ordered_instances);
    }

} // dodoe
