// do@Redlive

#include "draw_executor.h"
#include "gfx_context.h"
#include "runtime/function/render/render_service/submission_executor.h"

namespace dodoe {

    void DrawExecutor::execute(GfxDeviceHandle device, GfxContext* gfx, FrameContext& frame_ctx) {
        auto gfx_cmd = device->createCommandList();
        if (!gfx_cmd || !frame_ctx.command_list) {
            return;
        }

        gfx_cmd->open();
        SubmissionExecutor::prepareAndReplay(*frame_ctx.command_list, device, *gfx_cmd);
        gfx_cmd->close();

        device->executeCommandList(gfx_cmd);

        device->setEventQuery(frame_ctx.completion_query, GfxCommandQueue::Graphics);

        (void)gfx->presentSwapchainImage(frame_ctx.swapchain_image_index);
        gfx->clearGarbage();
    }

} // namespace dodoe
