// do@Redlive

#include "shader_manifest.h"

#include "runtime/core/utils/json.h"

#include <fstream>

namespace dodoe {

    Bool ShaderManifest::loadFromFile(const String& manifest_path) {
        auto full_path = FileSystem::GetEngineResPath() / manifest_path;
        std::ifstream in(full_path);
        if (!in.is_open()) {
            DO_ERROR("ShaderManifest::loadFromFile failed to open: {}", full_path.string());
            return false;
        }

        Json json;
        try {
            in >> json;
        } catch (const std::exception& e) {
            DO_ERROR("ShaderManifest::loadFromFile JSON parse error in {}: {}", full_path.string(), e.what());
            return false;
        }

        if (!json.contains("shaders") || !json["shaders"].is_array()) {
            DO_ERROR("ShaderManifest::loadFromFile missing 'shaders' array in {}", full_path.string());
            return false;
        }

        m_entries.clear();
        m_name_index.clear();

        for (const auto& entry : json["shaders"]) {
            ShaderManifestEntry e;
            e.name = entry.value("name", "");
            e.source = entry.value("source", "");
            e.entry_point = entry.value("entry_point", "main");
            e.stage = ParseStage(entry.value("stage", ""));

            if (entry.contains("platforms") && entry["platforms"].is_array()) {
                for (const auto& p : entry["platforms"]) {
                    e.platforms.push_back(p.get<String>());
                }
            }

            if (e.name.empty() || e.source.empty()) {
                DO_ERROR("ShaderManifest::loadFromFile entry missing name or source in {}", full_path.string());
                continue;
            }

            m_name_index[e.name] = m_entries.size();
            m_entries.push_back(std::move(e));
        }

        DO_INFO("ShaderManifest::loadFromFile loaded {} shaders from {}", m_entries.size(), full_path.string());
        return true;
    }

    const ShaderManifestEntry* ShaderManifest::find(const String& name) const {
        auto it = m_name_index.find(name);
        if (it != m_name_index.end()) {
            return &m_entries[it->second];
        }
        return nullptr;
    }

    GfxShaderType ShaderManifest::ParseStage(const String& stage_str) {
        if (stage_str == "vertex") {
            return GfxShaderType::Vertex;
        }
        if (stage_str == "pixel") {
            return GfxShaderType::Pixel;
        }
        if (stage_str == "compute") {
            return GfxShaderType::Compute;
        }
        if (stage_str == "geometry") {
            return GfxShaderType::Geometry;
        }
        if (stage_str == "hull") {
            return GfxShaderType::Hull;
        }
        if (stage_str == "domain") {
            return GfxShaderType::Domain;
        }
        DO_ERROR("ShaderManifest::ParseStage unknown stage: {}", stage_str);
        return GfxShaderType::None;
    }

    const char* ShaderManifest::StageToExtension(GfxShaderType stage) {
        switch (stage) {
            case GfxShaderType::Vertex:   return ".vert";
            case GfxShaderType::Pixel:    return ".frag";
            case GfxShaderType::Compute:  return ".comp";
            case GfxShaderType::Geometry: return ".geom";
            case GfxShaderType::Hull:     return ".hull";
            case GfxShaderType::Domain:   return ".domain";
            default:                      return "";
        }
    }

} // namespace dodoe
