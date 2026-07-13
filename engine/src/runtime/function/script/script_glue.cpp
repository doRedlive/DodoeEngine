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
#include "runtime/core/project/project.h"

namespace dodoe {

    namespace {

        static ScriptEngine* s_ScriptEngine = nullptr;

        static std::unordered_map<std::string, std::function<int(Entity)>> s_EntityHasComponentFuncUmap;
        static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityAddComponentFuncUmap;
        static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityRemoveComponentFuncUmap;

        template<typename... T> struct ComponentGroup { };

        using NativeComponents = ComponentGroup<
            IDComponent, TagComponent, TransformComponent,
            Animation2dComponent, Camera2dComponent, BoxCollider2dComponent,
            MeshRendererComponent, Rigidbody2dComponent, SpriteRendererComponent,
            TilemapComponent, TileLayerComponent
        >;

        template<typename TC>
        static std::string ResolveManagedComponentName() {
            std::string_view tn = typeid(TC).name();
            size_t p = tn.find_last_of(':');
            std::string_view n = (p == std::string_view::npos) ? tn : tn.substr(p + 1);
            if (n.starts_with("struct ")) n.remove_prefix(7);
            else if (n.starts_with("class ")) n.remove_prefix(6);
            return std::string(n);
        }

        template<typename TC>
        static void RegisterNativeComponent() {
            auto name = ResolveManagedComponentName<TC>();
            s_EntityHasComponentFuncUmap[name] = [](Entity e) { return e.hasComponent<TC>() ? 1 : 0; };
            s_EntityAddComponentFuncUmap[name] = [](Entity e) { if (!e.hasComponent<TC>()) e.addComponent<TC>(); };
            s_EntityRemoveComponentFuncUmap[name] = [](Entity e) { if (e.hasComponent<TC>()) e.removeComponent<TC>(); };
        }
        template<typename... TC>
        static void RegisterNativeComponents() { (RegisterNativeComponent<TC>(), ...); }
        template<typename... TC>
        static void RegisterNativeComponents(ComponentGroup<TC...>) { RegisterNativeComponents<TC...>(); }

        static Scene* GetCurrentScene() { Scene* s = GetWorld()->getCurrentScene(); DO_ASSERT(s); return s; }

        static Entity TryGetEntityByUuid(uint64_t uuid) {
            if (Scene* s = GetCurrentScene()) {
                Entity e = s->tryGetEntityByUUID(Uuid(uuid));
                if (e.valid()) return e;
            }
            if (GetScriptSystem())
                if (auto* r = GetScriptSystem()->getScriptRuntime())
                    r->removeEntityFromManagedWorld(uuid);
            return {};
        }

        template<typename TC>
        static TC* TryGetComponent(uint64_t uuid) {
            Entity e = TryGetEntityByUuid(uuid);
            if (!e.valid()) return nullptr;
            if (!e.hasComponent<TC>()) {
                DO_ERROR("Entity {} has no {}", uuid, ResolveManagedComponentName<TC>());
                return nullptr;
            }
            return &e.getComponent<TC>();
        }

#define DEF_STR_RET(id) thread_local static std::string _s_##id

        static void native_log(const char* msg) { if (msg) LOG_INFO("{}", msg); }

        static int native_entity_has_component(uint64_t uuid, const char* type) {
            Entity e = TryGetEntityByUuid(uuid);
            return (e.valid() && type && s_EntityHasComponentFuncUmap.count(type)) ? s_EntityHasComponentFuncUmap.at(type)(e) : 0;
        }
        static int native_component_exists(uint64_t, const char* type) {
            if (!type) return 0;
            return (s_EntityHasComponentFuncUmap.count(type) || s_EntityRemoveComponentFuncUmap.count(type)) ? 1 : 0;
        }
        static void native_entity_add_component(uint64_t uuid, const char* type) {
            Entity e = TryGetEntityByUuid(uuid);
            if (e.valid() && type && s_EntityAddComponentFuncUmap.count(type)) s_EntityAddComponentFuncUmap.at(type)(e);
        }
        static void native_entity_remove_component(uint64_t uuid, const char* type) {
            Entity e = TryGetEntityByUuid(uuid);
            if (e.valid() && type && s_EntityRemoveComponentFuncUmap.count(type)) s_EntityRemoveComponentFuncUmap.at(type)(e);
        }

        static int native_is_key_down(int key) { return Input::IsKeyPressed(static_cast<KeyCode>(key)) ? 1 : 0; }
        static float native_time_get_delta_time() { auto* ts = GetTimeSystem(); return ts ? ts->getDeltaTime() : 0.0f; }

        static uint64_t native_id_component_get_id(uint64_t u) { auto* c = TryGetComponent<IDComponent>(u); return c ? (uint64_t)c->id : 0; }
        DEF_STR_RET(id_get_name);
        static const char* native_id_component_get_name(uint64_t u) { auto* c = TryGetComponent<IDComponent>(u); if (c) { _s_id_get_name = c->name; return _s_id_get_name.c_str(); } return ""; }
        static void native_id_component_set_name(uint64_t u, const char* v) { auto* c = TryGetComponent<IDComponent>(u); if (c && v) c->setName(v); }

        static uint64_t native_create_entity(const char* name) {
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Entity e = s->createEntity(name ? name : "Entity");
            return e.valid() ? (uint64_t)e.uuid() : 0;
        }
        static void native_destroy_entity(uint64_t u) {
            Scene* s = GetCurrentScene(); if (!s) return;
            Entity e = s->getEntityByUUID(Uuid(u));
            if (!e.valid()) { DO_ERROR("destroy_entity: {} not found", u); return; }
            s->destroyEntity(e);
            if (GetScriptSystem()) if (auto* r = GetScriptSystem()->getScriptRuntime()) r->removeEntityFromManagedWorld(u);
        }

        static void native_tilemap_set_data(uint64_t u, int w, int h, int tw, int th) {
            Entity e = TryGetEntityByUuid(u); if (!e.valid()) return;
            if (!e.hasComponent<TilemapComponent>()) e.addComponent<TilemapComponent>();
            auto& m = e.getComponent<TilemapComponent>();
            m.map_width = (uint32_t)w; m.map_height = (uint32_t)h; m.tile_width = (uint32_t)tw; m.tile_height = (uint32_t)th; m.dirty = true;
        }
        static void native_tilemap_add_tileset(uint64_t u, const char* json_str) {
            Entity e = TryGetEntityByUuid(u);
            if (!e.valid() || !e.hasComponent<TilemapComponent>() || !json_str) return;
            auto& m = e.getComponent<TilemapComponent>(); auto ts = create_ref<TilesetAsset>();
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
            } catch (...) { DO_ERROR("tilemap_add_tileset parse error"); return; }
            m.tilesets.push_back(std::move(ts)); m.dirty = true;
        }
        static void native_tile_layer_set_data(uint64_t u, const uint32_t* tiles, int len,
                int w, int h, const char* name, int vis, float opac, int ox, int oy) {
            Entity e = TryGetEntityByUuid(u); if (!e.valid()) return;
            if (!e.hasComponent<TileLayerComponent>()) e.addComponent<TileLayerComponent>();
            auto& l = e.getComponent<TileLayerComponent>();
            l.layer_width = (uint32_t)w; l.layer_height = (uint32_t)h; l.layer_name = name ? name : "";
            l.visible = (vis != 0); l.opacity = opac; l.offset_x = ox; l.offset_y = oy;
            if (tiles && len > 0) { l.tiles.resize(len); for (int i = 0; i < len; ++i) l.tiles[i] = tiles[i]; }
        }

        static void native_entity_set_parent(uint64_t child_u, uint64_t parent_u) {
            Entity child = TryGetEntityByUuid(child_u), parent = TryGetEntityByUuid(parent_u);
            if (!child.valid() || !parent.valid()) return;
            if (!child.hasComponent<HierarchyComponent>()) child.addComponent<HierarchyComponent>();
            if (!parent.hasComponent<HierarchyComponent>()) parent.addComponent<HierarchyComponent>();
            auto& ch = child.getComponent<HierarchyComponent>(); auto& ph = parent.getComponent<HierarchyComponent>();
            ch.parent_uuid = parent.uuid(); ch.parent = parent; ch.dirty = true;
            ph.children.push_back(child); ph.child_count = (int)ph.children.size(); ph.dirty = true;
        }

        DEF_STR_RET(asset_dir);
        static const char* native_get_asset_directory() { _s_asset_dir = Project::AssetDirectory().string(); return _s_asset_dir.c_str(); }
        DEF_STR_RET(object_type_name);

        static const char* native_object_get_type_name(int instanceID) {
            auto* obj = Object::FindObjectFromInstanceID((InstanceID)instanceID);
            if (!obj) return "";
            _s_object_type_name = obj->getObjectTypeName();
            return _s_object_type_name.c_str();
        }

        static int native_texture_load(const char* path) {
            if (!path || path[0] == '\0') return 0;
            auto tex = Texture::Load(String(path));
            return tex ? (int)tex->getInstanceID() : 0;
        }

#define FOR_EACH_NATIVE_BINDING(X) \
    X(native_log, void, (const char* msg), msg) \
    X(native_entity_has_component, int, (uint64_t e, const char* type), e, type) \
    X(native_component_exists, int, (uint64_t e, const char* type), e, type) \
    X(native_entity_add_component, void, (uint64_t e, const char* type), e, type) \
    X(native_entity_remove_component, void, (uint64_t e, const char* type), e, type) \
    X(native_is_key_down, int, (int key), key) \
    X(native_time_get_delta_time, float, (), ) \
    X(native_id_component_get_id, uint64_t, (uint64_t e), e) \
    X(native_id_component_get_name, const char*, (uint64_t e), e) \
    X(native_id_component_set_name, void, (uint64_t e, const char* v), e, v) \
    /* === NATIVE_BINDINGS_GENERATED_START === */ \
X(native_Rigidbody2dComponent_gravity_scale_get, float, (uint64_t e), e) \
    X(native_Rigidbody2dComponent_gravity_scale_set, void, (uint64_t e, float v), e, v) \
    X(native_Rigidbody2dComponent_fixed_rotation_get, bool, (uint64_t e), e) \
    X(native_Rigidbody2dComponent_fixed_rotation_set, void, (uint64_t e, bool v), e, v) \
    X(native_MeshRendererComponent_visible_get, bool, (uint64_t e), e) \
    X(native_MeshRendererComponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_MeshRendererComponent_cast_shadow_get, bool, (uint64_t e), e) \
    X(native_MeshRendererComponent_cast_shadow_set, void, (uint64_t e, bool v), e, v) \
    X(native_Animation2dComponent_cur_time_duration_get, float, (uint64_t e), e) \
    X(native_Animation2dComponent_cur_time_duration_set, void, (uint64_t e, float v), e, v) \
    X(native_Animation2dComponent_speed_get, float, (uint64_t e), e) \
    X(native_Animation2dComponent_speed_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_zoom_get, float, (uint64_t e), e) \
    X(native_Camera2dComponent_zoom_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_fov_get, float, (uint64_t e), e) \
    X(native_Camera2dComponent_fov_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_near_plane_get, float, (uint64_t e), e) \
    X(native_Camera2dComponent_near_plane_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_far_plane_get, float, (uint64_t e), e) \
    X(native_Camera2dComponent_far_plane_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_aspect_ratio_get, float, (uint64_t e), e) \
    X(native_Camera2dComponent_aspect_ratio_set, void, (uint64_t e, float v), e, v) \
    X(native_Camera2dComponent_background_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_Camera2dComponent_background_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_BoxCollider2dComponent_offset_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_BoxCollider2dComponent_offset_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_BoxCollider2dComponent_size_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_BoxCollider2dComponent_size_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_BoxCollider2dComponent_density_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_friction_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_restitution_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_restitution_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_restitution_threshold_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_restitution_threshold_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleRendererComponent_radius_get, float, (uint64_t e), e) \
    X(native_CircleRendererComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_CircleRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_CircleRendererComponent_segments_get, uint, (uint64_t e), e) \
    X(native_CircleRendererComponent_segments_set, void, (uint64_t e, uint v), e, v) \
    X(native_CircleRendererComponent_thickness_get, float, (uint64_t e), e) \
    X(native_CircleRendererComponent_thickness_set, void, (uint64_t e, float v), e, v) \
    X(native_TilemapComponent_map_width_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_map_width_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_map_height_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_map_height_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_tile_width_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_tile_width_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_tile_height_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_tile_height_set, void, (uint64_t e, uint v), e, v) \
    X(native_FoliageRendererInstance_position_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_FoliageRendererInstance_position_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_FoliageRendererInstance_rotation_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_FoliageRendererInstance_rotation_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_FoliageRendererInstance_scale_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_FoliageRendererInstance_scale_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_FoliageRendererInstance_color_tint_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_FoliageRendererInstance_color_tint_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_FoliageRendererInstance_wind_phase_get, float, (uint64_t e), e) \
    X(native_FoliageRendererInstance_wind_phase_set, void, (uint64_t e, float v), e, v) \
    X(native_FoliageRendererInstance_variation_get, float, (uint64_t e), e) \
    X(native_FoliageRendererInstance_variation_set, void, (uint64_t e, float v), e, v) \
    X(native_FoliageRendererComponent_visible_get, bool, (uint64_t e), e) \
    X(native_FoliageRendererComponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_FoliageRendererComponent_cast_shadow_get, bool, (uint64_t e), e) \
    X(native_FoliageRendererComponent_cast_shadow_set, void, (uint64_t e, bool v), e, v) \
    X(native_FoliageRendererComponent_instance_bounds_extent_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_FoliageRendererComponent_instance_bounds_extent_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_PointLightComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_PointLightComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_PointLightComponent_intensity_get, float, (uint64_t e), e) \
    X(native_PointLightComponent_intensity_set, void, (uint64_t e, float v), e, v) \
    X(native_PointLightComponent_radius_get, float, (uint64_t e), e) \
    X(native_PointLightComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_PointLightComponent_range_get, float, (uint64_t e), e) \
    X(native_PointLightComponent_range_set, void, (uint64_t e, float v), e, v) \
    X(native_SpotLightComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_SpotLightComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_SpotLightComponent_intensity_get, float, (uint64_t e), e) \
    X(native_SpotLightComponent_intensity_set, void, (uint64_t e, float v), e, v) \
    X(native_SpotLightComponent_radius_get, float, (uint64_t e), e) \
    X(native_SpotLightComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_SpotLightComponent_range_get, float, (uint64_t e), e) \
    X(native_SpotLightComponent_range_set, void, (uint64_t e, float v), e, v) \
    X(native_SpotLightComponent_inner_angle_get, float, (uint64_t e), e) \
    X(native_SpotLightComponent_inner_angle_set, void, (uint64_t e, float v), e, v) \
    X(native_SpotLightComponent_outer_angle_get, float, (uint64_t e), e) \
    X(native_SpotLightComponent_outer_angle_set, void, (uint64_t e, float v), e, v) \
    X(native_RectRendererComponent_size_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_RectRendererComponent_size_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_RectRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_RectRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_RectRendererComponent_thickness_get, float, (uint64_t e), e) \
    X(native_RectRendererComponent_thickness_set, void, (uint64_t e, float v), e, v) \
    X(native_LineRendererComponent_direction_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_LineRendererComponent_direction_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_LineRendererComponent_length_get, float, (uint64_t e), e) \
    X(native_LineRendererComponent_length_set, void, (uint64_t e, float v), e, v) \
    X(native_LineRendererComponent_thickness_get, float, (uint64_t e), e) \
    X(native_LineRendererComponent_thickness_set, void, (uint64_t e, float v), e, v) \
    X(native_LineRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_LineRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_SkyLightComponent_intensity_get, float, (uint64_t e), e) \
    X(native_SkyLightComponent_intensity_set, void, (uint64_t e, float v), e, v) \
    X(native_SpriteRendererComponent_texture_get, int, (uint64_t e), e) \
    X(native_SpriteRendererComponent_texture_set, void, (uint64_t e, int v), e, v) \
    X(native_SpriteRendererComponent_flip_get, bool, (uint64_t e), e) \
    X(native_SpriteRendererComponent_flip_set, void, (uint64_t e, bool v), e, v) \
    X(native_SpriteRendererComponent_pivot_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_SpriteRendererComponent_pivot_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_SpriteRendererComponent_depth_get, float, (uint64_t e), e) \
    X(native_SpriteRendererComponent_depth_set, void, (uint64_t e, float v), e, v) \
    X(native_SpriteRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_SpriteRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_TagComponent_tag_get, const char*, (uint64_t e), e) \
    X(native_TagComponent_tag_set, void, (uint64_t e, const char* v), e, v) \
    X(native_TransformComponent_position_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_TransformComponent_position_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_TransformComponent_rotation_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_TransformComponent_rotation_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_TransformComponent_scale_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_TransformComponent_scale_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_HierarchyComponent_parent_uuid_get, uint64_t, (uint64_t e), e) \
    X(native_HierarchyComponent_parent_uuid_set, void, (uint64_t e, uint64_t v), e, v) \
    X(native_HierarchyComponent_child_count_get, int, (uint64_t e), e) \
    X(native_HierarchyComponent_child_count_set, void, (uint64_t e, int v), e, v) \
    X(native_TileLayerComponent_layer_name_get, const char*, (uint64_t e), e) \
    X(native_TileLayerComponent_layer_name_set, void, (uint64_t e, const char* v), e, v) \
    X(native_TileLayerComponent_layer_width_get, uint, (uint64_t e), e) \
    X(native_TileLayerComponent_layer_width_set, void, (uint64_t e, uint v), e, v) \
    X(native_TileLayerComponent_layer_height_get, uint, (uint64_t e), e) \
    X(native_TileLayerComponent_layer_height_set, void, (uint64_t e, uint v), e, v) \
    X(native_TileLayerComponent_visible_get, bool, (uint64_t e), e) \
    X(native_TileLayerComponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_TileLayerComponent_opacity_get, float, (uint64_t e), e) \
    X(native_TileLayerComponent_opacity_set, void, (uint64_t e, float v), e, v) \
    X(native_TileLayerComponent_offset_x_get, int, (uint64_t e), e) \
    X(native_TileLayerComponent_offset_x_set, void, (uint64_t e, int v), e, v) \
    X(native_TileLayerComponent_offset_y_get, int, (uint64_t e), e) \
    X(native_TileLayerComponent_offset_y_set, void, (uint64_t e, int v), e, v) \
/* === NATIVE_BINDINGS_GENERATED_END === */ \
    X(native_create_entity, uint64_t, (const char* name), name) \
    X(native_destroy_entity, void, (uint64_t e), e) \
    X(native_tilemap_set_data, void, (uint64_t e, int w, int h, int tw, int th), e, w, h, tw, th) \
    X(native_tilemap_add_tileset, void, (uint64_t e, const char* json_str), e, json_str) \
    X(native_tile_layer_set_data, void, (uint64_t e, const uint32_t* tiles, int len, int w, int h, const char* name, int vis, float opac, int ox, int oy), e, tiles, len, w, h, name, vis, opac, ox, oy) \
    X(native_entity_set_parent, void, (uint64_t child, uint64_t parent), child, parent) \
    X(native_get_asset_directory, const char*, (), ) \
    X(native_object_get_type_name, const char*, (int instanceID), instanceID) \
    X(native_texture_load, int, (const char* path), path)

#include "_generated/script/script_glue.generated.cpp"

#define BIND_FIELD(name, ret, sig, ...) ret (*name) sig;
        struct NativeBindings { FOR_EACH_NATIVE_BINDING(BIND_FIELD) };
#undef BIND_FIELD

        static NativeBindings s_bindings = {};

        static void FillBindings() {
#define BIND_ASSIGN(name, ret, sig, ...) s_bindings.name = name;
            FOR_EACH_NATIVE_BINDING(BIND_ASSIGN)
#undef BIND_ASSIGN
        }

    }

    void ScriptGlue::Initialize(ScriptEngine* engine) { s_ScriptEngine = engine; }

    void ScriptGlue::Shutdown() {
        s_ScriptEngine = nullptr;
        s_EntityHasComponentFuncUmap.clear();
        s_EntityAddComponentFuncUmap.clear();
        s_EntityRemoveComponentFuncUmap.clear();
    }

    void ScriptGlue::Register() { 
        if (!s_ScriptEngine) return;
        RegisterComponents(); 
        RegisterNativeBindings();
    }

    void ScriptGlue::RegisterComponents() {
        s_EntityHasComponentFuncUmap.clear();
        s_EntityAddComponentFuncUmap.clear();
        s_EntityRemoveComponentFuncUmap.clear();
        RegisterNativeComponents(NativeComponents{});
    }

    void ScriptGlue::RegisterNativeBindings() {
        if (!s_ScriptEngine) return;
        FillBindings();
        auto call = s_ScriptEngine->getCallFn();
        if (!call) { DO_ERROR("ScriptGlue: ScriptHub_Call not available"); return; }
        void* args[1] = { &s_bindings };
        call("register_natives", args, nullptr);
    }

} // dodoe
