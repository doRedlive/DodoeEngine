//
// Created by Redlive on 2026/3/19.
//

#include "texture_manager.h"

#include "runtime/core/utils/common.h"

#include "stb_image.h"

namespace dodoe {

    Scope<TextureManager> TextureManager::create(TextureManagerInitInfo create_info) {
        auto context = create_scope<TextureManager>();
        context->initialize(create_info);
        return context;
    }

    void TextureManager::destroy(Scope<TextureManager>& texture_manager) {
        if (!texture_manager) {
            return;
        }

        texture_manager->shutdown();
        texture_manager.reset();
    }

    void TextureManager::initialize(TextureManagerInitInfo init_info) {

    }

    void TextureManager::shutdown() {
        for (auto& [id, res] : texture_umap_) {
            res.texture.reset();
        }
        texture_umap_.clear();
    }

    Ref<Texture> TextureManager::load_texture(const std::string& path) {
        auto id = string2hash(path);
        if (auto it = texture_umap_.find(id); it != texture_umap_.end()) {
            return it->second.texture;
        } 

        stbi_set_flip_vertically_on_load(true);

        int width = 0;
        int height = 0;
        int channels = 0;

        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            DoError("Load texture from {} failed!", path);
            return nullptr;
        }

        auto texture = Texture::create({width, height, data});

        TextureRes res {texture, path};
        auto [inserted_it, _] = texture_umap_.emplace(id, std::move(res));

        stbi_image_free(data);

        return inserted_it->second.texture;
    }

    Ref<Texture> TextureManager::get_texture(const std::string& path) {
        auto id = string2hash(path);
        if (auto it = texture_umap_.find(id); it != texture_umap_.end()) {
            return it->second.texture;
        }

        DoError("Can't find the texture {}!", path);
        return nullptr;
    }

} // dodoe
