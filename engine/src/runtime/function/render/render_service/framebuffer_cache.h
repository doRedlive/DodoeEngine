// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class DrawCommandList;

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

    class FramebufferCache {
    public:
        FramebufferCache() = default;

        void initialize(GfxContext& gfx);

        GfxFramebufferHandle getOrCreate(const FramebufferCacheKey& key,
                                          const GfxFramebufferDesc& desc);
        void invalidateTexture(const void* texture_ptr);
        void endFrame();
        void reset();

    private:
        struct Entry {
            FramebufferCacheKey key;
            GfxFramebufferHandle framebuffer;
        };
        GfxContext* m_gfx_context{nullptr};
        DynamicArray<Entry> m_entries{};
    };

} // namespace dodoe
