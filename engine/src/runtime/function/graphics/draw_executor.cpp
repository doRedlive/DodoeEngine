// do@Redlive

#include "draw_executor.h"
#include "gfx_context.h"

namespace dodoe {

    void DrawExecutor::execute(GfxDeviceHandle device, GfxContext* gfx, FrameContext& frame_ctx) {
        auto gfx_cmd = device->createCommandList();
        if (!gfx_cmd) {
            return;
        }

        gfx_cmd->open();

        frame_ctx.command_list.execute(gfx_cmd);
        gfx_cmd->close();
        device->executeCommandList(gfx_cmd);

        device->setEventQuery(frame_ctx.completion_query, GfxCommandQueue::Graphics);

        gfx->presentSwapchainImage(frame_ctx.swapchain_image_index);
        gfx->clearGarbage();
        frame_ctx.command_list.reset();
    }

} // namespace dodoe
