// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

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
                                          GfxShaderHandle vertex_shader,
                                          DrawCommandList& cmd);
        void invalidateForShader(GfxShaderHandle shader);
        void clear();

    private:
        Bool initialize(const InputLayoutCacheCreateInfo& info);
        void shutdown();

        GfxContext* m_gfx_context{nullptr};
        UnorderedMap<Size_t, GfxInputLayoutHandle> m_cache{};
    };

} // namespace dodoe
