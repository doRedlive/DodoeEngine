// do@Redlive

#include "render_resource_service.h"

#include "runtime/function/graphics/gfx_context.h"
#include "submission_executor.h"

namespace dodoe {

    Bool RenderResourceService::initialize(const RenderResourceServiceCreateInfo& info) {
        m_device = info.gfx_context ? info.gfx_context->getDevice() : GfxDeviceHandle{};
        if (!m_device) {
            DO_ERROR("RenderResourceService: graphics device is unavailable");
            return false;
        }
        return true;
    }

    void RenderResourceService::shutdown() {
        m_device = nullptr;
    }

    GfxTextureHandle RenderResourceService::createTexture(const GfxTextureDesc& desc) const {
        if (!m_device) {
            DO_ERROR("RenderResourceService::createTexture: graphics device is unavailable");
            return nullptr;
        }
        return create_ref<GfxTexture>(desc);
    }

    GfxBufferHandle RenderResourceService::createBuffer(const GfxBufferDesc& desc) const {
        if (!m_device) {
            DO_ERROR("RenderResourceService::createBuffer: graphics device is unavailable");
            return nullptr;
        }
        return create_ref<GfxBuffer>(desc);
    }

    Bool RenderResourceService::enqueueTextureUpload(DrawCommandList& commands, const TextureUploadData& upload) const {
        if (!m_device || commands.getDevice() != m_device ||
            !upload.texture || !upload.data || upload.row_pitch == 0) {
            DO_ERROR("RenderResourceService::enqueueTextureUpload: invalid texture upload");
            return false;
        }
        commands.writeTexture(upload.texture, upload.mip_level, upload.array_slice, upload.data, upload.row_pitch);
        return true;
    }

    Bool RenderResourceService::enqueueBufferUpload(DrawCommandList& commands, const BufferUploadData& upload) const {
        if (!m_device || commands.getDevice() != m_device ||
            !upload.buffer || !upload.data || upload.data_size == 0) {
            DO_ERROR("RenderResourceService::enqueueBufferUpload: invalid buffer upload");
            return false;
        }
        commands.writeBuffer(upload.buffer, upload.data, upload.data_size, upload.destination_offset);
        return true;
    }

    Bool RenderResourceService::submitStartupUploads(
        const DynamicArray<TextureUploadData>& texture_uploads,
        const DynamicArray<BufferUploadData>& buffer_uploads) const {
        if (!m_device) {
            DO_ERROR("RenderResourceService::submitStartupUploads: graphics device is unavailable");
            return false;
        }
        if (texture_uploads.empty() && buffer_uploads.empty()) {
            return true;
        }

        DrawCommandList commands;
        commands.setDevice(m_device);
        for (const auto& upload : texture_uploads) {
            if (!enqueueTextureUpload(commands, upload)) return false;
        }
        for (const auto& upload : buffer_uploads) {
            if (!enqueueBufferUpload(commands, upload)) return false;
        }
        if (!SubmissionExecutor::prepareResources(commands, m_device)) return false;

        auto command_list = m_device->createCommandList();
        if (!command_list) {
            DO_ERROR("RenderResourceService::submitStartupUploads: command list creation failed");
            return false;
        }
        command_list->open();
        SubmissionExecutor::replay(commands, *command_list);
        command_list->close();
        m_device->executeCommandList(command_list);

        // Startup batches own their CPU command payload locally. Keep it alive
        // until this submission completes; runtime uploads use frame ownership.
        const auto completion = m_device->createEventQuery();
        if (completion) {
            m_device->setEventQuery(completion, GfxCommandQueue::Graphics);
            m_device->waitEventQuery(completion);
        } else {
            m_device->waitForIdle();
        }
        return true;
    }

} // namespace dodoe
