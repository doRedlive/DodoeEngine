// do@Redlive

#include "input_layout_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    Bool InputLayoutCache::initialize(const InputLayoutCacheCreateInfo& info) {
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "InputLayoutCache requires gfx_context");
        return true;
    }

    void InputLayoutCache::shutdown() {
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxInputLayoutHandle InputLayoutCache::getOrCreate(
        const DynamicArray<GfxVertexAttributeDesc>& attributes,
        GfxShaderHandle vertex_shader,
        DrawCommandList& cmd)
    {
        Size_t h = 0;
        for (const auto& attr : attributes) {
            hash_combine(h, static_cast<Size_t>(attr.location));
            hash_combine(h, static_cast<Size_t>(attr.binding));
            hash_combine(h, static_cast<Size_t>(attr.format));
            hash_combine(h, static_cast<Size_t>(attr.offset));
        }

        auto it = m_cache.find(h);
        if (it != m_cache.end()) {
            return it->second;
        }

        auto input_layout = cmd.createInputLayout(
            attributes.data(),
            static_cast<UInt32>(attributes.size()),
            vertex_shader);
        m_cache[h] = input_layout;
        return input_layout;
    }

    void InputLayoutCache::invalidateForShader(GfxShaderHandle shader) {
        m_cache.clear();
    }

    void InputLayoutCache::clear() {
        m_cache.clear();
    }

} // namespace dodoe
