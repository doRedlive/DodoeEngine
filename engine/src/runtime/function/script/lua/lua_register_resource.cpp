#include "lua_register_detail.h"

#include "runtime/function/render/backend/shader.h"
#include "runtime/function/render/backend/texture.h"
#include "runtime/function/animation/animation.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/common.h"

namespace dodoe::lua_register_detail {

    void register_resource(sol::state& lua, sol::table& dodoe_table) {
        dodoe_table.new_usertype<Texture>("Texture",
            "id", &Texture::id,
            "width", &Texture::width,
            "height", &Texture::height,
            "attach", &Texture::attach
        );
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
        dodoe_table.new_usertype<TextureRes>("TextureRes",
            sol::constructors<TextureRes()>(),
            "textureId", &TextureRes::texture_id,
            "path", &TextureRes::path,
            "ppu", &TextureRes::ppu,
            "texture", sol::property([](TextureRes& res) { return res.texture.get(); })
        );
        dodoe_table.new_usertype<AnimClip2d>("AnimClip2d",
            sol::constructors<AnimClip2d()>(),
            "loop", &AnimClip2d::loop,
            "frameCount", sol::property([](AnimClip2d& clip) { return clip.frames.size(); }),
            "frameTextureId", [](AnimClip2d& clip, const size_t index) -> identifier {
                if (index == 0) {
                    return 0;
                }
                const size_t i = index - 1;
                if (i >= clip.frames.size()) {
                    return 0;
                }
                return clip.frames[i].texture_id;
            },
            "frameDurationMs", [](AnimClip2d& clip, const size_t index) -> float {
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
        dodoe_table.new_usertype<AnimClip2dRes>("AnimClip2dRes",
            sol::constructors<AnimClip2dRes()>(),
            "id", &AnimClip2dRes::id,
            "name", &AnimClip2dRes::name,
            "clip", sol::property([](AnimClip2dRes& res) { return res.clip.get(); })
        );

        sol::table resource_manager_table = lua.create_table();
        dodoe_table["ResourceManager"] = resource_manager_table;
        resource_manager_table.set_function("loadTexture", sol::overload(
            [](const std::string& path) -> TextureRes {
                return ResourceManager::self().get_texture(path, path);
            },
            [](const std::string& id, const std::string& path) -> TextureRes {
                return ResourceManager::self().get_texture(id, path);
            }
        ));
        resource_manager_table.set_function("getTexture", sol::overload(
            [](const std::string& id) -> TextureRes {
                return ResourceManager::self().get_texture(id);
            },
            [](const std::string& id, const std::string& path) -> TextureRes {
                return ResourceManager::self().get_texture(id, path);
            }
        ));
        resource_manager_table.set_function("loadShader", [](const std::string& name, const std::string& vert_path, const std::string& frag_path) -> Shader* {
            return ResourceManager::self().load_shader(name, vert_path, frag_path).get();
        });
        resource_manager_table.set_function("getShader", [](const std::string& name) -> Shader* {
            return ResourceManager::self().get_shader(name).get();
        });

        resource_manager_table.set_function("createAnimClip2d", sol::overload(
            [](const std::string& name, const sol::table& texture_ids) -> AnimClip2dRes {
                std::vector<identifier> ids;
                for (size_t i = 1;; ++i) {
                    sol::object obj = texture_ids[i];
                    if (!obj.valid()) {
                        break;
                    }
                    if (obj.get_type() == sol::type::number) {
                        ids.push_back(static_cast<identifier>(obj.as<uint32_t>()));
                    } else if (obj.get_type() == sol::type::string) {
                        ids.push_back(static_cast<identifier>(string2hash(obj.as<std::string>())));
                    }
                }
                return ResourceManager::self().create_anim_clip2d(name, ids, false, 100.0f);
            },
            [](const std::string& name, const sol::table& texture_ids, const bool loop, const float frame_ms) -> AnimClip2dRes {
                std::vector<identifier> ids;
                for (size_t i = 1;; ++i) {
                    sol::object obj = texture_ids[i];
                    if (!obj.valid()) {
                        break;
                    }
                    if (obj.get_type() == sol::type::number) {
                        ids.push_back(static_cast<identifier>(obj.as<uint32_t>()));
                    } else if (obj.get_type() == sol::type::string) {
                        ids.push_back(static_cast<identifier>(string2hash(obj.as<std::string>())));
                    }
                }
                return ResourceManager::self().create_anim_clip2d(name, ids, loop, frame_ms);
            }
        ));
        resource_manager_table.set_function("getAnimClip2d", sol::overload(
            [](const std::string& name) -> AnimClip2dRes {
                return ResourceManager::self().get_anim_clip2d(name);
            },
            [](const uint32_t id) -> AnimClip2dRes {
                return ResourceManager::self().get_anim_clip2d(static_cast<identifier>(id));
            }
        ));
        resource_manager_table.set_function("destroyAnimClip2d", sol::overload(
            [](const uint32_t id) -> bool {
                return ResourceManager::self().destroy_anim_clip2d(static_cast<identifier>(id));
            },
            [](const std::string& name) -> bool {
                return ResourceManager::self().destroy_anim_clip2d(name);
            }
        ));
    }

} // dodoe::lua_register_detail
