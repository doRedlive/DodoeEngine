// Created by GreenMuffin on 2025/10/28.
#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "asset/shader_library.h"
#include "asset/texture_loader.h"
#include "asset/animation_library.h"
#include "asset/mesh_loader.h"
#include "asset/asset_manager.h"

namespace dodoe {

    class ResourceManager {
    public:
        static ResourceManager& Self();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        void initialize();
        void shutdown();

        Ref<Shader> load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path);
        ModelRes load_model(const std::string& name, const std::string& path);

        [[nodiscard]] Ref<Shader> get_shader(const std::string& name);
        [[nodiscard]] ModelRes get_model(identifier id);
        [[nodiscard]] ModelRes get_model(const std::string& id);
        [[nodiscard]] ModelRes get_model(const std::string& id, const std::string& path);
        [[nodiscard]] TextureRes get_texture(const std::string& id, const std::string& path);
        [[nodiscard]] TextureRes get_texture(const std::string& id);
        [[nodiscard]] MeshRes get_mesh(identifier id);
        [[nodiscard]] AnimClip2dRes get_anim_clip2d(identifier id);
        [[nodiscard]] AnimClip2dRes get_anim_clip2d(const std::string& name);
        [[nodiscard]] AssetManager* getAssetManager() const { return asset_manager_.get(); }

        AnimClip2dRes create_anim_clip2d(const std::string& name, const std::vector<identifier>& texture_ids, bool loop = false, float frame_ms = 100.0f);
        bool destroy_anim_clip2d(identifier id);
        bool destroy_anim_clip2d(const std::string& name);

    private:
        ResourceManager() = default;
        Scope<ShaderLibrary> shader_library_{nullptr};
        Scope<AnimationLibrary> animation_library_{nullptr};
        Scope<MeshLoader> mesh_loader_{nullptr};
        Scope<AssetManager> asset_manager_{nullptr};
    };
} // dodoe
