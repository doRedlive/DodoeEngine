#include "scene_serializer.h"

#include "components.h"
#include "entity.h"
#include "scene.h"

#include "runtime/core/meta/json.h"
#include "_generated/serializer/all_serializer.ipp"

namespace dodoe {
    namespace {
        template<typename T>
        void WriteComponent(Json& json, Entity entity, const char* name) {
            if (entity.hasComponent<T>()) {
                json[name] = Serializer::write(entity.getComponent<T>());
            }
        }

        template<typename T>
        void ReadComponent(const Json& json, Entity entity, const char* name) {
            if (!json.contains(name)) {
                return;
            }

            T& component = entity.addOrReplaceComponent<T>();
            Serializer::read(json.at(name), component);
        }

#define DODOE_SCENE_COMPONENTS(X)                    \
        X(IDComponent, "IDComponent")               \
        X(TagComponent, "TagComponent")             \
        X(TransformComponent, "TransformComponent") \
        X(Camera2dComponent, "Camera2dComponent")   \
        X(SpriteRendererComponent, "SpriteRendererComponent") \
        X(Rigidbody2dComponent, "Rigidbody2dComponent")       \
        X(BoxCollider2dComponent, "BoxCollider2dComponent")   \
        X(ModelRendererComponent, "ModelRendererComponent")   \
        X(MeshRendererComponent, "MeshRendererComponent")     \
        X(MeshComponent, "MeshComponent")           \
        X(Animation2dComponent, "Animation2dComponent")
    } // namespace

    SceneSerializer::SceneSerializer(Scene* scene) : scene_(scene) { }

    bool SceneSerializer::serialize(const std::filesystem::path& file_path) const {
        if (!scene_) {
            DO_ERROR("SceneSerializer::serialize failed because scene is null.");
            return false;
        }

        Json root = Json::object();
        root["Scene"] = scene_->getName();
        root["Entities"] = Json::array();

        for (Entity entity : scene_->getEntities()) {
            Json entity_json = Json::object();

#define DODOE_WRITE_COMPONENT(Type, Name) WriteComponent<Type>(entity_json, entity, Name);
            DODOE_SCENE_COMPONENTS(DODOE_WRITE_COMPONENT)
#undef DODOE_WRITE_COMPONENT

            root["Entities"].push_back(std::move(entity_json));
        }

        std::ofstream fout(file_path);
        if (!fout.is_open()) {
            DO_ERROR("Failed to open scene file for writing: {}", file_path.string());
            return false;
        }

        fout << root.dump(4);
        return true;
    }

    bool SceneSerializer::deserialize(const std::filesystem::path& file_path) {
        if (!scene_) {
            DO_ERROR("SceneSerializer::deserialize failed because scene is null.");
            return false;
        }

        Json root;
        try {
            std::ifstream fin(file_path);
            if (!fin.is_open()) {
                DO_ERROR("Failed to open scene file for reading: {}", file_path.string());
                return false;
            }
            fin >> root;
        }
        catch (const Json::exception& e) {
            DO_ERROR("Failed to parse scene file {}: {}", file_path.string(), e.what());
            return false;
        }

        if (root.contains("Scene") && root.at("Scene").is_string()) {
            scene_->setName(root.at("Scene").get<std::string>());
        }

        const std::vector<Entity> entities = scene_->getEntities();
        for (Entity entity : entities) {
            scene_->destroyEntity(entity);
        }

        if (!root.contains("Entities") || !root.at("Entities").is_array()) {
            return true;
        }

        for (const Json& entity_json : root.at("Entities")) {
            if (!entity_json.contains("IDComponent")) {
                continue;
            }

            IDComponent id_component{};
            Serializer::read(entity_json.at("IDComponent"), id_component);
            Entity entity = scene_->createEntity(id_component.id, id_component.name);

#define DODOE_READ_COMPONENT(Type, Name) ReadComponent<Type>(entity_json, entity, Name);
            DODOE_SCENE_COMPONENTS(DODOE_READ_COMPONENT)
#undef DODOE_READ_COMPONENT
        }

        return true;
    }
} // dodoe
