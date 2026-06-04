#include "lua_register_detail.h"

#include "runtime/function/animation/animation.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe::lua_register_detail {

    // Minimal TextureRes for Lua bindings (replaces old resource_type.h)
    struct LuaTextureRes {
        InstanceID id{0};
        std::string path{};
        float ppu{10.0f};
    };

    void register_resource(sol::state& lua, sol::table& dodoe_table) {
        // TODO: Shader class not yet migrated to new asset system
        /*
        dodoe_table.new_usertype<Shader>("Shader",
            "attach", &Shader::attach,
            "detach", &Shader::detach,
            "setBool", &Shader::set_bool,
            "setInt", &Shader::set_int,
            "setFloat", &Shader::set_float,
            "setVec2", &Shader::set_vec2,
            "setVec3", &Shader::set_vec3,
            "setVec4", &Shader::set_vec4,
            "setMat4", &Shader::set_mat4
        );
        */
        dodoe_table.new_usertype<LuaTextureRes>("TextureRes",
            sol::constructors<LuaTextureRes()>(),
            "textureId", &LuaTextureRes::id,
            "path", &LuaTextureRes::path,
            "ppu", &LuaTextureRes::ppu,
            "valid", sol::property([](LuaTextureRes& res) { return res.id != 0; })
        );
        dodoe_table.new_usertype<AnimClip2D>("AnimClip2D",
            sol::constructors<AnimClip2D()>(),
            "loop", &AnimClip2D::loop,
            "frameCount", sol::property([](AnimClip2D& clip) { return clip.frames.size(); }),
            "frameTextureId", [](AnimClip2D& clip, const size_t index) -> InstanceID {
                if (index == 0) {
                    return 0;
                }
                const size_t i = index - 1;
                if (i >= clip.frames.size()) {
                    return 0;
                }
                return clip.frames[i].texture_id;
            },
            "frameDurationMs", [](AnimClip2D& clip, const size_t index) -> float {
                if (index == 0) {
                    return 0.0f;
                }
                const size_t i = index - 1;
                if (i >= clip.frames.size()) {
                    return 0.0f;
                }
                return clip.frames[i].duration;
            }
        );
        dodoe_table.new_usertype<AnimClip2DRes>("AnimClip2DRes",
            sol::constructors<AnimClip2DRes()>(),
            "id", &AnimClip2DRes::id,
            "name", &AnimClip2DRes::name,
            "clip", sol::property([](AnimClip2DRes& res) { return res.clip.get(); })
        );

        sol::table resource_manager_table = lua.create_table();
        dodoe_table["ResourceManager"] = resource_manager_table;
        resource_manager_table.set_function("loadTexture", sol::overload(
            [](const std::string& path) -> LuaTextureRes {
                auto handle = ResourceManager::Self().getTexture(path);
                LuaTextureRes res;
                if (handle.isValid()) {
                    res.id = static_cast<InstanceID>(handle.getFileID().getID());
                    res.path = path;
                }
                return res;
            },
            [](const std::string& id, const std::string& path) -> LuaTextureRes {
                auto handle = ResourceManager::Self().getTexture(path);
                LuaTextureRes res;
                if (handle.isValid()) {
                    res.id = static_cast<InstanceID>(handle.getFileID().getID());
                    res.path = path;
                }
                return res;
            }
        ));
        resource_manager_table.set_function("getTexture", sol::overload(
            [](const std::string& id) -> LuaTextureRes {
                auto handle = ResourceManager::Self().getTexture(id);
                LuaTextureRes res;
                if (handle.isValid()) {
                    res.id = static_cast<InstanceID>(handle.getFileID().getID());
                    res.path = id;
                }
                return res;
            },
            [](const std::string& id, const std::string& path) -> LuaTextureRes {
                auto handle = ResourceManager::Self().getTexture(path);
                LuaTextureRes res;
                if (handle.isValid()) {
                    res.id = static_cast<InstanceID>(handle.getFileID().getID());
                    res.path = path;
                }
                return res;
            }
        ));

        // TODO: Shader and animation methods not yet migrated to new ResourceManager
        /*
        resource_manager_table.set_function("loadShader", [](const std::string& name, const std::string& vert_path, const std::string& frag_path) -> Shader* {
            return ResourceManager::Self().load_shader(name, vert_path, frag_path).get();
        });
        resource_manager_table.set_function("getShader", [](const std::string& name) -> Shader* {
            return ResourceManager::Self().get_shader(name).get();
        });
        */

        /*
        resource_manager_table.set_function("createAnimClip2D", sol::overload(
            [](const std::string& name, const sol::table& texture_ids) -> AnimClip2DRes {
                std::vector<InstanceID> ids;
                for (size_t i = 1;; ++i) {
                    sol::object obj = texture_ids[i];
                    if (!obj.valid()) {
                        break;
                    }
                    if (obj.get_type() == sol::type::number) {
                        ids.push_back(static_cast<InstanceID>(obj.as<uint32_t>()));
                    } else if (obj.get_type() == sol::type::string) {
                        ids.push_back(static_cast<InstanceID>(string2hash(obj.as<std::string>())));
                    }
                }
                return ResourceManager::Self().create_anim_clip2d(name, ids, false, 100.0f);
            },
            [](const std::string& name, const sol::table& texture_ids, const bool loop, const float frame_ms) -> AnimClip2DRes {
                std::vector<InstanceID> ids;
                for (size_t i = 1;; ++i) {
                    sol::object obj = texture_ids[i];
                    if (!obj.valid()) {
                        break;
                    }
                    if (obj.get_type() == sol::type::number) {
                        ids.push_back(static_cast<InstanceID>(obj.as<uint32_t>()));
                    } else if (obj.get_type() == sol::type::string) {
                        ids.push_back(static_cast<InstanceID>(string2hash(obj.as<std::string>())));
                    }
                }
                return ResourceManager::Self().create_anim_clip2d(name, ids, loop, frame_ms);
            }
        ));
        resource_manager_table.set_function("getAnimClip2D", sol::overload(
            [](const std::string& name) -> AnimClip2DRes {
                return ResourceManager::Self().get_anim_clip2d(name);
            },
            [](const uint32_t id) -> AnimClip2DRes {
                return ResourceManager::Self().get_anim_clip2d(static_cast<InstanceID>(id));
            }
        ));
        resource_manager_table.set_function("destroyAnimClip2D", sol::overload(
            [](const uint32_t id) -> bool {
                return ResourceManager::Self().destroy_anim_clip2d(static_cast<InstanceID>(id));
            },
            [](const std::string& name) -> bool {
                return ResourceManager::Self().destroy_anim_clip2d(name);
            }
        ));
        */
    }

} // dodoe::lua_register_detail
