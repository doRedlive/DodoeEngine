// do@Redlive

#include "material.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/utils/json.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#include <fstream>
#include <sstream>

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Material>> s_material_cache{};

    }

    Bool Material::loadFromJson(const String& absolute_path) {
        std::ifstream file(absolute_path.c_str());
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

        return true;
    }

    Bool Material::saveToJson(const String& absolute_path) const {
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

    Material* Material::Create(const ObjectID& ref, const String& path) {
        if (!ref.isValid() || path.empty()) {
            return nullptr;
        }

        String absolute_path = path;
        if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
            if (FsPath(path.c_str()).is_relative()) {
                absolute_path = String((asset_manager->getAssetDir() / FsPath(path.c_str())).generic_string().c_str());
            }
        }

        auto material = create_scope<Material>(ref);
        Material* raw = material.get();
        raw->setPath(absolute_path);
        raw->loadFromJson(absolute_path);
        const InstanceID instance_id = raw->getInstanceID();
        s_material_cache.emplace(instance_id, std::move(material));
        return raw;
    }

    void Material::Shutdown() {
        s_material_cache.clear();
    }

} // dodoe
