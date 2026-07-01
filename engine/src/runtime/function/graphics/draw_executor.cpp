#include "draw_executor.h"
#include "gfx_context.h"

namespace dodoe {

    void DrawExecutor::execute(GfxDeviceHandle device, GfxContext* gfx, UInt32 swapchain_image_index) {
        auto gfx_cmd = device->createCommandList();
        if (!gfx_cmd) {
            return;
        }

        gfx_cmd->open();
        GDrawCommandList.execute(gfx_cmd);
        gfx_cmd->close();
        device->executeCommandList(gfx_cmd);

        gfx->presentSwapchainImage(swapchain_image_index);
        gfx->clearGarbage();
        GDrawCommandList.reset();
    }

} // dodoe
