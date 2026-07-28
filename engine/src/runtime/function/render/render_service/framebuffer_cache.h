// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class GfxContext;
    class DrawCommandList;

    struct FramebufferCacheCreateInfo {
        GfxContext* gfx_context{nullptr};
    };

    struct FramebufferCacheKey {
        struct AttachmentRef {
            const void* texture_ptr{nullptr};
            UInt64 revision{0};
            UInt32 mip{0};
            UInt32 layer{0};

            [[nodiscard]] Bool operator==(const AttachmentRef&) const = default;
        };

        DynamicArray<AttachmentRef> color_refs{};
        AttachmentRef depth_ref{};
        UInt32 sample_count{1};

        [[nodiscard]] Size_t computeHash() const;
        [[nodiscard]] Bool operator==(const FramebufferCacheKey&) const = default;
    };

    class FramebufferCache : public Managed<FramebufferCache, FramebufferCacheCreateInfo> {
        friend class Managed<FramebufferCache, FramebufferCacheCreateInfo>;
    public:
        GfxFramebufferHandle getOrCreate(const FramebufferCacheKey& key,
                                          const GfxFramebufferDesc& desc);
        void invalidateTexture(const void* texture_ptr);
        void endFrame();
        void reset();

    private:
        Bool initialize(const FramebufferCacheCreateInfo& info);
        void shutdown();

        struct Entry {
            FramebufferCacheKey key;
            GfxFramebufferHandle framebuffer;
        };
        GfxContext* m_gfx_context{nullptr};
        DynamicArray<Entry> m_entries{};
    };

} // namespace dodoe
