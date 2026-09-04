// do@Redlive

#include "baseline_renderer.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace dodoe {

    Bool BaselineRenderer::initialize(const BaselineRendererCreateInfo& info) {
        if (!info.device) {
            DO_ERROR("BaselineRenderer: device is null");
            return false;
        }
        m_device = info.device;
        m_command_list = m_device->createCommandList();
        if (!m_command_list) {
            DO_ERROR("BaselineRenderer: failed to create raw command list");
            return false;
        }
        m_frame_counter = 0;
        DO_INFO("BaselineRenderer: initialized (raw cutie path, clear-only)");
        return true;
    }

    void BaselineRenderer::shutdown() {
        if (m_device) {
            m_device->waitForIdle();
        }
        m_command_list = nullptr;
        m_device = nullptr;
        m_frame_counter = 0;
    }

    void BaselineRenderer::render(GfxContext& gfx, UInt32 swapchain_image_index) {
        if (!m_device || !m_command_list) {
            return;
        }

        const auto& textures = gfx.getSwapchainTextures();
        if (swapchain_image_index >= textures.size() || !textures[swapchain_image_index]) {
            DO_ERROR("Swapchian Texture Error!");
            return;
        }
        const auto& backbuffer = textures[swapchain_image_index];
        if (!backbuffer->isGpuReady()) {
            DO_ERROR("Backbuffer gpu is not ready!");
            return;
        }
        auto* backbuffer_handle = backbuffer->getRHI();

        m_command_list->open();
        m_command_list->setTextureState(backbuffer_handle, cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        m_command_list->commitBarriers();
        m_command_list->clearTextureFloat(backbuffer_handle, cutie::AllSubresources, cutie::Color(0.18f, 0.35f, 0.60f, 1.0f));
        m_command_list->setTextureState(backbuffer_handle, cutie::AllSubresources, cutie::ResourceStates::Present);
        m_command_list->commitBarriers();
        m_command_list->close();

        m_device->executeCommandList(m_command_list.Get());
        m_device->runGarbageCollection();

        if (((m_frame_counter++) % 120) == 0) {
#if defined(_WIN32)
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
            DO_INFO("BaselineRenderer: frame={} private_commit={:.1f} MB",
                m_frame_counter - 1,
                static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0));
#else
            DO_INFO("BaselineRenderer: frame={}", m_frame_counter - 1);
#endif
        }
    }

} // namespace dodoe
