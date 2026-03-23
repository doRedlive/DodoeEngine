//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_TEXTURE_MANAGER_H
#define DODOE_TEXTURE_MANAGER_H

#include "dopch.h"

#include "runtime/function/render/backend/texture.h"

namespace dodoe {

    struct TextureRes {
        Ref<Texture> texture;
        std::string path;
        float ppu{10.0f};
    };

    struct TextureManagerInitInfo {
        
    };

    class TextureManager {
    public:
        static Scope<TextureManager> create(TextureManagerInitInfo create_info);
        static void destroy(Scope<TextureManager>& texture_manager);

        void initialize(TextureManagerInitInfo init_info);
        void shutdown();

        TextureRes load_texture(const std::string& path);
        [[nodiscard]] TextureRes get_texture(const std::string& path);

    private:
        std::unordered_map<identifier, TextureRes> texture_umap_;
    };

} // dodoe

#endif//DODOE_TEXTURE_MANAGER_H
