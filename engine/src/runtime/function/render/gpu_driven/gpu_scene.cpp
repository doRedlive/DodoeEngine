// do@Redlive

#include "gpu_scene.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_scene/render_scene_delta.h"
#include "runtime/function/render/render_scene/light_scene_info.h"

namespace dodoe {

    Bool GpuScene::initialize(const GpuSceneCreateInfo& info) {
        (void)info;
        return true;
    }

    void GpuScene::shutdown() {
        m_quad_ib = nullptr;
        m_quad_vb = nullptr;
        m_light_instance_buffer = nullptr;
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
        m_light_instance_cpu.clear();
        m_object_capacity = 0;
        m_transform_capacity = 0;
        m_bounds_capacity = 0;
        m_sprite_instance_capacity = 0;
        m_primitive_instance_capacity = 0;
        m_light_instance_capacity = 0;
        m_quad_buffers_ready = false;
        m_last_upload_ranges.clear();
        m_last_stats = GpuSceneStats{};
        m_meta_dirty_range.reset();
        m_meta_dirty_range.clearBits();
        m_transform_dirty_range.reset();
        m_transform_dirty_range.clearBits();
        m_bounds_dirty_range.reset();
        m_bounds_dirty_range.clearBits();
        m_sprite_instance_dirty_range.reset();
        m_sprite_instance_dirty_range.clearBits();
        m_primitive_instance_dirty_range.reset();
        m_primitive_instance_dirty_range.clearBits();
        m_light_instance_dirty_range.reset();
        m_light_instance_dirty_range.clearBits();
    }

    GpuObjectHandle GpuScene::registerObject(const GpuObjectType type, GpuObjectMeta meta) {
        meta.type = static_cast<UInt32>(type);
        const UInt32 idx = m_objects.occupiedCount();
        ensureObjectMetaBuffer(idx + 1);
        ensureTransformsBuffer(idx + 1);
        ensureBoundsBuffer(idx + 1);

        auto handle = m_objects.insert(meta);

        while (m_dirty_flags.size() <= idx) {
            m_dirty_flags.push_back(GpuObjectDirtyFlags::None);
        }
        m_dirty_flags[idx] = GpuObjectDirtyFlags::All;

        m_meta_dirty_range.expand(idx);
        m_meta_dirty_range.markBit(idx);
        m_transform_dirty_range.expand(idx);
        m_transform_dirty_range.markBit(idx);
        m_bounds_dirty_range.expand(idx);
        m_bounds_dirty_range.markBit(idx);

        switch (type) {
        case GpuObjectType::Sprite:
            ensureSpriteInstanceBuffer(idx + 1);
            m_sprite_instance_dirty_range.expand(idx);
            m_sprite_instance_dirty_range.markBit(idx);
            break;
        case GpuObjectType::Primitive:
            ensurePrimitiveInstanceBuffer(idx + 1);
            m_primitive_instance_dirty_range.expand(idx);
            m_primitive_instance_dirty_range.markBit(idx);
            break;
        case GpuObjectType::Light:
            ensureLightInstanceBuffer(idx + 1);
            m_light_instance_dirty_range.expand(idx);
            m_light_instance_dirty_range.markBit(idx);
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
        m_meta_dirty_range.markBit(idx);
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
            m_meta_dirty_range.markBit(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::Transform)) {
            m_transform_dirty_range.expand(idx);
            m_transform_dirty_range.markBit(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::Bounds)) {
            m_bounds_dirty_range.expand(idx);
            m_bounds_dirty_range.markBit(idx);
        }
        if (HasAnyFlags(flags, GpuObjectDirtyFlags::InstanceData)) {
            const auto* meta = m_objects.get(handle);
            if (meta) {
                const auto type = static_cast<GpuObjectType>(meta->type);
                if (type == GpuObjectType::Sprite) {
                    m_sprite_instance_dirty_range.expand(idx);
                    m_sprite_instance_dirty_range.markBit(idx);
                } else if (type == GpuObjectType::Primitive) {
                    m_primitive_instance_dirty_range.expand(idx);
                    m_primitive_instance_dirty_range.markBit(idx);
                } else if (type == GpuObjectType::Light) {
                    m_light_instance_dirty_range.expand(idx);
                    m_light_instance_dirty_range.markBit(idx);
                }
            }
        }
    }

    void GpuScene::updateTransform(const GpuObjectHandle handle, const Matrix4f& local_to_world) {
        const UInt32 idx = handle.index();
        const auto* meta = m_objects.get(handle);
        if (!meta || idx >= m_transform_capacity) return;
        ensureTransformsBuffer(idx + 1);
        m_transforms_cpu[idx].local_to_world = local_to_world;
        m_transform_dirty_range.expand(idx);
        m_transform_dirty_range.markBit(idx);
    }

    void GpuScene::updateBounds(const GpuObjectHandle handle, const Vector3f& center, const Vector3f& extent) {
        const UInt32 idx = handle.index();
        const auto* meta = m_objects.get(handle);
        if (!meta || idx >= m_bounds_capacity) return;
        ensureBoundsBuffer(idx + 1);
        m_bounds_cpu[idx].center = center;
        m_bounds_cpu[idx].extent = extent;
        m_bounds_cpu[idx].sphere_radius = Math::Length(extent);
        m_bounds_dirty_range.expand(idx);
        m_bounds_dirty_range.markBit(idx);
    }

    void GpuScene::updateSpriteInstance(const GpuObjectHandle handle, const SpriteGpuData& data) {
        const UInt32 idx = handle.index();
        auto* meta = m_objects.get(handle);
        if (!meta || idx >= m_sprite_instance_capacity) return;
        ensureSpriteInstanceBuffer(idx + 1);
        m_sprite_instance_cpu[idx] = data;
        m_sprite_instance_dirty_range.expand(idx);
        m_sprite_instance_dirty_range.markBit(idx);
        meta->texture_id = data.atlas_index;
        meta->material_id = data.material_id;
        m_meta_dirty_range.expand(idx);
        m_meta_dirty_range.markBit(idx);
    }

    void GpuScene::updatePrimitiveInstance(const GpuObjectHandle handle, const PrimitiveGpuData& data) {
        const UInt32 idx = handle.index();
        const auto* meta = m_objects.get(handle);
        if (!meta || idx >= m_primitive_instance_capacity) return;
        ensurePrimitiveInstanceBuffer(idx + 1);
        m_primitive_instance_cpu[idx] = data;
        m_primitive_instance_dirty_range.expand(idx);
        m_primitive_instance_dirty_range.markBit(idx);
    }

    void GpuScene::updateLightInstance(const GpuObjectHandle handle, const LightGpuData& data) {
        const UInt32 idx = handle.index();
        const auto* meta = m_objects.get(handle);
        if (!meta || idx >= m_light_instance_capacity) return;
        ensureLightInstanceBuffer(idx + 1);
        m_light_instance_cpu[idx] = data;
        m_light_instance_dirty_range.expand(idx);
        m_light_instance_dirty_range.markBit(idx);
    }

    void GpuScene::applyDelta(const RenderSceneDelta& delta) {
        (void)delta;
    }

    void GpuScene::flushSparseRange(DrawCommandList& cmd_list, DirtyRange& range,
                                     GfxBufferHandle buffer, GfxResourceStates target_state,
                                     const void* cpu_data, UInt32 element_size,
                                     UInt32 max_index) {
        if (!range.hasBits() || !buffer) return;

        const UInt32 num_words = range.dirty_bits.size();
        UInt32 upload_count = 0;

        struct DirtyRegion {
            UInt32 offset_bytes;
            UInt32 size_bytes;
        };
        DynamicArray<DirtyRegion> regions{};

        for (UInt32 word_idx = 0; word_idx < num_words; word_idx++) {
            UInt64 word = range.dirty_bits[word_idx];
            if (word == 0) continue;
            UInt32 base_idx = word_idx << 6;

            while (word != 0) {
                const UInt32 bit = std::countr_zero(word);
                const UInt64 mask = UInt64(1) << bit;
                word &= ~mask;

                const UInt32 idx = base_idx + bit;
                if (idx >= max_index) continue;

                const UInt32 offset_bytes = idx * element_size;

                if (!regions.empty() &&
                    regions.back().offset_bytes + regions.back().size_bytes == offset_bytes) {
                    regions.back().size_bytes += element_size;
                } else {
                    regions.push_back({offset_bytes, element_size});
                }
                upload_count++;
            }
        }

        if (upload_count == 0) return;

        cmd_list.setBufferState(buffer, GfxResourceStates::CopyDest);
        cmd_list.commitBarriers();

        for (const auto& region : regions) {
            cmd_list.writeBuffer(buffer,
                                 static_cast<const UInt8*>(cpu_data) + region.offset_bytes,
                                 region.size_bytes,
                                 region.offset_bytes);
            m_last_upload_ranges.push_back({buffer, region.offset_bytes, region.size_bytes});
        }

        cmd_list.setBufferState(buffer, target_state);
        cmd_list.commitBarriers();

        range.reset();
        range.clearBits();
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
        range.clearBits();
    }

    void GpuScene::flushUpdates(DrawCommandList& cmd_list) {
        m_last_upload_ranges.clear();
        m_last_stats = GpuSceneStats{};

        ensureQuadBuffers();

        if (m_meta_dirty_range.valid() && m_object_meta_buffer) {
            DynamicArray<GpuObjectMeta> metas(m_objects.slotCount());
            m_objects.forEachOccupied([&](GpuObjectHandle h, const GpuObjectMeta& meta) {
                if (h.index() < metas.size()) {
                    metas[h.index()] = meta;
                }
            });
            m_last_stats.meta_dirty_count = m_meta_dirty_range.hasBits() ? m_meta_dirty_range.countBits() : (m_meta_dirty_range.end - m_meta_dirty_range.start);
            if (m_meta_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_meta_dirty_range, m_object_meta_buffer,
                                GfxResourceStates::ShaderResource, metas.data(), sizeof(GpuObjectMeta),
                                static_cast<UInt32>(metas.size()));
            } else {
                flushDirtyRange(cmd_list, m_meta_dirty_range, m_object_meta_buffer,
                               GfxResourceStates::ShaderResource, metas.data(), sizeof(GpuObjectMeta));
            }
        }

        if (m_transform_dirty_range.valid() && m_transforms_buffer) {
            m_last_stats.transform_dirty_count = m_transform_dirty_range.hasBits() ? m_transform_dirty_range.countBits() : (m_transform_dirty_range.end - m_transform_dirty_range.start);
            if (m_transform_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_transform_dirty_range, m_transforms_buffer,
                                GfxResourceStates::ShaderResource,
                                m_transforms_cpu.data(), sizeof(GpuTransform),
                                static_cast<UInt32>(m_transforms_cpu.size()));
            } else {
                flushDirtyRange(cmd_list, m_transform_dirty_range, m_transforms_buffer,
                               GfxResourceStates::ShaderResource,
                               m_transforms_cpu.data(), sizeof(GpuTransform));
            }
        }

        if (m_bounds_dirty_range.valid() && m_bounds_buffer) {
            m_last_stats.bounds_dirty_count = m_bounds_dirty_range.hasBits() ? m_bounds_dirty_range.countBits() : (m_bounds_dirty_range.end - m_bounds_dirty_range.start);
            if (m_bounds_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_bounds_dirty_range, m_bounds_buffer,
                                GfxResourceStates::ShaderResource,
                                m_bounds_cpu.data(), sizeof(GpuBounds),
                                static_cast<UInt32>(m_bounds_cpu.size()));
            } else {
                flushDirtyRange(cmd_list, m_bounds_dirty_range, m_bounds_buffer,
                               GfxResourceStates::ShaderResource,
                               m_bounds_cpu.data(), sizeof(GpuBounds));
            }
        }

        if (m_sprite_instance_dirty_range.valid() && m_sprite_instance_buffer) {
            m_last_stats.sprite_instance_dirty_count = m_sprite_instance_dirty_range.hasBits() ? m_sprite_instance_dirty_range.countBits() : (m_sprite_instance_dirty_range.end - m_sprite_instance_dirty_range.start);
            if (m_sprite_instance_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_sprite_instance_dirty_range, m_sprite_instance_buffer,
                                GfxResourceStates::VertexBuffer,
                                m_sprite_instance_cpu.data(), sizeof(SpriteGpuData),
                                static_cast<UInt32>(m_sprite_instance_cpu.size()));
            } else {
                flushDirtyRange(cmd_list, m_sprite_instance_dirty_range, m_sprite_instance_buffer,
                               GfxResourceStates::VertexBuffer,
                               m_sprite_instance_cpu.data(), sizeof(SpriteGpuData));
            }
        }

        if (m_primitive_instance_dirty_range.valid() && m_primitive_instance_buffer) {
            m_last_stats.primitive_instance_dirty_count = m_primitive_instance_dirty_range.hasBits() ? m_primitive_instance_dirty_range.countBits() : (m_primitive_instance_dirty_range.end - m_primitive_instance_dirty_range.start);
            if (m_primitive_instance_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_primitive_instance_dirty_range, m_primitive_instance_buffer,
                                GfxResourceStates::VertexBuffer,
                                m_primitive_instance_cpu.data(), sizeof(PrimitiveGpuData),
                                static_cast<UInt32>(m_primitive_instance_cpu.size()));
            } else {
                flushDirtyRange(cmd_list, m_primitive_instance_dirty_range, m_primitive_instance_buffer,
                               GfxResourceStates::VertexBuffer,
                               m_primitive_instance_cpu.data(), sizeof(PrimitiveGpuData));
            }
        }

        if (m_light_instance_dirty_range.valid() && m_light_instance_buffer) {
            m_last_stats.light_instance_dirty_count = m_light_instance_dirty_range.hasBits() ? m_light_instance_dirty_range.countBits() : (m_light_instance_dirty_range.end - m_light_instance_dirty_range.start);
            if (m_light_instance_dirty_range.hasBits()) {
                flushSparseRange(cmd_list, m_light_instance_dirty_range, m_light_instance_buffer,
                                GfxResourceStates::ShaderResource,
                                m_light_instance_cpu.data(), sizeof(LightGpuData),
                                static_cast<UInt32>(m_light_instance_cpu.size()));
            } else {
                flushDirtyRange(cmd_list, m_light_instance_dirty_range, m_light_instance_buffer,
                               GfxResourceStates::ShaderResource,
                               m_light_instance_cpu.data(), sizeof(LightGpuData));
            }
        }

        m_last_stats.upload_ranges_count = static_cast<UInt32>(m_last_upload_ranges.size());
        for (const auto& r : m_last_upload_ranges) {
            m_last_stats.total_upload_bytes += r.size_bytes;
        }
        m_last_stats.light_count = static_cast<UInt32>(m_light_instance_cpu.size());
    }

    GpuScenePassResources GpuScene::getPassResources() const {
        GpuScenePassResources res{};
        res.object_meta = m_object_meta_buffer;
        res.transforms = m_transforms_buffer;
        res.bounds = m_bounds_buffer;
        res.sprite_instance = m_sprite_instance_buffer;
        res.primitive_instance = m_primitive_instance_buffer;
        res.light_instance = m_light_instance_buffer;
        res.quad_vb = m_quad_vb;
        res.quad_ib = m_quad_ib;
        return res;
    }

    void GpuScene::ensureObjectMetaBuffer(const UInt32 capacity) {
        if (capacity <= m_object_capacity && m_object_meta_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_object_capacity * 2, 64u));
        m_object_meta_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuObjectMeta)))
                .setStructStride(sizeof(GpuObjectMeta))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene ObjectMeta"));
        m_object_capacity = new_cap;
        m_meta_dirty_range.reset();
        m_meta_dirty_range.clearBits();
        m_meta_dirty_range.expand(0);
        m_meta_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureTransformsBuffer(const UInt32 capacity) {
        if (capacity <= m_transform_capacity && m_transforms_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_transform_capacity * 2, 64u));
        m_transforms_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuTransform)))
                .setStructStride(sizeof(GpuTransform))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene Transforms"));
        m_transforms_cpu.resize(new_cap);
        m_transform_capacity = new_cap;
        m_transform_dirty_range.reset();
        m_transform_dirty_range.clearBits();
        m_transform_dirty_range.expand(0);
        m_transform_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureBoundsBuffer(const UInt32 capacity) {
        if (capacity <= m_bounds_capacity && m_bounds_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_bounds_capacity * 2, 64u));
        m_bounds_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(GpuBounds)))
                .setStructStride(sizeof(GpuBounds))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene Bounds"));
        m_bounds_cpu.resize(new_cap);
        m_bounds_capacity = new_cap;
        m_bounds_dirty_range.reset();
        m_bounds_dirty_range.clearBits();
        m_bounds_dirty_range.expand(0);
        m_bounds_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureSpriteInstanceBuffer(const UInt32 capacity) {
        if (capacity <= m_sprite_instance_capacity && m_sprite_instance_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_sprite_instance_capacity * 2, 64u));
        m_sprite_instance_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(SpriteGpuData)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene SpriteInstance"));
        m_sprite_instance_cpu.resize(new_cap);
        m_sprite_instance_capacity = new_cap;
        m_sprite_instance_dirty_range.reset();
        m_sprite_instance_dirty_range.clearBits();
        m_sprite_instance_dirty_range.expand(0);
        m_sprite_instance_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensurePrimitiveInstanceBuffer(const UInt32 capacity) {
        if (capacity <= m_primitive_instance_capacity && m_primitive_instance_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_primitive_instance_capacity * 2, 64u));
        m_primitive_instance_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(PrimitiveGpuData)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene PrimitiveInstance"));
        m_primitive_instance_cpu.resize(new_cap);
        m_primitive_instance_capacity = new_cap;
        m_primitive_instance_dirty_range.reset();
        m_primitive_instance_dirty_range.clearBits();
        m_primitive_instance_dirty_range.expand(0);
        m_primitive_instance_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureLightInstanceBuffer(const UInt32 capacity) {
        if (capacity <= m_light_instance_capacity && m_light_instance_buffer) return;
        const UInt32 new_cap = std::max(capacity, std::max(m_light_instance_capacity * 2, 64u));
        m_light_instance_buffer = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(new_cap * static_cast<UInt32>(sizeof(LightGpuData)))
                .setStructStride(sizeof(LightGpuData))
                .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                .setDebugName("GpuScene LightInstance"));
        m_light_instance_cpu.resize(new_cap);
        m_light_instance_capacity = new_cap;
        m_light_instance_dirty_range.reset();
        m_light_instance_dirty_range.clearBits();
        m_light_instance_dirty_range.expand(0);
        m_light_instance_dirty_range.expand(new_cap - 1);
    }

    void GpuScene::ensureQuadBuffers() {
        if (m_quad_buffers_ready) return;

        m_quad_vb = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(kQuadVertices)))
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName("GpuScene QuadVB"));

        m_quad_ib = GDrawCommandList.createBuffer(
            GfxBufferDesc()
                .setByteSize(static_cast<UInt32>(sizeof(kQuadIndices)))
                .setIsIndexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
                .setDebugName("GpuScene QuadIB"));

        auto device = GDrawCommandList.getDevice();
        auto cmd = device->createCommandList();
        cmd->open();
        cmd->setBufferState(m_quad_vb->getRHIHandle(), GfxResourceStates::CopyDest);
        cmd->setBufferState(m_quad_ib->getRHIHandle(), GfxResourceStates::CopyDest);
        cmd->commitBarriers();
        cmd->writeBuffer(m_quad_vb->getRHIHandle(), kQuadVertices, sizeof(kQuadVertices), 0);
        cmd->writeBuffer(m_quad_ib->getRHIHandle(), kQuadIndices, sizeof(kQuadIndices), 0);
        cmd->setBufferState(m_quad_vb->getRHIHandle(), GfxResourceStates::VertexBuffer);
        cmd->setBufferState(m_quad_ib->getRHIHandle(), GfxResourceStates::IndexBuffer);
        cmd->commitBarriers();
        cmd->close();
        device->executeCommandList(cmd);

        m_quad_buffers_ready = true;
    }

} // dodoe
