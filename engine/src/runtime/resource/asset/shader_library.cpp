//
// Created by Redlive on 2026/3/19.
//

#include "shader_library.h"

#include "runtime/core/utils/common.h"

namespace {
    std::optional<std::string> read_file(const std::string& path) {
        auto file_path = std::filesystem::path(path);
        std::ifstream ifs(file_path, std::ios::in | std::ios::binary);
        if (!ifs) {
            DO_ERROR("Can't open the file: {}", path);
            return std::nullopt;
        }

        std::string content;
        ifs.seekg(0, std::ios::end);
        const auto size = ifs.tellg();
        if (size < 0) {
           DO_ERROR("Can't get the file size!");
            return std::nullopt;
        }

        content.reserve(static_cast<size_t>(size));
        ifs.seekg(0, std::ios::beg);
        content.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        return content;
    }
}

namespace dodoe {

    bool ShaderLibrary::initialize(const ShaderLibraryCreateInfo& init_info) {
        (void)init_info;
        return true;
    }

    void ShaderLibrary::shutdown() {
        shader_umap_.clear();
    }

    Ref<Shader> ShaderLibrary::load_shader(const std::string& name, const std::string& vert_path, const std::string& frag_path) {
        auto id = string2hash(name);
        if (auto it = shader_umap_.find(id); it != shader_umap_.end()) {
            return it->second.shader;
        }

        auto vert_source = read_file(vert_path);
        auto frag_source = read_file(frag_path);
        if (vert_source.has_value() && frag_source.has_value()) {
            auto shader = Shader::create({vert_source.value(), frag_source.value()});

            ShaderRes res {shader, name, vert_path, frag_path};
            auto [inserted_it, _] = shader_umap_.emplace(id, std::move(res));
            return inserted_it->second.shader;
        }
        DO_ERROR("Can't load the shader! name={}, vert={}, frag={}", name, vert_path, frag_path);
        return nullptr;
    }

    Ref<Shader> ShaderLibrary::get_shader(const std::string& name) { 
        auto id = string2hash(name);
        if (auto it = shader_umap_.find(id); it != shader_umap_.end()) {
            return it->second.shader;
        }

        DO_ERROR("Can't find the shader {}!", name);
        return nullptr;
    }

} // dodoe
