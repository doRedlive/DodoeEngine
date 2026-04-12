// do->Redlive

#include "render_helper.h"

namespace dodoe {

	rhi::TextureHandle TextureManager::createTexture(const TextureRes& res) {
		if (!device_ || !res.data || !res.data->isValid() || res.data->width <= 0 || res.data->height <= 0) {
			DoError("TextureManager::createTexture: invalid texture resource.");
			return {};
		}

		auto texture_desc = rhi::TextureDesc()
			.setDimension(rhi::TextureDimension::Texture2D)
			.setWidth(res.data->width)
			.setHeight(res.data->height)
			.setFormat(rhi::Format::RGBA8_UNORM)
			.setMipLevels(1)
			.enableAutomaticStateTracking(rhi::ResourceStates::ShaderResource)
			.setDebugName(res.path);
		auto handle = device_->createTexture(texture_desc);
		if (!handle) {
			DoError("TextureManager::createTexture: createTexture failed for {}.", res.path);
			return {};
		}

		auto upload_cmd = device_->createCommandList();
		if (!upload_cmd) {
			DoError("TextureManager::createTexture: createCommandList failed for {}.", res.path);
			return handle;
		}

		const size_t row_pitch = static_cast<size_t>(res.data->width) * 4u;
		upload_cmd->open();
		upload_cmd->writeTexture(handle, 0, 0, res.data->pixels, row_pitch);
		upload_cmd->close();
		device_->executeCommandList(upload_cmd);

		return handle;
	}

	TextureManager& TextureManager::self() {
		static TextureManager instance;
		return instance;
	}

	void TextureManager::initialize(rhi::DeviceHandle device) {
		if (device_ == device && device_) {
			return;
		}
		device_ = device;
		createFallbackTexture();
	}

	void TextureManager::shutdown() {
		texture_umap_.clear();
		fallback_texture_ = nullptr;
		device_ = nullptr;
	}

	rhi::TextureHandle TextureManager::getTexture(const identifier texture_id, const TextureRes& res) {
		if (texture_id == 0) {
			return getFallbackTexture();
		}

		auto cache_it = texture_umap_.find(texture_id);
		if (cache_it != texture_umap_.end()) {
			return cache_it->second;
		}

		if (!res.data || !res.data->isValid()) {
			return getFallbackTexture();
		}

		auto handle = createTexture(res);
		if (!handle) {
			return getFallbackTexture();
		}

		texture_umap_.emplace(texture_id, handle);
		return handle;
	}

	rhi::TextureHandle TextureManager::getTexture(const TextureRes& res) {
		return getTexture(res.id, res);
	}

	rhi::TextureHandle TextureManager::getFallbackTexture() {
		return fallback_texture_;
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
		auto handle = device_->createTexture(texture_desc);

		auto upload_cmd = device_->createCommandList();
		const unsigned char white[4] = {255, 255, 255, 255};
		upload_cmd->open();
		upload_cmd->writeTexture(handle, 0, 0, white, sizeof(white));
		upload_cmd->close();
		device_->executeCommandList(upload_cmd);

		fallback_texture_ = handle;
		DoAssert(fallback_texture_);
	}

} // dodoe
 
