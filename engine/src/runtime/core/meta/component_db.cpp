// do@Redlive

#include "component_db.h"

#include "_generated/serializer/all_serializer.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/function/world/components.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <functional>
#include <type_traits>
#include <utility>

namespace dodoe {

    namespace {
        template <typename T>
        concept HasDirtyFlag = requires(T component) {
            component.dirty = true;
        };
    }

    bool ComponentDB::Entry::contains(Entity& entity) const {
        return has && has(entity);
    }

    void* ComponentDB::Entry::get(Entity& entity) const {
        return getPtr ? getPtr(entity) : nullptr;
    }

    ComponentDB& ComponentDB::self() {
        static ComponentDB db;
        return db;
    }

    ComponentDB::ComponentDB() {
        m_entries.reserve(16);
        registerBuiltinComponents();
    }

    const ComponentDB::Entry* ComponentDB::find(const String& name) const {
        const auto it = m_name2index.find(name);
        if (it == m_name2index.end()) {
            return nullptr;
        }
        return &m_entries[it->second];
    }

    const ComponentDB::Entry* ComponentDB::find(entt::id_type type) const {
        const auto it = m_typ2index.find(type);
        if (it == m_typ2index.end()) {
            return nullptr;
        }
        return &m_entries[it->second];
    }

    const std::vector<ComponentDB::Entry>& ComponentDB::entries() const {
        return m_entries;
    }

    const std::vector<std::pair<entt::id_type, String>>& ComponentDB::allComponents() const {
        return m_addable_components;
    }

    bool ComponentDB::hasComponent(Entity& entity, const String& name) const {
        const Entry* entry = find(name);
        return entry && entry->contains(entity);
    }

    void* ComponentDB::getComponentPtr(Entity& entity, const String& name) const {
        const Entry* entry = find(name);
        return entry ? entry->get(entity) : nullptr;
    }

    bool ComponentDB::addComponent(Entity& entity, const String& name) const {
        const Entry* entry = find(name);
        if (!entry || !entry->canAdd()) {
            return false;
        }
        entry->add(entity);
        return true;
    }

    bool ComponentDB::removeComponent(Entity& entity, const String& name) const {
        const Entry* entry = find(name);
        if (!entry || entry->remove == nullptr) {
            return false;
        }
        entry->remove(entity);
        return true;
    }

    bool ComponentDB::markComponentDirty(Entity& entity, const String& name) const {
        const Entry* entry = find(name);
        if (!entry || entry->markDirty == nullptr) {
            return false;
        }
        entry->markDirty(entity);
        return true;
    }

    template <typename T>
    void ComponentDB::registerComponent(const String& name, bool addable) {
        Entry entry{};
        entry.type = entt::type_hash<T>::value();
        entry.name = name;
        entry.addable = addable;

        entry.has = +[](Entity& entity) -> bool {
            return entity.hasComponent<T>();
        };

        if constexpr (!std::is_empty_v<T>) {
            entry.getPtr = +[](Entity& entity) -> void* {
                if (!entity.hasComponent<T>()) {
                    return nullptr;
                }
                return &entity.getComponent<T>();
            };

            entry.add = +[](Entity& entity) -> void {
                if (!entity.hasComponent<T>()) {
                    entity.addComponent<T>();
                }
            };
        } else {
            entry.getPtr = +[](Entity&) -> void* {
                return nullptr;
            };
            entry.add = nullptr;
        }

        entry.remove = +[](Entity& entity) -> void {
            if (entity.hasComponent<T>()) {
                entity.removeComponent<T>();
            }
        };

        if constexpr (HasDirtyFlag<T>) {
            entry.markDirty = +[](Entity& entity) -> void {
                if (entity.hasComponent<T>()) {
                    entity.getComponent<T>().dirty = true;
                }
            };
        } else {
            entry.markDirty = nullptr;
        }

        if constexpr (!std::is_empty_v<T>) {
            entry.writeJson = +[](void* component) -> Json {
                return Serializer::write(*static_cast<T*>(component));
            };
            entry.readJson = +[](void* component, const Json& json) -> bool {
                Serializer::read(json, *static_cast<T*>(component));
                return true;
            };
        } else {
            entry.writeJson = nullptr;
            entry.readJson = nullptr;
        }

        const std::size_t index = m_entries.size();
        m_entries.push_back(std::move(entry));
        m_name2index.emplace(m_entries[index].name, index);
        m_typ2index.emplace(m_entries[index].type, index);

        if (m_entries[index].canAdd()) {
            m_addable_components.emplace_back(m_entries[index].type, m_entries[index].name);
        }
    }

    void ComponentDB::registerBuiltinComponents() {
        registerComponent<IDComponent>("IDComponent", false);
        registerComponent<TagComponent>("TagComponent", false);
        registerComponent<TransformComponent>("TransformComponent", false);
        registerComponent<HierarchyComponent>("HierarchyComponent", false);
        registerComponent<PrefabInstanceComponent>("PrefabInstanceComponent");

        registerComponent<AnimatorComponent>("AnimatorComponent");
        registerComponent<AnimationDriveModeComponent>("AnimationDriveModeComponent");
        registerComponent<AudioSourceComponent>("AudioSourceComponent");
        registerComponent<AudioListenerComponent>("AudioListenerComponent");
        registerComponent<BoneAttachmentComponent>("BoneAttachmentComponent");
        registerComponent<CameraComponent>("CameraComponent");
        registerComponent<BoxColliderComponent>("BoxColliderComponent");
        registerComponent<BoxCollider2dComponent>("BoxCollider2dComponent");
        registerComponent<CapsuleColliderComponent>("CapsuleColliderComponent");
        registerComponent<CircleCollider2dComponent>("CircleCollider2dComponent");
        registerComponent<FoliageRendererComponent>("FoliageRendererComponent");
        registerComponent<PointLightComponent>("PointLightComponent");
        registerComponent<SpotLightComponent>("SpotLightComponent");
        registerComponent<SkyLightComponent>("SkyLightComponent");
        registerComponent<CircleRendererComponent>("CircleRendererComponent");
        registerComponent<LineRendererComponent>("LineRendererComponent");
        registerComponent<MeshRendererComponent>("MeshRendererComponent");
        registerComponent<RectRendererComponent>("RectRendererComponent");
        registerComponent<Rigidbody2dComponent>("Rigidbody2dComponent");
        registerComponent<RigidbodyComponent>("RigidbodyComponent");
        registerComponent<SphereColliderComponent>("SphereColliderComponent");
        registerComponent<SpriteRendererComponent>("SpriteRendererComponent");
        registerComponent<TilemapComponent>("TilemapComponent", false);
        registerComponent<TileLayerComponent>("TileLayerComponent");
    }

} // dodoe
