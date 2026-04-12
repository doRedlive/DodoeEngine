//
// Created by GreenMuffin on 2025/10/28.
//

#include "resource_manager.h"

#include "runtime/core/utils/common.h"

#include "asset/texture_loader.h"
#include "asset/shader_library.h"
#include "asset/animation_library.h"
#include "asset/mesh_loader.h"

namespace dodoe {

    ResourceManager& ResourceManager::self() {
        static ResourceManager instance;
        return instance;
    }

    void ResourceManager::initialize() {        
        shutdown();
        texture_loader_ = TextureLoader::create({});
        shader_library_ = ShaderLibrary::create({});
        animation_library_ = AnimationLibrary::create({});
        mesh_loader_ = MeshLoader::create();
    }

    void ResourceManager::shutdown() {
        TextureLoader::destroy(texture_loader_);
        ShaderLibrary::destroy(shader_library_);
        AnimationLibrary::destroy(animation_library_);
        MeshLoader::destroy(mesh_loader_);
    }

    TextureRes ResourceManager::load_texture(const std::string& name, const std::string& path) {
        if (!texture_loader_) {
            DoError("TextureLoader is not initialized!");
            return {};
        }

        return texture_loader_->loadTexture(name, path);
    }

    Ref<Shader> ResourceManager::load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path) {
        if (!shader_library_) {
            DoError("ShaderLibrary is not initialized!");
            return nullptr;
        }

        return shader_library_->load_shader(name, vert_path, frag_path);
    }

    ModelRes ResourceManager::load_model(const std::string& name, const std::string& path) {
        if (!mesh_loader_) {
            DoError("MeshLoader is not initialized!");
            return {};
        }

        return mesh_loader_->loadModel(name, path);
    }

    TextureRes ResourceManager::getTextureRes(const identifier id) {
        if (!texture_loader_) {
            DoError("TextureLoader is not initialized!");
            return {};
        }
        return texture_loader_->getTexture(id);
    }

    TextureRes ResourceManager::get_texture(const std::string& id) {
        if (!texture_loader_) {
            DoError("TextureLoader is not initialized!");
            return {};
        }
        const identifier texture_id = static_cast<identifier>(string2hash(id));
        return texture_loader_->getTexture(texture_id);
    }

    TextureRes ResourceManager::get_texture(const std::string& id, const std::string& path) {
        if (!texture_loader_) {
            DoError("TextureLoader is not initialized!");
            return {};
        }
        const identifier texture_id = static_cast<identifier>(string2hash(id));
        return texture_loader_->getTexture(texture_id, path);
    }

    Ref<Shader> ResourceManager::get_shader(const std::string& name) {
        return shader_library_->get_shader(name);
    }

    ModelRes ResourceManager::get_model(const identifier id) {
        if (!mesh_loader_) {
            DoError("MeshLoader is not initialized!");
            return {};
        }
        return mesh_loader_->getModel(id);
    }

    ModelRes ResourceManager::get_model(const std::string& id) {
        if (!mesh_loader_) {
            DoError("MeshLoader is not initialized!");
            return {};
        }
        return mesh_loader_->getModel(id);
    }

    ModelRes ResourceManager::get_model(const std::string& id, const std::string& path) {
        if (!mesh_loader_) {
            DoError("MeshLoader is not initialized!");
            return {};
        }
        return mesh_loader_->getModel(id, path);
    }

    MeshRes ResourceManager::get_mesh(const identifier id) {
        if (!mesh_loader_) {
            DoError("MeshLoader is not initialized!");
            return {};
        }
        return mesh_loader_->getMesh(id);
    }

    AnimClip2dRes ResourceManager::get_anim_clip2d(const identifier id) {
        if (!animation_library_) {
            DoError("AnimationLibrary is not initialized!");
            return {};
        }
        return animation_library_->get_clip(id);
    }

    AnimClip2dRes ResourceManager::get_anim_clip2d(const std::string& name) {
        if (!animation_library_) {
            DoError("AnimationLibrary is not initialized!");
            return {};
        }
        return animation_library_->get_clip(name);
    }

    AnimClip2dRes ResourceManager::create_anim_clip2d(const std::string& name, const std::vector<identifier>& texture_ids, const bool loop, const float frame_ms) {
        if (!animation_library_) {
            DoError("AnimationLibrary is not initialized!");
            return {};
        }
        return animation_library_->create_clip(name, texture_ids, loop, frame_ms);
    }

    bool ResourceManager::destroy_anim_clip2d(const identifier id) {
        if (!animation_library_) {
            DoError("AnimationLibrary is not initialized!");
            return false;
        }
        return animation_library_->destroy_clip(id);
    }

    bool ResourceManager::destroy_anim_clip2d(const std::string& name) {
        if (!animation_library_) {
            DoError("AnimationLibrary is not initialized!");
            return false;
        }
        return animation_library_->destroy_clip(name);
    }

} // dodoe 
