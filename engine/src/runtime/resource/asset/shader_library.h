
//
// Created by Redlive on 2026/3/19.
//

#ifndef DODOE_SHADER_LIBRARY_H
#define DODOE_SHADER_LIBRARY_H

#include "dopch.h"

#include "runtime/function/render/backend/shader.h"


namespace dodoe {

    struct ShaderRes{
        Ref<Shader> shader;
        std::string name;
        std::string vert_path;
        std::string frag_path;
    };

    struct ShaderLibraryCreateInfo {

    };

    class ShaderLibrary {
    public:
        static Scope<ShaderLibrary> create(ShaderLibraryCreateInfo create_info);
        static void destroy(Scope<ShaderLibrary>& shader_library);
        
        Ref<Shader> load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path);
        
        [[nodiscard]] Ref<Shader> get_shader(const std::string& name);
        
    private:
        std::unordered_map<identifier, ShaderRes> shader_umap_;

        void shutdown();
        void initialize(ShaderLibraryCreateInfo init_info);
    };

}

#endif//DODOE_SHADER_LIBRARY_H