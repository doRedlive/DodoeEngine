//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_TEXTURE_LOADER_H
#define DODOE_TEXTURE_LOADER_H

#include "dopch.h"

#include "../resource_type.h"

namespace dodoe {

    struct TextureLoaderCreateInfo {

    };

    class TextureLoader {
    public:
        static Scope<TextureLoader> create(const TextureLoaderCreateInfo& create_info);
        static void destroy(Scope<TextureLoader>& texture_loader);

        void initialize(const TextureLoaderCreateInfo& info);
        void shutdown();

        TextureRes loadTexture(identifier id, const std::string& path);
        TextureRes loadTexture(const std::string& id, const std::string& path);
        [[nodiscard]] TextureRes getTexture(identifier id);
        [[nodiscard]] TextureRes getTexture(identifier id, const std::string& path);
        [[nodiscard]] TextureRes getTexture(const std::string& id);

    private:
        std::unordered_map<identifier, TextureRes> texture_umap_;
    };

} // dodoe

#endif //DODOE_TEXTURE_LOADER_H
