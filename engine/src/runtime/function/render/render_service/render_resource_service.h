// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class GfxContext;

    struct RenderResourceServiceCreateInfo {
        GfxContext* gfx_context{nullptr};
    };

    struct TextureUploadData {
        GfxTextureHandle texture{};
        UInt32 mip_level{0};
        UInt32 array_slice{0};
        const void* data{nullptr};
        Size_t row_pitch{0};
    };

    struct BufferUploadData {
        GfxBufferHandle buffer{};
        const void* data{nullptr};
        Size_t data_size{0};
        UInt64 destination_offset{0};
    };

    // Creates logical resources; physical objects are prepared by SubmissionExecutor.
    // Resource owners retain caching and lifetime policy for their own objects.
    class RenderResourceService : public Managed<RenderResourceService, RenderResourceServiceCreateInfo> {
        friend class Managed<RenderResourceService, RenderResourceServiceCreateInfo>;

        GfxDeviceHandle m_device{};

    public:
        // Returned handles describe resources only. Record their upload/use in a
        // command list to include them in that submission's preparation plan.
        [[nodiscard]] GfxTextureHandle createTexture(const GfxTextureDesc& desc) const;
        [[nodiscard]] GfxBufferHandle createBuffer(const GfxBufferDesc& desc) const;

        Bool enqueueTextureUpload(DrawCommandList& commands, const TextureUploadData& upload) const;
        Bool enqueueBufferUpload(DrawCommandList& commands, const BufferUploadData& upload) const;

        // Records a startup batch through the same resource preparation/replay path.
        // Waits for completion before releasing its locally owned command data.
        Bool submitStartupUploads(
            const DynamicArray<TextureUploadData>& texture_uploads,
            const DynamicArray<BufferUploadData>& buffer_uploads = {}) const;

    private:
        Bool initialize(const RenderResourceServiceCreateInfo& info);
        void shutdown();
    };

} // namespace dodoe
