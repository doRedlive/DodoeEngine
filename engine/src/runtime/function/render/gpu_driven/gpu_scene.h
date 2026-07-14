// do@Redlive

#pragma once

#include "dopch.h"

#include "gpu_object_id.h"
#include "gpu_scene_buffers.h"
#include "gpu_dirty_flags.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/core/container/slot_map.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GpuSceneCreateInfo {};

    struct UploadRange {
        GfxBufferHandle buffer;
        UInt32 offset_bytes{0};
        UInt32 size_bytes{0};
    };

    struct GpuScenePassResources {
        GfxBufferHandle object_meta;
        GfxBufferHandle transforms;
        GfxBufferHandle bounds;
        GfxBufferHandle sprite_instance;
        GfxBufferHandle primitive_instance;
        GfxBufferHandle quad_vb;
        GfxBufferHandle quad_ib;
    };

    class GpuScene : public Managed<GpuScene, GpuSceneCreateInfo> {
        friend class Managed<GpuScene, GpuSceneCreateInfo>;

        struct DirtyRange {
            UInt32 start{~0u};
            UInt32 end{0};

            void expand(UInt32 idx) {
                if (start == ~0u) start = idx;
                if (end <= idx) end = idx + 1;
            }
            Bool valid() const { return end > start; }
            void reset() { start = ~0u; end = 0; }
        };

        SlotMap<GpuObjectMeta> m_objects{};

        DynamicArray<GpuObjectDirtyFlags> m_dirty_flags{};

        DirtyRange m_meta_dirty_range{};
        DirtyRange m_transform_dirty_range{};
        DirtyRange m_bounds_dirty_range{};
        DirtyRange m_sprite_instance_dirty_range{};
        DirtyRange m_primitive_instance_dirty_range{};

        GfxBufferHandle m_object_meta_buffer{};
        GfxBufferHandle m_transforms_buffer{};
        GfxBufferHandle m_bounds_buffer{};
        GfxBufferHandle m_sprite_instance_buffer{};
        GfxBufferHandle m_primitive_instance_buffer{};
        GfxBufferHandle m_quad_vb{};
        GfxBufferHandle m_quad_ib{};

        DynamicArray<GpuTransform> m_transforms_cpu{};
        DynamicArray<GpuBounds> m_bounds_cpu{};
        DynamicArray<SpriteGpuData> m_sprite_instance_cpu{};
        DynamicArray<PrimitiveGpuData> m_primitive_instance_cpu{};

        UInt32 m_object_capacity{0};
        UInt32 m_transform_capacity{0};
        UInt32 m_bounds_capacity{0};
        UInt32 m_sprite_instance_capacity{0};
        UInt32 m_primitive_instance_capacity{0};
        Bool m_quad_buffers_ready{false};

        DynamicArray<UploadRange> m_last_upload_ranges{};

    public:
        GpuObjectHandle registerObject(GpuObjectType type, GpuObjectMeta meta);
        void unregisterObject(GpuObjectHandle handle);

        void markDirty(GpuObjectHandle handle, GpuObjectDirtyFlags flags);

        void updateTransform(GpuObjectHandle handle, const Matrix4f& local_to_world);
        void updateBounds(GpuObjectHandle handle, const Vector3f& center, const Vector3f& extent);
        void updateSpriteInstance(GpuObjectHandle handle, const SpriteGpuData& data);
        void updatePrimitiveInstance(GpuObjectHandle handle, const PrimitiveGpuData& data);

        void flushUpdates(DrawCommandList& cmd_list);

        GpuScenePassResources getPassResources() const;
        const DynamicArray<UploadRange>& getLastUploadRanges() const { return m_last_upload_ranges; }

    private:
        Bool initialize(const GpuSceneCreateInfo& info);
        void shutdown();

        void ensureObjectMetaBuffer(UInt32 capacity, DrawCommandList& cmd_list);
        void ensureTransformsBuffer(UInt32 capacity, DrawCommandList& cmd_list);
        void ensureBoundsBuffer(UInt32 capacity, DrawCommandList& cmd_list);
        void ensureSpriteInstanceBuffer(UInt32 capacity, DrawCommandList& cmd_list);
        void ensurePrimitiveInstanceBuffer(UInt32 capacity, DrawCommandList& cmd_list);
        void ensureQuadBuffers(DrawCommandList& cmd_list);

        void flushDirtyRange(DrawCommandList& cmd_list, DirtyRange& range,
                             GfxBufferHandle buffer, GfxResourceStates target_state,
                             const void* cpu_data, UInt32 element_size);
    };

} // dodoe
