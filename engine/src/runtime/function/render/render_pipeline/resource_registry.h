// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/render_service/render_target_handle.h"

namespace dodoe {

	class ResourceRegistry {
	public:
	    void registerTexture(const String& name, GfxTextureHandle texture, GfxFormat format,
	                         RenderTargetScalePolicy scale_policy) {
	        m_textures[name] = { texture, format, scale_policy };
	    }

	    void registerBuffer(const String& name, GfxBufferHandle buffer) {
	        m_buffers[name] = buffer;
	    }

	    GfxTextureHandle findTexture(const String& name) const {
	        auto it = m_textures.find(name);
	        return it != m_textures.end() ? it->second.texture : GfxTextureHandle{};
	    }

	    GfxBufferHandle findBuffer(const String& name) const {
	        auto it = m_buffers.find(name);
	        return it != m_buffers.end() ? it->second : GfxBufferHandle{};
	    }

	    Bool hasTexture(const String& name) const {
	        return m_textures.find(name) != m_textures.end();
	    }

	    Bool hasBuffer(const String& name) const {
	        return m_buffers.find(name) != m_buffers.end();
	    }

	    void registerRenderTarget(const String& name, RenderTargetHandle* handle) {
	        m_render_targets[name] = handle;
	    }

	    RenderTargetHandle* findRenderTarget(const String& name) const {
	        auto it = m_render_targets.find(name);
	        return it != m_render_targets.end() ? it->second : nullptr;
	    }

	    void clear() {
	        m_textures.clear();
	        m_buffers.clear();
	        m_render_targets.clear();
	    }

	private:
	    struct TextureEntry {
	        GfxTextureHandle texture{};
	        GfxFormat format{GfxFormat::Unknown};
	        RenderTargetScalePolicy scale_policy{RenderTargetScalePolicy::Relative};
	    };

	    UnorderedMap<String, TextureEntry> m_textures{};
	    UnorderedMap<String, GfxBufferHandle> m_buffers{};
	    UnorderedMap<String, RenderTargetHandle*> m_render_targets{};
	};

} // namespace dodoe
