
//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_SHADER_LIBRARY_H
#define DODOE_SHADER_LIBRARY_H

#include "dopch.h"


namespace dodoe {

    struct ShaderCreateInfo {
        std::string vert_source{};
        std::string frag_source{};
    };

    class Shader {
    public:
        static Ref<Shader> create(const ShaderCreateInfo& info) {
            (void)info;
            return create_ref<Shader>();
        }

        void attach() {}
        void detach() {}

        void set_bool(const std::string& name, bool value) {
            (void)name;
            (void)value;
        }

        void set_int(const std::string& name, int value) {
            (void)name;
            (void)value;
        }

        void set_float(const std::string& name, float value) {
            (void)name;
            (void)value;
        }

        void set_vec2(const std::string& name, const Vector2f& value) {
            (void)name;
            (void)value;
        }

        void set_vec3(const std::string& name, const Vector3f& value) {
            (void)name;
            (void)value;
        }

        void set_vec4(const std::string& name, const Vector4f& value) {
            (void)name;
            (void)value;
        }

        void set_mat4(const std::string& name, const Matrix4f& value) {
            (void)name;
            (void)value;
        }
    };

    struct ShaderRes{
        Ref<Shader> shader;
        std::string name;
        std::string vert_path;
        std::string frag_path;
    };

    struct ShaderLibraryCreateInfo {

    };

    class ShaderLibrary : public Managed<ShaderLibrary, ShaderLibraryCreateInfo> {
        friend class Managed<ShaderLibrary, ShaderLibraryCreateInfo>;
    public:

        Ref<Shader> load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path);
        
        [[nodiscard]] Ref<Shader> get_shader(const std::string& name);
        
    private:
        std::unordered_map<identifier, ShaderRes> shader_umap_;

        void shutdown();
        bool initialize(const ShaderLibraryCreateInfo& init_info);
    };

}

#endif//DODOE_SHADER_LIBRARY_H
