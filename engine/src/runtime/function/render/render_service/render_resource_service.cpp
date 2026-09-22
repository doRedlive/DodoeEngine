// do@Redlive

#include "render_resource_service.h"

#include "runtime/function/graphics/gfx_context.h"

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
        auto texture = create_ref<GfxTexture>(desc);
        texture->initializeGpu(m_device);
        return texture;
    }

    GfxBufferHandle RenderResourceService::createBuffer(const GfxBufferDesc& desc) const {
        if (!m_device) {
            DO_ERROR("RenderResourceService::createBuffer: graphics device is unavailable");
            return nullptr;
        }
        auto buffer = create_ref<GfxBuffer>(desc);
        buffer->initializeGpu(m_device);
        return buffer;
    }

    Bool RenderResourceService::enqueueTextureUpload(DrawCommandList& commands, const TextureUploadData& upload) const {
        if (!upload.texture || !upload.texture->isGpuReady() || !upload.data || upload.row_pitch == 0) {
            DO_ERROR("RenderResourceService::enqueueTextureUpload: invalid texture upload");
            return false;
        }
        commands.writeTexture(upload.texture, upload.mip_level, upload.array_slice, upload.data, upload.row_pitch);
        return true;
    }

    Bool RenderResourceService::enqueueBufferUpload(DrawCommandList& commands, const BufferUploadData& upload) const {
        if (!upload.buffer || !upload.buffer->isGpuReady() || !upload.data || upload.data_size == 0) {
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

        auto command_list = m_device->createCommandList();
        if (!command_list) {
            DO_ERROR("RenderResourceService::submitStartupUploads: command list creation failed");
            return false;
        }

        command_list->open();
        for (const auto& upload : texture_uploads) {
            if (!upload.texture || !upload.texture->isGpuReady() || !upload.data || upload.row_pitch == 0) {
                DO_ERROR("RenderResourceService::submitStartupUploads: invalid texture upload");
                command_list->close();
                return false;
            }
            command_list->writeTexture(
                upload.texture->getRHIHandle(), upload.array_slice, upload.mip_level, upload.data, upload.row_pitch);
        }
        for (const auto& upload : buffer_uploads) {
            if (!upload.buffer || !upload.buffer->isGpuReady() || !upload.data || upload.data_size == 0) {
                DO_ERROR("RenderResourceService::submitStartupUploads: invalid buffer upload");
                command_list->close();
                return false;
            }
            command_list->writeBuffer(
                upload.buffer->getRHIHandle(), upload.data, upload.data_size, upload.destination_offset);
        }
        command_list->close();
        m_device->executeCommandList(command_list);
        return true;
    }

} // namespace dodoe
