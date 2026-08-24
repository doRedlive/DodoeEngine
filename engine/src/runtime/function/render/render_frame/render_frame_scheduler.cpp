// do@Redlive

#include "render_frame_scheduler.h"

#include <limits>

namespace dodoe {

    Bool RenderFrameScheduler::initialize(const RenderFrameSchedulerCreateInfo& info) {
        m_device = info.device;
        m_current_index = 0;
        m_frame_counter = 0;

        for (Size_t i = 0; i < kMaxFramesInFlight; i++) {
            m_slots[i].completion_query = m_device->createEventQuery();
            m_slots[i].staging = FrameStagingAllocator::Create(FrameStagingAllocatorCreateInfo{m_device});
        }
        return true;
    }

    void RenderFrameScheduler::shutdown() {
        if (m_device) {
            m_device->waitForIdle();
        }
        for (Size_t i = 0; i < kMaxFramesInFlight; i++) {
            FrameStagingAllocator::Destroy(m_slots[i].staging);
            m_slots[i].completion_query = nullptr;
        }
        m_device = nullptr;
    }

    FrameContext RenderFrameScheduler::beginFrame(UInt32 swapchain_image_index) {
        FrameSlot& slot = m_slots[m_current_index];

        if (slot.in_flight && slot.completion_query) {
            m_device->waitEventQuery(slot.completion_query);
            m_device->resetEventQuery(slot.completion_query);
            slot.in_flight = false;
            m_deletion_queue.processCompleted(slot.frame_number);
        }

        slot.command_list.beginFrame();
        slot.transient_resource_pool.releaseAll();
        if (slot.staging) slot.staging->reset();
        slot.frame_number = m_frame_counter++;
        slot.swapchain_image_index = swapchain_image_index;
        slot.in_flight = true;

        m_current_index = (m_current_index + 1) % kMaxFramesInFlight;

        FrameContext ctx;
        ctx.command_list = &slot.command_list;
        ctx.frame_number = slot.frame_number;
        ctx.swapchain_image_index = slot.swapchain_image_index;
        ctx.completion_query = slot.completion_query;
        ctx.staging = slot.staging.get();
        ctx.transient_resource_pool = &slot.transient_resource_pool;
        ctx.valid = true;
        return ctx;
    }

    void RenderFrameScheduler::endFrame(FrameContext& ctx) {
        if (ctx.completion_query) {
            m_device->setEventQuery(ctx.completion_query, GfxCommandQueue::Graphics);
        }
    }

    void RenderFrameScheduler::retireCompletedFrames() {
        for (auto& slot : m_slots) {
            slot.command_list.beginFrame();
            slot.transient_resource_pool.releaseAll();
            if (slot.staging) {
                slot.staging->reset();
            }
            if (slot.completion_query) {
                m_device->resetEventQuery(slot.completion_query);
            }
            slot.in_flight = false;
        }
        m_deletion_queue.processCompleted((std::numeric_limits<UInt64>::max)());
    }

    void RenderFrameScheduler::deferDeleteFunc(std::function<void()> deleter) {
        m_deletion_queue.enqueueFunc(std::move(deleter), m_frame_counter);
    }

    Size_t RenderFrameScheduler::getInFlightCount() const {
        Size_t count = 0;
        for (Size_t i = 0; i < kMaxFramesInFlight; i++) {
            if (m_slots[i].in_flight) {
                count++;
            }
        }
        return count;
    }

} // namespace dodoe
