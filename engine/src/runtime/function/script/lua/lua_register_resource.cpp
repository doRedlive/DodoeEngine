#include "lua_register_detail.h"

#include "runtime/function/render/backend/shader.h"
#include "runtime/function/render/backend/texture.h"
#include "runtime/resource/resource_manager.h"

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
    }

} // dodoe::lua_register_detail
