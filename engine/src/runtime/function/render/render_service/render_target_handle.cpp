// do@Redlive

#include "render_target_handle.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    static constexpr UInt64 kDeferredFrameDelay = 3;

    void RenderTargetHandle::initialize(const RenderTargetDesc& desc, GfxContext& gfx,
                                         DeferredDeletionQueue* deletion_queue) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::initialize", "startup");
        m_desc = desc;
        m_deletion_queue = deletion_queue;

        UInt32 width = desc.fixed_width;
        UInt32 height = desc.fixed_height;
        if (desc.scale_policy == RenderTargetScalePolicy::Relative) {
            const auto extent = gfx.getSwapchainExtent2D();
            width  = static_cast<UInt32>(static_cast<Float>(extent.x) * desc.scale_x);
            height = static_cast<UInt32>(static_cast<Float>(extent.y) * desc.scale_y);
        }
        width  = std::max(width, 1u);
        height = std::max(height, 1u);
        m_current_width  = width;
        m_current_height = height;

        createAllTextures(gfx);
        DO_INFO("RenderTargetHandle: initialized ({}x{}, color attachments={}, depth={}, samples={})",
            m_current_width, m_current_height, m_desc.color_attachments.size(), m_desc.has_depth, m_desc.sample_count);
    }

    Bool RenderTargetHandle::resolve(const UInt32 reference_width, const UInt32 reference_height,
        GfxContext& gfx, const UInt64 current_frame) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::resolve", "swapchain");
        UInt32 target_width  = reference_width;
        UInt32 target_height = reference_height;

        if (m_desc.scale_policy == RenderTargetScalePolicy::Relative) {
            target_width  = static_cast<UInt32>(static_cast<Float>(reference_width)  * m_desc.scale_x);
            target_height = static_cast<UInt32>(static_cast<Float>(reference_height) * m_desc.scale_y);
        }

        target_width  = std::max(target_width, 1u);
        target_height = std::max(target_height, 1u);

        if (target_width == m_current_width && target_height == m_current_height) {
            return false;
        }

        if (m_deletion_queue) {
            auto old_color_textures = std::move(m_color_textures);
            auto old_depth_texture  = m_depth_texture;
            auto old_framebuffer    = m_framebuffer;
            m_deletion_queue->enqueueFunc(
                [old_color_textures, old_depth_texture, old_framebuffer]() mutable {
                    old_framebuffer.reset();
                    old_depth_texture.reset();
                    old_color_textures.clear();
                }, current_frame + kDeferredFrameDelay);
        } else {
            destroyAllTextures();
        }

        m_color_textures.clear();
        m_depth_texture.reset();
        m_framebuffer.reset();

        m_current_width  = target_width;
        m_current_height = target_height;
        ++m_revision;

        createAllTextures(gfx);
        // DO_DEBUG("RenderTargetHandle: resized to {}x{} (revision={})", m_current_width, m_current_height, m_revision);
        return true;
    }

    void RenderTargetHandle::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::shutdown", "shutdown");
        destroyAllTextures();
        m_framebuffer.reset();
        m_revision = 0;
        m_deletion_queue = nullptr;
    }

    GfxTextureHandle RenderTargetHandle::getColorTexture(const UInt32 index) const {
        if (index < m_color_textures.size()) {
            return m_color_textures[index];
        }
        return nullptr;
    }

    GfxTextureHandle RenderTargetHandle::getDepthTexture() const {
        return m_depth_texture;
    }

    GfxFramebufferHandle RenderTargetHandle::getFramebuffer(DrawCommandList& cmd) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::getFramebuffer", "resource");
        if (m_framebuffer) {
            return m_framebuffer;
        }

        GfxFramebufferDesc fb_desc{};
        for (const auto& tex : m_color_textures) {
            if (tex) {
                fb_desc.addColorAttachment(tex);
            }
        }
        if (m_depth_texture) {
            fb_desc.setDepthAttachment(m_depth_texture);
        }

        m_framebuffer = cmd.createFramebuffer(fb_desc);
        if (!m_framebuffer) {
            DO_ERROR("RenderTargetHandle::getFramebuffer: failed to create framebuffer");
        }
        return m_framebuffer;
    }

    void RenderTargetHandle::createAllTextures(GfxContext& gfx) {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::createAllTextures", "resource");
        const auto device = gfx.getDevice();
        if (!device) {
            DO_ERROR("RenderTargetHandle::createAllTextures: graphics device is unavailable");
            return;
        }

        for (const auto& ca : m_desc.color_attachments) {
            GfxTextureDesc tex_desc{};
            tex_desc.width  = m_current_width;
            tex_desc.height = m_current_height;
            tex_desc.format = ca.format;
            tex_desc.dimension = GfxTextureDimension::Texture2D;
            tex_desc.mipLevels = 1;
            tex_desc.arraySize = 1;
            tex_desc.sampleCount = m_desc.sample_count;
            tex_desc.enableAutomaticStateTracking(GfxResourceStates::ShaderResource);
            tex_desc.setIsRenderTarget(true);
            tex_desc.setClearValue(ca.clear_color);

            auto tex = create_ref<GfxTexture>(tex_desc, ca.debug_name);
            tex->initializeGpu(device);
            m_color_textures.push_back(tex);
        }

        if (m_desc.has_depth) {
            GfxTextureDesc depth_desc{};
            depth_desc.width  = m_current_width;
            depth_desc.height = m_current_height;
            depth_desc.format = m_desc.depth_format;
            depth_desc.dimension = GfxTextureDimension::Texture2D;
            depth_desc.mipLevels = 1;
            depth_desc.arraySize = 1;
            depth_desc.sampleCount = m_desc.sample_count;
            depth_desc.enableAutomaticStateTracking(GfxResourceStates::DepthWrite);
            depth_desc.setIsRenderTarget(true);
            depth_desc.setClearValue(GfxColor(m_desc.clear_depth, 0.0f, 0.0f, 0.0f));

            m_depth_texture = create_ref<GfxTexture>(depth_desc, m_desc.depth_debug_name);
            m_depth_texture->initializeGpu(device);
        }
        // DO_DEBUG("RenderTargetHandle: created {} color texture(s){}",
        //     m_color_textures.size(), m_depth_texture ? " and depth texture" : "");
    }

    void RenderTargetHandle::destroyAllTextures() {
        DO_PROFILE_SCOPE_CATEGORY("RenderTargetHandle::destroyAllTextures", "shutdown");
        m_color_textures.clear();
        m_depth_texture.reset();
        m_framebuffer.reset();
    }

} // namespace dodoe
