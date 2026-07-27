// do@Redlive

#include "material_asset.h"

#include "runtime/core/meta/serializer/serializer.h"

namespace dodoe {

    Bool MaterialAsset::loadFromSource(const String& absolute_source_path) {
        std::ifstream file(absolute_source_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        Json json;
        try {
            json = Json::parse(buffer.str());
        } catch (const Json::exception&) {
            return false;
        }

        if (json.contains("color")) {
            Serializer::read(json["color"], m_color);
        }
        if (json.contains("emissive")) {
            Serializer::read(json["emissive"], m_emissive);
        }
        if (json.contains("metallic")) {
            Serializer::read(json["metallic"], m_metallic);
        }
        if (json.contains("roughness")) {
            Serializer::read(json["roughness"], m_roughness);
        }
        if (json.contains("base_color_texture")) {
            Serializer::read(json["base_color_texture"], m_base_color_texture);
        }
        if (json.contains("normal_texture")) {
            Serializer::read(json["normal_texture"], m_normal_texture);
        }
        if (json.contains("metallic_roughness_texture")) {
            Serializer::read(json["metallic_roughness_texture"], m_metallic_roughness_texture);
        }
        if (json.contains("emissive_texture")) {
            Serializer::read(json["emissive_texture"], m_emissive_texture);
        }

        m_meta.source_path = absolute_source_path;
        return true;
    }

    void MaterialAsset::unloadRuntime() {
    }

    Bool MaterialAsset::saveToSource(const String& absolute_path) const {
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            return false;
        }

        Json json;
        json["color"] = Serializer::write(m_color);
        json["emissive"] = Serializer::write(m_emissive);
        json["metallic"] = Serializer::write(m_metallic);
        json["roughness"] = Serializer::write(m_roughness);
        json["base_color_texture"] = Serializer::write(m_base_color_texture);
        json["normal_texture"] = Serializer::write(m_normal_texture);
        json["metallic_roughness_texture"] = Serializer::write(m_metallic_roughness_texture);
        json["emissive_texture"] = Serializer::write(m_emissive_texture);

        file << json.dump(4);
        file.flush();
        return true;
    }

} // dodoe
