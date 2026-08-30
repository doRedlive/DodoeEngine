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
        DO_PROFILE_SCOPE_CATEGORY("Material::loadFromJson", "asset");
        std::ifstream file(absolute_path.c_str());
        if (!file.is_open()) {
            DO_ERROR("Material::loadFromJson: failed to open '{}'", absolute_path);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        Json json;
        try {
            json = Json::parse(buffer.str());
        } catch (const Json::exception& e) {
            DO_ERROR("Material::loadFromJson: JSON parse failed for '{}': {}", absolute_path, e.what());
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

        // DO_DEBUG("Material: loaded '{}'", absolute_path);
        return true;
    }

    Bool Material::saveToJson(const String& absolute_path) const {
        DO_PROFILE_SCOPE_CATEGORY("Material::saveToJson", "asset");
        std::ofstream file(absolute_path.c_str());
        if (!file.is_open()) {
            DO_ERROR("Material::saveToJson: failed to open '{}'", absolute_path);
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
        // DO_DEBUG("Material: saved '{}'", absolute_path);
        return true;
    }

    Material* Material::Create(const ObjectID& ref, const String& path) {
        DO_PROFILE_SCOPE_CATEGORY("Material::Create", "asset");
        if (!ref.isValid() || path.empty()) {
            DO_ERROR("Material::Create: invalid object reference or empty path");
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
        if (!raw->loadFromJson(absolute_path)) {
            DO_WARN("Material::Create: using defaults because '{}' could not be loaded", absolute_path);
        }
        const InstanceID instance_id = raw->getInstanceID();
        s_material_cache.emplace(instance_id, std::move(material));
        DO_INFO("Material: created '{}'", absolute_path);
        return raw;
    }

    void Material::Shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("Material::Shutdown", "shutdown");
        DO_INFO("Material: releasing {} cached material(s)", s_material_cache.size());
        s_material_cache.clear();
    }

} // dodoe
