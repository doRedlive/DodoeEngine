// do@Redlive

#include "input_layout_cache.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    Bool InputLayoutCache::initialize(const InputLayoutCacheCreateInfo& info) {
        m_gfx_context = info.gfx_context;
        DO_ASSERT(m_gfx_context != nullptr, "InputLayoutCache requires gfx_context");
        return m_gfx_context != nullptr;
    }

    void InputLayoutCache::shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
        m_gfx_context = nullptr;
    }

    GfxInputLayoutHandle InputLayoutCache::getOrCreate(
        const DynamicArray<GfxVertexAttributeDesc>& attributes,
        GfxShaderHandle vertex_shader)
    {
        Size_t h = reinterpret_cast<Size_t>(vertex_shader.Get());
        hash_combine(h, attributes.size());
        for (const auto& attr : attributes) {
            hash_combine(h, std::hash<std::string>{}(attr.name));
            hash_combine(h, static_cast<Size_t>(attr.format));
            hash_combine(h, static_cast<Size_t>(attr.arraySize));
            hash_combine(h, static_cast<Size_t>(attr.bufferIndex));
            hash_combine(h, static_cast<Size_t>(attr.offset));
            hash_combine(h, static_cast<Size_t>(attr.elementStride));
            hash_combine(h, static_cast<Size_t>(attr.isInstanced));
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_cache) {
            if (entry.hash != h || entry.shader != vertex_shader || entry.attributes.size() != attributes.size()) continue;
            Bool equal = true;
            for (Size_t i = 0; i < attributes.size(); ++i) {
                const auto& a = attributes[i];
                const auto& b = entry.attributes[i];
                if (a.name != b.name || a.format != b.format || a.arraySize != b.arraySize ||
                    a.bufferIndex != b.bufferIndex || a.offset != b.offset ||
                    a.elementStride != b.elementStride || a.isInstanced != b.isInstanced) { equal = false; break; }
            }
            if (equal) return entry.layout;
        }

        DynamicArray<GfxVertexAttributeDesc> remapped = attributes;
        for (UInt32 index = 0; index < remapped.size(); ++index) {
            remapped[index].setName("TEXCOORD" + std::to_string(index));
        }

        auto input_layout = m_gfx_context->getDevice()->createInputLayout(
            remapped.data(),
            static_cast<UInt32>(remapped.size()),
            vertex_shader);
        if (input_layout) m_cache.push_back({h, vertex_shader, attributes, input_layout});
        return input_layout;
    }

    void InputLayoutCache::invalidateForShader(GfxShaderHandle shader) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.erase(std::remove_if(m_cache.begin(), m_cache.end(),
            [&](const Entry& entry) { return entry.shader == shader; }), m_cache.end());
    }

    void InputLayoutCache::clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
    }

} // namespace dodoe
