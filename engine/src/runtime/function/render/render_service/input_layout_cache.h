// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include <mutex>

namespace dodoe {

    class GfxContext;
    class DrawCommandList;

    struct InputLayoutCacheCreateInfo {
        GfxContext* gfx_context{nullptr};
    };

    class InputLayoutCache : public Managed<InputLayoutCache, InputLayoutCacheCreateInfo> {
        friend class Managed<InputLayoutCache, InputLayoutCacheCreateInfo>;

    public:
        GfxInputLayoutHandle getOrCreate(const DynamicArray<GfxVertexAttributeDesc>& attributes,
                                          GfxShaderHandle vertex_shader);
        void invalidateForShader(GfxShaderHandle shader);
        void clear();

    private:
        Bool initialize(const InputLayoutCacheCreateInfo& info);
        void shutdown();

        GfxContext* m_gfx_context{nullptr};
        struct Entry {
            Size_t hash{0};
            GfxShaderHandle shader{};
            DynamicArray<GfxVertexAttributeDesc> attributes{};
            GfxInputLayoutHandle layout{};
        };
        mutable std::mutex m_mutex{};
        DynamicArray<Entry> m_cache{};
    };

} // namespace dodoe
