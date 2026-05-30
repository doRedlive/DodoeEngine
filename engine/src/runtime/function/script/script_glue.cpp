// do@GreenMuffin

#include "script_glue.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/input/input.h"
#include "runtime/function/script/script_engine.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/world/components.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/tilemap/tileset_asset.h"
#include "runtime/core/utils/json.h"

#include "mono/metadata/class.h"
#include "mono/metadata/image.h"
#include "mono/metadata/object.h"
#include "mono/metadata/reflection.h"
#include "mono/utils/mono-publib.h"

namespace dodoe {

    namespace {

#define DO_ADD_INTERNAL_CALL(name) mono_add_internal_call("GreenCake.InternalCalls::" #name, name);

        static ScriptEngine* s_ScriptEngine = nullptr;

        static std::unordered_map<MonoType*, std::function<bool(Entity)>>  s_EntityHasComponentFuncUmap;
        static std::unordered_map<MonoClass*, std::function<void(Entity)>> s_EntityAddComponentFuncUmap;
        static std::unordered_map<MonoType*, std::function<void(Entity)>>  s_EntityRemoveComponentFuncUmap;

        template<typename... T>
        struct ComponentGroup { };

        using NativeComponents = ComponentGroup<
            IDComponent,
            TagComponent,
            TransformComponent,
            Animation2dComponent,
            Camera2dComponent,
            BoxCollider2dComponent,
            MeshRendererComponent,
            Rigidbody2dComponent,
            SpriteRendererComponent,
            TilemapComponent,
            TileLayerComponent
        >;

        template<typename TComponent>
        static std::string ResolveManagedComponentName() {
            std::string_view type_name = typeid(TComponent).name();
            size_t pos = type_name.find_last_of(':');
            std::string_view struct_name = (pos == std::string_view::npos) ? type_name : type_name.substr(pos + 1);

            if (struct_name.starts_with("struct ")) {
                struct_name.remove_prefix(7);
            } else if (struct_name.starts_with("class ")) {
                struct_name.remove_prefix(6);
            }

            return std::string(struct_name);
        }

        template<typename TComponent>
        static void RegisterNativeComponent() {
            const std::string managed_component_name = ResolveManagedComponentName<TComponent>();
            MonoClass* managed_class = mono_class_from_name(s_ScriptEngine->getCoreImage(), "GreenCake", managed_component_name.c_str());
            if (!managed_class) {
                DO_ERROR("Could not find managed component GreenCake.{} for native registration.", managed_component_name);
                return;
            }

            MonoType* managed_type = mono_class_get_type(managed_class);
            if (!managed_type) {
                DO_ERROR("Could not resolve managed type for GreenCake.{}.", managed_component_name);
                return;
            }

            s_EntityHasComponentFuncUmap[managed_type] = [](Entity entity) {
                return entity.hasComponent<TComponent>();
            };

            s_EntityAddComponentFuncUmap[managed_class] = [](Entity entity) {
                if (!entity.hasComponent<TComponent>()) {
                    entity.addComponent<TComponent>();
                }
            };

            s_EntityRemoveComponentFuncUmap[managed_type] = [](Entity entity) {
                if (entity.hasComponent<TComponent>()) {
                    entity.removeComponent<TComponent>();
                }
            };
        }

        template<typename... TComponent>
        static void RegisterNativeComponents() {
            (RegisterNativeComponent<TComponent>(), ...);
        }

        template<typename... TComponent>
        static void RegisterNativeComponents(ComponentGroup<TComponent...>) {
            RegisterNativeComponents<TComponent...>();
        }

        static Scene* GetCurrentScene() {
            auto& app = Application::Self();
            Scene* scene = app.context().world->getCurrentScene();
            DO_ASSERT(scene);
            return scene;
        }

        static Entity TryGetEntityByUuid(const uint64_t entity_uuid) {
            if (Scene* scene = GetCurrentScene()) {
                Entity entity = scene->tryGetEntityByUUID(Uuid(entity_uuid));
                if (entity.valid()) {
                    return entity;
                }
            }

            if (auto& app = Application::Self(); app.context().script_system) {
                if (auto* runtime = app.context().script_system->getMonoRuntime()) {
                    runtime->removeEntityFromManagedWorld(entity_uuid);
                }
            }
            return {};
        }

        static void Native_Log(MonoString* message) {
            char* c_message_str = mono_string_to_utf8(message);
            std::string message_string(c_message_str);
            mono_free(c_message_str);
            LOG_INFO("{}", message_string);
        }

        static std::string MonoStringToStdString(MonoString* value) {
            if (!value) {
                return {};
            }

            char* chars = mono_string_to_utf8(value);
            std::string result = chars ? chars : "";
            mono_free(chars);
            return result;
        }

        static MonoString* StdStringToMonoString(const std::string& value) {
            return mono_string_new(s_ScriptEngine->getCoreDomain(), value.c_str());
        }

        template<typename TComponent>
        static TComponent* TryGetComponent(const uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (!entity.valid()) {
                return nullptr;
            }

            if (!entity.hasComponent<TComponent>()) {
                DO_ERROR("Entity uuid {} does not have component {}.", entity_uuid, ResolveManagedComponentName<TComponent>());
                return nullptr;
            }

            return &entity.getComponent<TComponent>();
        }

        static bool Native_EntityHasComponent(uint64_t entity_uuid, MonoReflectionType* component_type) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid()) {
                MonoType* mono_type = mono_reflection_type_get_type(component_type);
                if (s_EntityHasComponentFuncUmap.contains(mono_type)) {
                    return s_EntityHasComponentFuncUmap.at(mono_type)(entity);
                }
                DO_ERROR("No has-component injector registered for the requested managed type.");
                return false;
            }
            return false;
        }

        static bool Native_ComponentExists(uint64_t entity_uuid, MonoReflectionType* component_type) {
            (void)entity_uuid;

            MonoType* mono_type = mono_reflection_type_get_type(component_type);
            return s_EntityHasComponentFuncUmap.contains(mono_type)
                || s_EntityRemoveComponentFuncUmap.contains(mono_type);
        }

        static void Native_EntityAddComponent(uint64_t entity_uuid, MonoObject* component) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid()) {
                MonoClass* component_class = mono_object_get_class(component);
                if (s_EntityAddComponentFuncUmap.contains(component_class)) {
                    s_EntityAddComponentFuncUmap.at(component_class)(entity);
                    return;
                }
                DO_ERROR("No add-component injector registered for the requested managed class.");
                return;
            }
            return;
        }

        static void Native_EntityRemoveComponent(uint64_t entity_uuid, MonoReflectionType* component_type) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid()) {
                MonoType* mono_type = mono_reflection_type_get_type(component_type);
                if (s_EntityRemoveComponentFuncUmap.contains(mono_type)) {
                    return s_EntityRemoveComponentFuncUmap.at(mono_type)(entity);
                }
                DO_ERROR("No remove-component injector registered for the requested managed type.");
                return;
            }
            return;
        }

        static uint64_t Native_IDComponentGetID(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<IDComponent>(entity_uuid)) {
                return static_cast<uint64_t>(component->id);
            }
            return 0;
        }

        static MonoString* Native_IDComponentGetName(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<IDComponent>(entity_uuid)) {
                return StdStringToMonoString(component->name);
            }
            return StdStringToMonoString("");
        }

        static void Native_IDComponentSetName(uint64_t entity_uuid, MonoString* name) {
            if (auto* component = TryGetComponent<IDComponent>(entity_uuid)) {
                component->setName(MonoStringToStdString(name));
            }
        }

        static MonoString* Native_TagComponentGetTag(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<TagComponent>(entity_uuid)) {
                return StdStringToMonoString(component->getTag());
            }
            return StdStringToMonoString("");
        }

        static void Native_TagComponentSetTag(uint64_t entity_uuid, MonoString* tag) {
            if (auto* component = TryGetComponent<TagComponent>(entity_uuid)) {
                component->setTag(MonoStringToStdString(tag));
            }
        }

        static void Native_TransfromComponentGetPosition(uint64_t entity_uuid, Vector3f* position) {
            if (position) {
                *position = {};
            }

            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (position) {
                    *position = component->getPosition();
                }
            }
        }

        static void Native_TransfromComponentSetPosition(uint64_t entity_uuid, Vector3f* position) {
            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (position) {
                    component->setPosition(*position);
                }
            }
        }

        static void Native_TransfromComponentGetRotation(uint64_t entity_uuid, Vector3f* rotation) {
            if (rotation) {
                *rotation = {};
            }

            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (rotation) {
                    *rotation = component->getRotation();
                }
            }
        }

        static void Native_TransfromComponentSetRotation(uint64_t entity_uuid, Vector3f* rotation) {
            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (rotation) {
                    component->setRotation(*rotation);
                }
            }
        }

        static void Native_TransfromComponentGetScale(uint64_t entity_uuid, Vector3f* scale) {
            if (scale) {
                *scale = {1.0f, 1.0f, 1.0f};
            }

            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (scale) {
                    *scale = component->getScale();
                }
            }
        }

        static void Native_TransfromComponentSetScale(uint64_t entity_uuid, Vector3f* scale) {
            if (auto* component = TryGetComponent<TransformComponent>(entity_uuid)) {
                if (scale) {
                    component->setScale(*scale);
                }
            }
        }

        static uint32_t Native_Animation2dComponentGetCurrentAnimationID(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                return component->cur_anim_id;
            }
            return 0;
        }

        static void Native_Animation2dComponentSetCurrentAnimationID(uint64_t entity_uuid, uint32_t animation_id) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                component->cur_anim_id = animation_id;
            }
        }

        static uint64_t Native_Animation2dComponentGetCurrentFrameID(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                return static_cast<uint64_t>(component->cur_frame_id);
            }
            return 0;
        }

        static void Native_Animation2dComponentSetCurrentFrameID(uint64_t entity_uuid, uint64_t frame_id) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                component->cur_frame_id = static_cast<size_t>(frame_id);
            }
        }

        static float Native_Animation2dComponentGetCurrentTimeDuration(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                return component->cur_time_duration;
            }
            return 0.0f;
        }

        static void Native_Animation2dComponentSetCurrentTimeDuration(uint64_t entity_uuid, float duration) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                component->cur_time_duration = duration;
            }
        }

        static float Native_Animation2dComponentGetSpeed(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                return component->speed;
            }
            return 1.0f;
        }

        static void Native_Animation2dComponentSetSpeed(uint64_t entity_uuid, float speed) {
            if (auto* component = TryGetComponent<Animation2dComponent>(entity_uuid)) {
                component->speed = speed;
            }
        }

        static int32_t Native_Camera2dComponentGetType(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                return static_cast<int32_t>(component->type);
            }
            return 0;
        }

        static void Native_Camera2dComponentSetType(uint64_t entity_uuid, int32_t camera_type) {
            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                component->setCameraType(static_cast<CameraType>(camera_type));
            }
        }

        static float Native_Camera2dComponentGetZoom(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                return component->zoom;
            }
            return 1.0f;
        }

        static void Native_Camera2dComponentSetZoom(uint64_t entity_uuid, float zoom) {
            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                component->setZoom(zoom);
            }
        }

        static void Native_Camera2dComponentGetBackground(uint64_t entity_uuid, Color* color) {
            if (color) {
                *color = Color::white();
            }

            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                if (color) {
                    *color = component->background;
                }
            }
        }

        static void Native_Camera2dComponentSetBackground(uint64_t entity_uuid, Color* color) {
            if (auto* component = TryGetComponent<Camera2dComponent>(entity_uuid)) {
                if (color) {
                    component->setBackgroundColor(*color);
                }
            }
        }

        static void Native_BoxCollider2dComponentGetOffset(uint64_t entity_uuid, Vector2f* offset) {
            if (offset) {
                *offset = {};
            }

            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                if (offset) {
                    *offset = component->offset;
                }
            }
        }

        static void Native_BoxCollider2dComponentSetOffset(uint64_t entity_uuid, Vector2f* offset) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                if (offset) {
                    component->offset = *offset;
                }
            }
        }

        static void Native_BoxCollider2dComponentGetSize(uint64_t entity_uuid, Vector2f* size) {
            if (size) {
                *size = {};
            }

            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                if (size) {
                    *size = component->size;
                }
            }
        }

        static void Native_BoxCollider2dComponentSetSize(uint64_t entity_uuid, Vector2f* size) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                if (size) {
                    component->size = *size;
                }
            }
        }

        static float Native_BoxCollider2dComponentGetDensity(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                return component->density;
            }
            return 1.0f;
        }

        static void Native_BoxCollider2dComponentSetDensity(uint64_t entity_uuid, float density) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                component->density = density;
            }
        }

        static float Native_BoxCollider2dComponentGetFriction(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                return component->friction;
            }
            return 0.5f;
        }

        static void Native_BoxCollider2dComponentSetFriction(uint64_t entity_uuid, float friction) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                component->friction = friction;
            }
        }

        static float Native_BoxCollider2dComponentGetRestitution(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                return component->restitution;
            }
            return 0.0f;
        }

        static void Native_BoxCollider2dComponentSetRestitution(uint64_t entity_uuid, float restitution) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                component->restitution = restitution;
            }
        }

        static float Native_BoxCollider2dComponentGetRestitutionThreshold(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                return component->restitution_threshold;
            }
            return 0.5f;
        }

        static void Native_BoxCollider2dComponentSetRestitutionThreshold(uint64_t entity_uuid, float restitution_threshold) {
            if (auto* component = TryGetComponent<BoxCollider2dComponent>(entity_uuid)) {
                component->restitution_threshold = restitution_threshold;
            }
        }

        static int32_t Native_MeshRendererComponentGetValue(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<MeshRendererComponent>(entity_uuid)) {
                return component->mesh ? 1 : 0;
            }
            return 0;
        }

        static void Native_MeshRendererComponentSetValue(uint64_t entity_uuid, int32_t value) {
            if (auto* component = TryGetComponent<MeshRendererComponent>(entity_uuid)) {
                component->dirty = component->dirty || (value != 0);
            }
        }

        static int32_t Native_Rigidbody2dComponentGetType(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                return static_cast<int32_t>(component->type);
            }
            return 0;
        }

        static void Native_Rigidbody2dComponentSetType(uint64_t entity_uuid, int32_t body_type) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                component->type = static_cast<Rigidbody2dComponent::BodyType>(body_type);
            }
        }

        static float Native_Rigidbody2dComponentGetGravityScale(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                return component->gravity_scale;
            }
            return 1.0f;
        }

        static void Native_Rigidbody2dComponentSetGravityScale(uint64_t entity_uuid, float gravity_scale) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                component->gravity_scale = gravity_scale;
            }
        }

        static bool Native_Rigidbody2dComponentGetFixedRotation(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                return component->fixed_rotation;
            }
            return false;
        }

        static void Native_Rigidbody2dComponentSetFixedRotation(uint64_t entity_uuid, bool fixed_rotation) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                component->fixed_rotation = fixed_rotation;
            }
        }

        static void Native_Rigidbody2dComponentSetLinearVelocity(uint64_t entity_uuid, Vector2f* velocity) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                if (velocity) {
                    component->setLinearVelocity(*velocity);
                }
            }
        }

        static void Native_Rigidbody2dComponentApplyForceToCenter(uint64_t entity_uuid, Vector2f* force, bool wake) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                if (force) {
                    component->applyForceToCenter(*force, wake);
                }
            }
        }

        static void Native_Rigidbody2dComponentApplyLinearImpulseToCenter(uint64_t entity_uuid, Vector2f* impulse, bool wake) {
            if (auto* component = TryGetComponent<Rigidbody2dComponent>(entity_uuid)) {
                if (impulse) {
                    component->applyLinearImpulseToCenter(*impulse, wake);
                }
            }
        }

        static uint32_t Native_SpriteRendererComponentGetTextureID(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                return component->asset_ref.path_id;
            }
            return 0;
        }

        static void Native_SpriteRendererComponentSetTextureID(uint64_t entity_uuid, uint32_t texture_id) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                component->asset_ref.type = AssetType::Texture;
                component->asset_ref.path_id = texture_id;
            }
        }

        static bool Native_SpriteRendererComponentGetFlip(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                return component->flip;
            }
            return false;
        }

        static void Native_SpriteRendererComponentSetFlip(uint64_t entity_uuid, bool flip) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                component->flip = flip;
            }
        }

        static void Native_SpriteRendererComponentGetPivot(uint64_t entity_uuid, Vector2f* pivot) {
            if (pivot) {
                *pivot = {};
            }

            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                if (pivot) {
                    *pivot = component->pivot;
                }
            }
        }

        static void Native_SpriteRendererComponentSetPivot(uint64_t entity_uuid, Vector2f* pivot) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                if (pivot) {
                    component->pivot = *pivot;
                }
            }
        }

        static float Native_SpriteRendererComponentGetDepth(uint64_t entity_uuid) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                return component->depth_;
            }
            return 0.0f;
        }

        static void Native_SpriteRendererComponentSetDepth(uint64_t entity_uuid, float depth) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                component->depth_ = depth;
            }
        }

        static void Native_SpriteRendererComponentGetColor(uint64_t entity_uuid, Color* color) {
            if (color) {
                *color = {};
            }

            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                if (color) {
                    *color = component->color;
                }
            }
        }

        static void Native_SpriteRendererComponentSetColor(uint64_t entity_uuid, Color* color) {
            if (auto* component = TryGetComponent<SpriteRendererComponent>(entity_uuid)) {
                if (color) {
                    component->color = *color;
                }
            }
        }

        static bool Native_IsKeyDown(int key_code) {
            return Input::IsKeyPressed(static_cast<KeyCode>(key_code));
        }

        static float Native_TimeGetDeltaTime() {
            auto& app = Application::Self();
            auto* time_system = app.context().time_system.get();
            if (!time_system) {
                return 0.0f;
            }
            return time_system->delta_time();
        }

        static uint64_t Native_CreateEntity(MonoString* name) {
            Scene* scene = GetCurrentScene();
            if (!scene) {
                return 0;
            }

            std::string entity_name = "Entity";
            if (name) {
                char* c_name = mono_string_to_utf8(name);
                entity_name = c_name ? c_name : entity_name;
                mono_free(c_name);
            }

            Entity entity = scene->createEntity(entity_name);
            if (!entity.valid()) {
                DO_ERROR("Native_CreateEntity failed to create a valid entity.");
                return 0;
            }

            return static_cast<uint64_t>(entity.uuid());
        }

        static void Native_DestroyEntity(uint64_t entity_uuid) {
            Scene* scene = GetCurrentScene();
            if (!scene) {
                return;
            }

            Entity entity = scene->getEntityByUUID(Uuid(entity_uuid));
            if (!entity.valid()) {
                DO_ERROR("Native_DestroyEntity: uuid {} not found.", entity_uuid);
                return;
            }

            scene->destroyEntity(entity);
            if (auto& app = Application::Self(); app.context().script_system) {
                if (auto* runtime = app.context().script_system->getMonoRuntime()) {
                    runtime->removeEntityFromManagedWorld(entity_uuid);
                }
            }
        }

        static void Native_TilemapSetData(uint64_t entity_uuid, int map_width, int map_height, int tile_width, int tile_height) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (!entity.valid()) {
                return;
            }

            if (!entity.hasComponent<TilemapComponent>()) {
                entity.addComponent<TilemapComponent>();
            }

            auto& map = entity.getComponent<TilemapComponent>();
            map.map_width = static_cast<uint32_t>(map_width);
            map.map_height = static_cast<uint32_t>(map_height);
            map.tile_width = static_cast<uint32_t>(tile_width);
            map.tile_height = static_cast<uint32_t>(tile_height);
            map.dirty = true;
        }

        static void Native_TilemapAddTileset(uint64_t entity_uuid, MonoString* tileset_json) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (!entity.valid() || !entity.hasComponent<TilemapComponent>()) {
                return;
            }

            std::string json_str = MonoStringToStdString(tileset_json);
            if (json_str.empty()) {
                return;
            }

            auto& map = entity.getComponent<TilemapComponent>();
            auto ts = create_ref<TilesetAsset>();

            try {
                Json j = Json::parse(json_str);
                if (j.contains("Name")) ts->name = j["Name"].get<std::string>();
                if (j.contains("FirstGid")) ts->first_gid = j["FirstGid"].get<UInt32>();
                if (j.contains("TileWidth")) ts->tile_width = j["TileWidth"].get<UInt32>();
                if (j.contains("TileHeight")) ts->tile_height = j["TileHeight"].get<UInt32>();
                if (j.contains("Columns")) ts->columns = j["Columns"].get<UInt32>();
                if (j.contains("TileCount")) ts->tile_count = j["TileCount"].get<UInt32>();
                if (j.contains("ImagePath")) ts->image_path = j["ImagePath"].get<std::string>();
                if (j.contains("TextureId")) ts->texture_id = j["TextureId"].get<UInt32>();
            } catch (const std::exception& e) {
                DO_ERROR("Native_TilemapAddTileset: JSON parse error: {}", e.what());
                return;
            }

            map.tilesets.push_back(std::move(ts));
            map.dirty = true;
        }

        static void Native_TileLayerSetData(uint64_t entity_uuid, MonoArray* tiles_array,
                int width, int height, MonoString* name, bool visible, float opacity, int offset_x, int offset_y) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (!entity.valid()) {
                return;
            }

            if (!entity.hasComponent<TileLayerComponent>()) {
                entity.addComponent<TileLayerComponent>();
            }

            auto& layer = entity.getComponent<TileLayerComponent>();
            layer.layer_width = static_cast<uint32_t>(width);
            layer.layer_height = static_cast<uint32_t>(height);
            layer.layer_name = MonoStringToStdString(name);
            layer.visible = visible;
            layer.opacity = opacity;
            layer.offset_x = static_cast<int32_t>(offset_x);
            layer.offset_y = static_cast<int32_t>(offset_y);

            if (tiles_array) {
                uintptr_t length = mono_array_length(tiles_array);
                layer.tiles.resize(length);
                for (uintptr_t i = 0; i < length; ++i) {
                    layer.tiles[i] = mono_array_get(tiles_array, uint32_t, i);
                }
            }
        }

        static void Native_EntitySetParent(uint64_t child_uuid, uint64_t parent_uuid) {
            Entity child = TryGetEntityByUuid(child_uuid);
            Entity parent = TryGetEntityByUuid(parent_uuid);
            if (!child.valid() || !parent.valid()) {
                return;
            }

            if (!child.hasComponent<HierarchyComponent>()) {
                child.addComponent<HierarchyComponent>();
            }
            if (!parent.hasComponent<HierarchyComponent>()) {
                parent.addComponent<HierarchyComponent>();
            }

            auto& child_hier = child.getComponent<HierarchyComponent>();
            auto& parent_hier = parent.getComponent<HierarchyComponent>();

            child_hier.parent_uuid = parent.uuid();
            child_hier.parent = parent;
            child_hier.dirty = true;

            parent_hier.children.push_back(child);
            parent_hier.child_count = static_cast<int>(parent_hier.children.size());
            parent_hier.dirty = true;
        }

        static uint32_t Native_TilemapComponentGetMapWidth(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TilemapComponent>()
                ? entity.getComponent<TilemapComponent>().map_width : 0u;
        }

        static void Native_TilemapComponentSetMapWidth(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TilemapComponent>()) {
                entity.getComponent<TilemapComponent>().map_width = value;
                entity.getComponent<TilemapComponent>().dirty = true;
            }
        }

        static uint32_t Native_TilemapComponentGetMapHeight(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TilemapComponent>()
                ? entity.getComponent<TilemapComponent>().map_height : 0u;
        }

        static void Native_TilemapComponentSetMapHeight(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TilemapComponent>()) {
                entity.getComponent<TilemapComponent>().map_height = value;
                entity.getComponent<TilemapComponent>().dirty = true;
            }
        }

        static uint32_t Native_TilemapComponentGetTileWidth(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TilemapComponent>()
                ? entity.getComponent<TilemapComponent>().tile_width : 16u;
        }

        static void Native_TilemapComponentSetTileWidth(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TilemapComponent>()) {
                entity.getComponent<TilemapComponent>().tile_width = value;
                entity.getComponent<TilemapComponent>().dirty = true;
            }
        }

        static uint32_t Native_TilemapComponentGetTileHeight(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TilemapComponent>()
                ? entity.getComponent<TilemapComponent>().tile_height : 16u;
        }

        static void Native_TilemapComponentSetTileHeight(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TilemapComponent>()) {
                entity.getComponent<TilemapComponent>().tile_height = value;
                entity.getComponent<TilemapComponent>().dirty = true;
            }
        }

        static MonoString* Native_TileLayerComponentGetLayerName(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                return StdStringToMonoString(entity.getComponent<TileLayerComponent>().layer_name);
            }
            return StdStringToMonoString("");
        }

        static void Native_TileLayerComponentSetLayerName(uint64_t entity_uuid, MonoString* value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().layer_name = MonoStringToStdString(value);
            }
        }

        static uint32_t Native_TileLayerComponentGetLayerWidth(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                ? entity.getComponent<TileLayerComponent>().layer_width : 0u;
        }

        static void Native_TileLayerComponentSetLayerWidth(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().layer_width = value;
            }
        }

        static uint32_t Native_TileLayerComponentGetLayerHeight(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                ? entity.getComponent<TileLayerComponent>().layer_height : 0u;
        }

        static void Native_TileLayerComponentSetLayerHeight(uint64_t entity_uuid, uint32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().layer_height = value;
            }
        }

        static bool Native_TileLayerComponentGetVisible(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                && entity.getComponent<TileLayerComponent>().visible;
        }

        static void Native_TileLayerComponentSetVisible(uint64_t entity_uuid, bool value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().visible = value;
            }
        }

        static float Native_TileLayerComponentGetOpacity(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                ? entity.getComponent<TileLayerComponent>().opacity : 1.0f;
        }

        static void Native_TileLayerComponentSetOpacity(uint64_t entity_uuid, float value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().opacity = value;
            }
        }

        static int32_t Native_TileLayerComponentGetOffsetX(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                ? entity.getComponent<TileLayerComponent>().offset_x : 0;
        }

        static void Native_TileLayerComponentSetOffsetX(uint64_t entity_uuid, int32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().offset_x = value;
            }
        }

        static int32_t Native_TileLayerComponentGetOffsetY(uint64_t entity_uuid) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            return entity.valid() && entity.hasComponent<TileLayerComponent>()
                ? entity.getComponent<TileLayerComponent>().offset_y : 0;
        }

        static void Native_TileLayerComponentSetOffsetY(uint64_t entity_uuid, int32_t value) {
            Entity entity = TryGetEntityByUuid(entity_uuid);
            if (entity.valid() && entity.hasComponent<TileLayerComponent>()) {
                entity.getComponent<TileLayerComponent>().offset_y = value;
            }
        }

    }

    void ScriptGlue::Initialize(ScriptEngine* engine) {
        s_ScriptEngine = engine;
    }

    void ScriptGlue::Shutdown() {
        s_ScriptEngine = nullptr;
        s_EntityHasComponentFuncUmap.clear();
        s_EntityAddComponentFuncUmap.clear();
        s_EntityRemoveComponentFuncUmap.clear();
    }

    void ScriptGlue::Register() {
        if (!s_ScriptEngine) {
            return;
        }

        RegisterComponents();
        RegisterFunctions();
    }

    void ScriptGlue::RegisterComponents() {
        s_EntityHasComponentFuncUmap.clear();
        s_EntityAddComponentFuncUmap.clear();
        s_EntityRemoveComponentFuncUmap.clear();

        RegisterNativeComponents(NativeComponents{});
    }

    void ScriptGlue::RegisterFunctions() {
        DO_ADD_INTERNAL_CALL(Native_Log);
        DO_ADD_INTERNAL_CALL(Native_EntityHasComponent);
        DO_ADD_INTERNAL_CALL(Native_ComponentExists);
        DO_ADD_INTERNAL_CALL(Native_EntityAddComponent);
        DO_ADD_INTERNAL_CALL(Native_EntityRemoveComponent);
        DO_ADD_INTERNAL_CALL(Native_IsKeyDown);
        DO_ADD_INTERNAL_CALL(Native_TimeGetDeltaTime);
        DO_ADD_INTERNAL_CALL(Native_IDComponentGetID);
        DO_ADD_INTERNAL_CALL(Native_IDComponentGetName);
        DO_ADD_INTERNAL_CALL(Native_IDComponentSetName);
        DO_ADD_INTERNAL_CALL(Native_TagComponentGetTag);
        DO_ADD_INTERNAL_CALL(Native_TagComponentSetTag);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentGetPosition);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentSetPosition);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentGetRotation);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentSetRotation);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentGetScale);
        DO_ADD_INTERNAL_CALL(Native_TransfromComponentSetScale);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentGetCurrentAnimationID);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentSetCurrentAnimationID);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentGetCurrentFrameID);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentSetCurrentFrameID);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentGetCurrentTimeDuration);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentSetCurrentTimeDuration);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentGetSpeed);
        DO_ADD_INTERNAL_CALL(Native_Animation2dComponentSetSpeed);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentGetType);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentSetType);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentGetZoom);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentSetZoom);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentGetBackground);
        DO_ADD_INTERNAL_CALL(Native_Camera2dComponentSetBackground);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetOffset);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetOffset);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetSize);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetSize);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetDensity);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetDensity);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetFriction);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetFriction);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetRestitution);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetRestitution);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentGetRestitutionThreshold);
        DO_ADD_INTERNAL_CALL(Native_BoxCollider2dComponentSetRestitutionThreshold);
        DO_ADD_INTERNAL_CALL(Native_MeshRendererComponentGetValue);
        DO_ADD_INTERNAL_CALL(Native_MeshRendererComponentSetValue);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentGetType);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentSetType);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentGetGravityScale);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentSetGravityScale);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentGetFixedRotation);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentSetFixedRotation);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentSetLinearVelocity);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentApplyForceToCenter);
        DO_ADD_INTERNAL_CALL(Native_Rigidbody2dComponentApplyLinearImpulseToCenter);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentGetTextureID);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentSetTextureID);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentGetFlip);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentSetFlip);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentGetPivot);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentSetPivot);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentGetDepth);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentSetDepth);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentGetColor);
        DO_ADD_INTERNAL_CALL(Native_SpriteRendererComponentSetColor);
        DO_ADD_INTERNAL_CALL(Native_CreateEntity);
        DO_ADD_INTERNAL_CALL(Native_DestroyEntity);
        DO_ADD_INTERNAL_CALL(Native_TilemapSetData);
        DO_ADD_INTERNAL_CALL(Native_TilemapAddTileset);
        DO_ADD_INTERNAL_CALL(Native_TileLayerSetData);
        DO_ADD_INTERNAL_CALL(Native_EntitySetParent);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentGetMapWidth);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentSetMapWidth);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentGetMapHeight);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentSetMapHeight);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentGetTileWidth);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentSetTileWidth);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentGetTileHeight);
        DO_ADD_INTERNAL_CALL(Native_TilemapComponentSetTileHeight);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetLayerName);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetLayerName);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetLayerWidth);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetLayerWidth);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetLayerHeight);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetLayerHeight);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetVisible);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetVisible);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetOpacity);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetOpacity);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetOffsetX);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetOffsetX);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentGetOffsetY);
        DO_ADD_INTERNAL_CALL(Native_TileLayerComponentSetOffsetY);

    }

} // dodoe
