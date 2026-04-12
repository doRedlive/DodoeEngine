//
// Created by GreenMuffin on 2025/10/28.
//

#ifndef DODOE_RESOURCE_MANAGER_H
#define DODOE_RESOURCE_MANAGER_H
#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "asset/shader_library.h"
#include "asset/texture_loader.h"
#include "asset/animation_library.h"
#include "asset/mesh_loader.h"

namespace dodoe {

    class ResourceManager {
    public:
        static ResourceManager& self();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        void initialize();
        void shutdown();

        TextureRes load_texture(const std::string& name, const std::string& path);
        Ref<Shader> load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path);
        ModelRes load_model(const std::string& name, const std::string& path);

        [[nodiscard]] TextureRes getTextureRes(identifier id);
        [[nodiscard]] TextureRes get_texture(const std::string& id);
        [[nodiscard]] TextureRes get_texture(const std::string& id, const std::string& path);
        [[nodiscard]] Ref<Shader> get_shader(const std::string& name);
        [[nodiscard]] ModelRes get_model(identifier id);
        [[nodiscard]] ModelRes get_model(const std::string& id);
        [[nodiscard]] ModelRes get_model(const std::string& id, const std::string& path);
        [[nodiscard]] MeshRes get_mesh(identifier id);
        [[nodiscard]] AnimClip2dRes get_anim_clip2d(identifier id);
        [[nodiscard]] AnimClip2dRes get_anim_clip2d(const std::string& name);

        AnimClip2dRes create_anim_clip2d(const std::string& name, const std::vector<identifier>& texture_ids, bool loop = false, float frame_ms = 100.0f);
        bool destroy_anim_clip2d(identifier id);
        bool destroy_anim_clip2d(const std::string& name);

    private:
        ResourceManager() = default;
        Scope<TextureLoader> texture_loader_{nullptr};
        Scope<ShaderLibrary> shader_library_{nullptr};
        Scope<AnimationLibrary> animation_library_{nullptr};
        Scope<MeshLoader> mesh_loader_{nullptr};
    };
} // dodoe



#endif //DODOE_RESOURCE_MANAGER_H
