// do@Redlive

#include "texture_manager.h"

#include "../interface/rhi_context.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/asset/texture_loader.h"

namespace dodoe {

	bool TextureManager::initialize(const TextureManagerCreateInfo& info) {
		rhi_ = info.rhi;
        descriptor_table_ = info.descriptor_table;
		createFallbackTexture();
		return rhi_ && descriptor_table_;
	}

	void TextureManager::shutdown() {
		texture_umap_.clear();
		fallback_texture_ = nullptr;
		descriptor_table_ = nullptr;
		rhi_ = nullptr;
	}

	Ref<Texture> TextureManager::loadTexture(const identifier id, const std::string& path) {
		if (id == 0) {
			return loadFallbackTexture();
		}

        const auto& it = texture_umap_.find(id);
        if (it != texture_umap_.end()) { return it->second; }

		Ref<Texture> texture = createTexture(path);
		if (!texture) {
			return fallback_texture_;
		}
		texture->id = id;
		texture_umap_.emplace(id, texture);
		return texture;
	}

	Ref<Texture> TextureManager::loadTexture(const std::string& path) {
		return loadTexture(string2hash(path), path);
	}

	Ref<Texture> TextureManager::loadTexture(const identifier id) {
        const auto& it = texture_umap_.find(id);
        if (it != texture_umap_.end()) { return it->second; }
		return fallback_texture_;
	}

	Ref<Texture> TextureManager::loadFallbackTexture() {
		return fallback_texture_;
	}

	Ref<Texture> TextureManager::createTexture(const std::string& path) {
		TextureBlob data(path);
		if (!data.isValid()) {
			DO_ERROR("TextureManager: Create texture {} failed!", path);
			return nullptr;
		}

		const auto texture_format = data.is_hdr ? rhi::Format::RGBA32_FLOAT : rhi::Format::RGBA8_UNORM;
		const size_t bytes_per_channel = data.is_hdr ? sizeof(float) : sizeof(uchar);

		auto texture_desc = rhi::TextureDesc()
			.setDimension(rhi::TextureDimension::Texture2D)
			.setWidth(data.width)
			.setHeight(data.height)
			.setFormat(texture_format)
			.setMipLevels(1)
			.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
			.setDebugName(path);
		auto handle = rhi_->getDevice()->createTexture(texture_desc);
		if (!handle) {
			DO_ERROR("TextureManager::createTexture: createTexture failed for {}.", path);
			return nullptr;
		}

		auto cmd = rhi_->getCommandList();
		const size_t row_pitch = static_cast<size_t>(data.width) * 4u * bytes_per_channel;
		cmd->open();
		cmd->writeTexture(handle, 0, 0, data.pixels, row_pitch);
		cmd->close();
		rhi_->getDevice()->executeCommandList(cmd);

		Ref<Texture> texture = create_ref<Texture>();
		texture->id = string2hash(path);
		texture->width = data.width;
		texture->height = data.height;
		texture->path = path;
		texture->handle = handle;
        texture->descriptor_index = descriptor_table_->createDescriptor(rhi::BindingSetItem::Texture_SRV(0, texture->handle));

		return texture;
	}

	void TextureManager::createFallbackTexture() {
		auto texture_desc = rhi::TextureDesc()
			.setDimension(rhi::TextureDimension::Texture2D)
			.setWidth(1)
			.setHeight(1)
			.setFormat(rhi::Format::RGBA8_UNORM)
			.setMipLevels(1)
			.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
			.setDebugName("Render TextureManager Fallback");
		auto handle = rhi_->getDevice()->createTexture(texture_desc);

		auto upload_cmd = rhi_->getCommandList();
		const unsigned char white[4] = {255, 255, 255, 255};
		upload_cmd->open();
		upload_cmd->writeTexture(handle, 0, 0, white, sizeof(white));
		upload_cmd->close();
		rhi_->getDevice()->executeCommandList(upload_cmd);

		fallback_texture_ = create_ref<Texture>();
		fallback_texture_->id = 0;
		fallback_texture_->path = "<fallback>";
		fallback_texture_->width = 1;
		fallback_texture_->height = 1;
		fallback_texture_->handle = handle;
		fallback_texture_->descriptor_index = descriptor_table_->createDescriptor(rhi::BindingSetItem::Texture_SRV(0, fallback_texture_->handle));
		DO_ASSERT(fallback_texture_);
	}

} // dodoe
