// do@Redlive

#include "gpu_scene.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    Bool GpuScene::initialize(const GpuSceneCreateInfo& info) {
        (void)info;
        return true;
    }

    void GpuScene::shutdown() {
        m_quad_ib = nullptr;
        m_quad_vb = nullptr;
        m_primitive_instance_buffer = nullptr;
        m_sprite_instance_buffer = nullptr;
        m_bounds_buffer = nullptr;
        m_transforms_buffer = nullptr;
        m_object_meta_buffer = nullptr;
        m_objects.clear();
        m_dirty_flags.clear();
        m_transforms_cpu.clear();
        m_bounds_cpu.clear();
        m_sprite_instance_cpu.clear();
        m_primitive_instance_cpu.clear();
        m_object_capacity = 0;
        m_transform_capacity = 0;
        m_bounds_capacity = 0;
        m_sprite_instance_capacity = 0;
        m_primitive_instance_capacity = 0;
        m_quad_buffers_ready = false;
        m_last_upload_ranges.clear();
        m_meta_dirty_range.reset();
        m_transform_dirty_range.reset();
        m_bounds_dirty_range.reset();
        m_sprite_instance_dirty_range.reset();
        m_primitive_instance_dirty_range.reset();
    }

    GpuObjectHandle GpuScene::registerObject(const GpuObjectType type, GpuObjectMeta meta) {
        meta.type = static_cast<UInt32>(type);
        const UInt32 idx = m_objects.occupiedCount();
        ensureObjectMetaBuffer(idx + 1, GDrawCommandList);
        ensureTransformsBuffer(idx + 1, GDrawCommandList);
        ensureBoundsBuffer(idx + 1, GDrawCommandList);

        auto handle = m_objects.insert(meta);

        while (m_dirty_flags.size() <= idx) {
            m_dirty_flags.push_back(GpuObjectDirtyFlags::None);
        }
        m_dirty_flags[idx] = GpuObjectDirtyFlags::All;

        m_meta_dirty_range.expand(idx);
        m_transform_dirty_range.expand(idx);
        m_bounds_dirty_range.expand(idx);

        switch (type) {
        case GpuObjectType::Sprite:
            ensureSpriteInstanceBuffer(idx + 1, GDrawCommandList);
            m_sprite_instance_dirty_range.expand(idx);
            break;
        case GpuObjectType::Primitive:
            ensurePrimitiveInstanceBuffer(idx + 1, GDrawCommandList);
            m_primitive_instance_dirty_range.expand(idx);
            break;
        default:
            break;
        }

        return handle;
    }

    void GpuScene::unregisterObject(const GpuObjectHandle handle) {
        if (!handle.valid()) return;
        const UInt32 idx = handle.index();
        m_meta_dirty_range.expand(idx);
        m_objects.remove(handle);
        if (idx < m_dirty_flags.size()) {
            m_dirty_flags[idx] = GpuObjectDirtyFlags::None;
        }
    }

    void GpuScene::markDirty(const GpuObjectHandle handle, const GpuObjectDirtyFlags flags) {
        const UInt32 idx = handle.index();
        if (!handle.valid() || idx >= m_dirty_flags.size()) return;

        m_dirty_flags[idx] |= flags;

        if (HasAnyFlags(flags, GpuObjectDirtyFlags::ObjectMeta)) {
            m_meta_dirty_range.expand(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::Transform)) {
            m_transform_dirty_range.expand(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::Bounds)) {
            m_bounds_dirty_range.expand(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::InstanceData)) {
            const auto* meta = m_objects.get(handle);
            if (meta) {
                const auto type = static_cast<GpuObjectType>(meta->type);
                if (type == GpuObjectType::Sprite) {
                    m_sprite_instance_dirty_range.expand(idx);
                } else if (type == GpuObjectType::Primitive) {
                    m_primitive_instance_dirty_range.expand(idx);
                }
            }
        }
    }

    void GpuScene::updateTransform(const GpuObjectHandle handle, const Matrix4f& local_to_world) {
        const UInt32 idx = handle.index();
        if (!handle.valid() || idx >= m_transform_capacity) return;
        ensureTransformsBuffer(idx + 1, GDrawCommandList);
        m_transforms_cpu[idx].local_to_world = local_to_world;
        m_transform_dirty_range.expand(idx);
    }

    void GpuScene::updateBounds(const GpuObjectHandle handle, const Vector3f& center, const Vector3f& extent) {
        const UInt32 idx = handle.index();
        if (!handle.valid() || idx >= m_bounds_capacity) return;
        ensureBoundsBuffer(idx + 1, GDrawCommandList);
        m_bounds_cpu[idx].center = center;
        m_bounds_cpu[idx].extent = extent;
        m_bounds_cpu[idx].sphere_radius = Math::Length(extent);
        m_bounds_dirty_range.expand(idx);
    }

    void GpuScene::updateSpriteInstance(const GpuObjectHandle handle, const SpriteGpuData& data) {
        const UInt32 idx = handle.index();
        if (!handle.valid() || idx >= m_sprite_instance_capacity) return;
        ensureSpriteInstanceBuffer(idx + 1, GDrawCommandList);
        m_sprite_instance_cpu[idx] = data;
        m_sprite_instance_dirty_range.expand(idx);

        auto* meta = m_objects.get(handle);
        if (meta) {
            meta->texture_id = data.atlas_index;
            meta->material_id = data.material_id;
            m_meta_dirty_range.expand(idx);
        }
    }

    void GpuScene::updatePrimitiveInstance(const GpuObjectHandle handle, const PrimitiveGpuData& data) {
        const UInt32 idx = handle.index();
        if (!handle.valid() || idx >= m_primitive_instance_capacity) return;
        ensurePrimitiveInstanceBuffer(idx + 1, GDrawCommandList);
        m_primitive_instance_cpu[idx] = data;
        m_primitive_instance_dirty_range.expand(idx);
    }

    void GpuScene::flushDirtyRange(DrawCommandList& cmd_list, DirtyRange& range,
                                    GfxBufferHandle buffer, GfxResourceStates target_state,
                                    const void* cpu_data, UInt32 element_size) {
        if (!range.valid() || !buffer) return;

        const UInt32 offset_bytes = range.start * element_size;
        const UInt32 size_bytes = (range.end - range.start) * element_size;

        cmd_list.setBufferState(buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(buffer, static_cast<const UInt8*>(cpu_data) + offset_bytes, size_bytes, offset_bytes);
        cmd_list.setBufferState(buffer, target_state);
        cmd_list.commitBarriers();

        m_last_upload_ranges.push_back({buffer, offset_bytes, size_bytes});
        range.reset();
    }

    void GpuScene::flushUpdates(DrawCommandList& cmd_list) {
        m_last_upload_ranges.clear();

        ensureQuadBuffers(cmd_list);

        if (m_meta_dirty_range.valid() && m_object_meta_buffer) {
            DynamicArray<GpuObjectMeta> metas(m_objects.slotCount());
            m_objects.forEachOccupied([&](GpuObjectHandle h, const GpuObjectMeta& meta) {
                if (h.index() < metas.size()) {
                    metas[h.index()] = meta;
                }
            });
            flushDirtyRange(cmd_list, m_meta_dirty_range, m_object_meta_buffer,
                           GfxResourceStates::ShaderResource, metas.data(), sizeof(GpuObjectMeta));
        }

        if (m_transform_dirty_range.valid() && m_transforms_buffer) {
            flushDirtyRange(cmd_list, m_transform_dirty_range, m_transforms_buffer,
                           GfxResourceStates::ShaderResource,
                           m_transforms_cpu.data(), sizeof(GpuTransform));
        }

        if (m_bounds_dirty_range.valid() && m_bounds_buffer) {
            flushDirtyRange(cmd_list, m_bounds_dirty_range, m_bounds_buffer,
                           GfxResourceStates::ShaderResource,
                           m_bounds_cpu.data(), sizeof(GpuBounds));
        }

        if (m_sprite_instance_dirty_range.valid() && m_sprite_instance_buffer) {
            flushDirtyRange(cmd_list, m_sprite_instance_dirty_range, m_sprite_instance_buffer,
                           GfxResourceStates::VertexBuffer,
                           m_sprite_instance_cpu.data(), sizeof(SpriteGpuData));
        }

        if (m_primitive_instance_dirty_range.valid() && m_primitive_instance_buffer) {
            flushDirtyRange(cmd_list, m_primitive_instance_dirty_range, m_primitive_instance_buffer,
                           GfxResourceStates::VertexBuffer,
                           m_primitive_instance_cpu.data(), sizeof(PrimitiveGpuData));
        }
    }

    GpuScenePassResources GpuScene::getPassResources() const {
        GpuScenePassResources res{};
        res.object_meta = m_object_meta_buffer;
        res.transforms = m_transforms_buffer;
        res.bounds = m_bounds_buffer;
        res.sprite_instance = m_sprite_instance_buffer;
        res.primitive_instance = m_primitive_instance_buffer;
        res.quad_vb = m_quad_vb;
        res.quad_ib = m_quad_ib;
        return res;
    }

    void GpuScene::ensureObjectMetaBuffer(const UInt32 capacity, DrawCommandList& cmd_list) {
        if (capacity <= m_object_capacity && m_object_meta_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_object_capacity * 2, 64u));
        m_object_meta_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuObjectMeta)))
                .setStructStride(sizeof(GpuObjectMeta))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene ObjectMeta"));
        m_object_capacity = new_cap;
        m_meta_dirty_range.reset();
        m_meta_dirty_range.expand(0);
        m_meta_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureTransformsBuffer(const UInt32 capacity, DrawCommandList& cmd_list) {
        if (capacity <= m_transform_capacity && m_transforms_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_transform_capacity * 2, 64u));
        m_transforms_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuTransform)))
                .setStructStride(sizeof(GpuTransform))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene Transforms"));
        m_transforms_cpu.resize(new_cap);
        m_transform_capacity = new_cap;
        m_transform_dirty_range.reset();
        m_transform_dirty_range.expand(0);
        m_transform_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureBoundsBuffer(const UInt32 capacity, DrawCommandList& cmd_list) {
        if (capacity <= m_bounds_capacity && m_bounds_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_bounds_capacity * 2, 64u));
        m_bounds_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuBounds)))
                .setStructStride(sizeof(GpuBounds))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene Bounds"));
        m_bounds_cpu.resize(new_cap);
        m_bounds_capacity = new_cap;
        m_bounds_dirty_range.reset();
        m_bounds_dirty_range.expand(0);
        m_bounds_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureSpriteInstanceBuffer(const UInt32 capacity, DrawCommandList& cmd_list) {
        if (capacity <= m_sprite_instance_capacity && m_sprite_instance_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_sprite_instance_capacity * 2, 64u));
        m_sprite_instance_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(SpriteGpuData)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene SpriteInstance"));
        m_sprite_instance_cpu.resize(new_cap);
        m_sprite_instance_capacity = new_cap;
        m_sprite_instance_dirty_range.reset();
        m_sprite_instance_dirty_range.expand(0);
        m_sprite_instance_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensurePrimitiveInstanceBuffer(const UInt32 capacity, DrawCommandList& cmd_list) {
        if (capacity <= m_primitive_instance_capacity && m_primitive_instance_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_primitive_instance_capacity * 2, 64u));
        m_primitive_instance_buffer = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(PrimitiveGpuData)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene PrimitiveInstance"));
        m_primitive_instance_cpu.resize(new_cap);
        m_primitive_instance_capacity = new_cap;
        m_primitive_instance_dirty_range.reset();
        m_primitive_instance_dirty_range.expand(0);
        m_primitive_instance_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureQuadBuffers(DrawCommandList& cmd_list) {
        if (m_quad_buffers_ready) return;

        m_quad_vb = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(kQuadVertices)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene QuadVB"));

        m_quad_ib = cmd_list.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(kQuadIndices)))
                .setIsIndexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
                .setDebugName("GpuScene QuadIB"));

        cmd_list.setBufferState(m_quad_vb, GfxResourceStates::CopyDest);
        cmd_list.setBufferState(m_quad_ib, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();
        cmd_list.writeBuffer(m_quad_vb, kQuadVertices, sizeof(kQuadVertices), 0);
        cmd_list.writeBuffer(m_quad_ib, kQuadIndices, sizeof(kQuadIndices), 0);
        cmd_list.setBufferState(m_quad_vb, GfxResourceStates::VertexBuffer);
        cmd_list.setBufferState(m_quad_ib, GfxResourceStates::IndexBuffer);
        cmd_list.commitBarriers();

        m_quad_buffers_ready = true;
    }

} // dodoe
