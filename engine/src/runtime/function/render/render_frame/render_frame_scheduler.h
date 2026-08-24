// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/container/deferred_deletion.h"
#include "frame_context.h"
#include "frame_telemetry.h"
#include "frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_transient_pool.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct RenderFrameSchedulerCreateInfo {
        GfxDeviceHandle device{};
    };

    class RenderFrameScheduler : public Managed<RenderFrameScheduler, RenderFrameSchedulerCreateInfo> {
        friend class Managed<RenderFrameScheduler, RenderFrameSchedulerCreateInfo>;

        static constexpr Size_t kMaxFramesInFlight = 3;

        struct FrameSlot {
            UInt64 frame_number{0};
            Scope<FrameStagingAllocator> staging{nullptr};
            RenderGraphTransientPool transient_resource_pool{};
            UInt32 swapchain_image_index{0};
            Bool in_flight{false};
            GfxEventQueryHandle completion_query{};
            DrawCommandList command_list{};
        };

        GfxDeviceHandle m_device{};
        StaticArray<FrameSlot, kMaxFramesInFlight> m_slots{};
        Size_t m_current_index{0};
        UInt64 m_frame_counter{0};

        DeferredDeletionQueue m_deletion_queue{};
        FrameTelemetryCollector m_telemetry{};

    public:
        FrameContext beginFrame(UInt32 swapchain_image_index);
        void endFrame(FrameContext& ctx);
        void retireCompletedFrames();

        void deferDeleteFunc(std::function<void()> deleter);

        [[nodiscard]] DeferredDeletionQueue* getDeletionQueue() { return &m_deletion_queue; }

        [[nodiscard]] Size_t getInFlightCount() const;

    private:
        Bool initialize(const RenderFrameSchedulerCreateInfo& info);
        void shutdown();
    };

} // namespace dodoe
