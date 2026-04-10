//
// Created by Redlive on 2026/3/19.
//

#include "texture_loader.h"

#include "runtime/core/utils/common.h"

#include "stb_image.h"

namespace dodoe {

    Scope<TextureLoader> TextureLoader::create(const TextureLoaderCreateInfo& create_info) {
        auto context = create_scope<TextureLoader>();
        context->initialize(create_info);
        return context;
    }

    void TextureLoader::destroy(Scope<TextureLoader>& texture_loader) {
        if (!texture_loader) {
            return;
        }

        texture_loader->shutdown();
        texture_loader.reset();
    }

    void TextureLoader::initialize(const TextureLoaderCreateInfo& info) {

    }

    void TextureLoader::shutdown() {
        for (auto& [id, res] : texture_umap_) {
            res.data.reset();
        }
        texture_umap_.clear();
    }

    TextureRes TextureLoader::loadTexture(const std::string& id, const std::string& path) {
        return loadTexture(static_cast<identifier>(String2Hash(id)), path);
    }

    TextureRes TextureLoader::loadTexture(identifier id, const std::string& path) {
        if (auto it = texture_umap_.find(id); it != texture_umap_.end()) {
            return it->second;
        }

        stbi_set_flip_vertically_on_load(true);

        int width = 0;
        int height = 0;
        int channels = 0;

        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            DoError("Load texture from {} failed!", path);
            return {};
        }

        auto texture_data = create_ref<TextureData>(width, height, data);

        TextureRes res{id, texture_data, path, 10.0f};
        auto [inserted_it, _] = texture_umap_.emplace(id, std::move(res));

        stbi_image_free(data);

        return inserted_it->second;
    }

    TextureRes TextureLoader::getTexture(identifier id) {
        if (auto it = texture_umap_.find(id); it != texture_umap_.end()) {
            return it->second;
        }

        DoError("TextureLoader::getTexture: texture not found.");
        return {};
    }

    TextureRes TextureLoader::getTexture(identifier id, const std::string& path) {
        if (auto it = texture_umap_.find(id); it != texture_umap_.end()) {
            return it->second;
        }
        return loadTexture(id, path);
    }

    TextureRes TextureLoader::getTexture(const std::string& id) {
        return getTexture(static_cast<identifier>(String2Hash(id)));
    }

} // dodoe
