// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/container/deferred_deletion.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GfxContext;
    class DrawCommandList;

    enum class RenderTargetScalePolicy : UInt8 {
        Fixed,
        Relative,
    };

    struct RenderTargetDesc {
        String name{};

        RenderTargetScalePolicy scale_policy{RenderTargetScalePolicy::Relative};
        Float scale_x{1.0f};
        Float scale_y{1.0f};
        UInt32 fixed_width{0};
        UInt32 fixed_height{0};

        struct ColorAttachment {
            GfxFormat format{GfxFormat::RGBA8_UNORM};
            String debug_name{};
            GfxColor clear_color{0.0f, 0.0f, 0.0f, 1.0f};
        };
        DynamicArray<ColorAttachment> color_attachments{};

        Bool has_depth{false};
        GfxFormat depth_format{GfxFormat::D32};
        String depth_debug_name{};
        Float clear_depth{1.0f};
        UInt8 clear_stencil{0};

        UInt32 sample_count{1};
    };

    class RenderTargetHandle {
    public:
        RenderTargetHandle() = default;

        void initialize(const RenderTargetDesc& desc, GfxContext& gfx,
                        DeferredDeletionQueue* deletion_queue);
        Bool resolve(UInt32 reference_width, UInt32 reference_height,
                     GfxContext& gfx, UInt64 current_frame);
        void shutdown();

        [[nodiscard]] GfxTextureHandle getColorTexture(UInt32 index = 0) const;
        [[nodiscard]] GfxTextureHandle getDepthTexture() const;
        [[nodiscard]] GfxFramebufferHandle getFramebuffer(DrawCommandList& cmd);
        [[nodiscard]] UInt64 getRevision() const { return m_revision; }
        [[nodiscard]] UInt32 getWidth() const { return m_current_width; }
        [[nodiscard]] UInt32 getHeight() const { return m_current_height; }
        [[nodiscard]] const RenderTargetDesc& getDesc() const { return m_desc; }

    private:
        void createAllTextures(GfxContext& gfx);
        void destroyAllTextures();

        RenderTargetDesc m_desc{};
        DynamicArray<GfxTextureHandle> m_color_textures{};
        GfxTextureHandle m_depth_texture{};
        GfxFramebufferHandle m_framebuffer{};
        UInt32 m_current_width{0};
        UInt32 m_current_height{0};
        UInt64 m_revision{0};
        DeferredDeletionQueue* m_deletion_queue{nullptr};
    };

} // namespace dodoe
