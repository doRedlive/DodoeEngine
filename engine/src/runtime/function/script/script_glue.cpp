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

        DEF_STR_RET(tag_get_tag);
        static const char* native_tag_component_get_tag(uint64_t u) { auto* c = TryGetComponent<TagComponent>(u); if (c) { _s_tag_get_tag = c->getTag(); return _s_tag_get_tag.c_str(); } return ""; }
        static void native_tag_component_set_tag(uint64_t u, const char* v) { auto* c = TryGetComponent<TagComponent>(u); if (c && v) c->setTag(v); }

        static void native_transform_get_position(uint64_t u, float* x, float* y, float* z) {
            if (x) *x = 0; if (y) *y = 0; if (z) *z = 0;
            if (auto* c = TryGetComponent<TransformComponent>(u)) { auto p = c->getPosition(); if (x)*x=p.x; if (y)*y=p.y; if (z)*z=p.z; }
        }
        static void native_transform_set_position(uint64_t u, float x, float y, float z) { if (auto* c = TryGetComponent<TransformComponent>(u)) c->setPosition({x,y,z}); }
        static void native_transform_get_rotation(uint64_t u, float* x, float* y, float* z) {
            if (x) *x = 0; if (y) *y = 0; if (z) *z = 0;
            if (auto* c = TryGetComponent<TransformComponent>(u)) { auto r = c->getRotation(); if (x)*x=r.x; if (y)*y=r.y; if (z)*z=r.z; }
        }
        static void native_transform_set_rotation(uint64_t u, float x, float y, float z) { if (auto* c = TryGetComponent<TransformComponent>(u)) c->setRotation({x,y,z}); }
        static void native_transform_get_scale(uint64_t u, float* x, float* y, float* z) {
            if (x) *x = 1; if (y) *y = 1; if (z) *z = 1;
            if (auto* c = TryGetComponent<TransformComponent>(u)) { auto s = c->getScale(); if (x)*x=s.x; if (y)*y=s.y; if (z)*z=s.z; }
        }
        static void native_transform_set_scale(uint64_t u, float x, float y, float z) { if (auto* c = TryGetComponent<TransformComponent>(u)) c->setScale({x,y,z}); }

        static uint32_t native_animation2d_get_anim_id(uint64_t u) { auto* c = TryGetComponent<Animation2dComponent>(u); return c ? c->cur_anim_id : 0; }
        static void native_animation2d_set_anim_id(uint64_t u, uint32_t v) { if (auto* c = TryGetComponent<Animation2dComponent>(u)) c->cur_anim_id = v; }
        static uint64_t native_animation2d_get_frame_id(uint64_t u) { auto* c = TryGetComponent<Animation2dComponent>(u); return c ? (uint64_t)c->cur_frame_id : 0; }
        static void native_animation2d_set_frame_id(uint64_t u, uint64_t v) { if (auto* c = TryGetComponent<Animation2dComponent>(u)) c->cur_frame_id = (size_t)v; }
        static float native_animation2d_get_time(uint64_t u) { auto* c = TryGetComponent<Animation2dComponent>(u); return c ? c->cur_time_duration : 0; }
        static void native_animation2d_set_time(uint64_t u, float v) { if (auto* c = TryGetComponent<Animation2dComponent>(u)) c->cur_time_duration = v; }
        static float native_animation2d_get_speed(uint64_t u) { auto* c = TryGetComponent<Animation2dComponent>(u); return c ? c->speed : 1.0f; }
        static void native_animation2d_set_speed(uint64_t u, float v) { if (auto* c = TryGetComponent<Animation2dComponent>(u)) c->speed = v; }

        static int32_t native_camera2d_get_type(uint64_t u) { auto* c = TryGetComponent<Camera2dComponent>(u); return c ? (int32_t)c->type : 0; }
        static void native_camera2d_set_type(uint64_t u, int32_t v) { if (auto* c = TryGetComponent<Camera2dComponent>(u)) c->setCameraType((CameraType)v); }
        static float native_camera2d_get_zoom(uint64_t u) { auto* c = TryGetComponent<Camera2dComponent>(u); return c ? c->zoom : 1.0f; }
        static void native_camera2d_set_zoom(uint64_t u, float v) { if (auto* c = TryGetComponent<Camera2dComponent>(u)) c->setZoom(v); }
        static void native_camera2d_get_background(uint64_t u, float* r, float* g, float* b, float* a) {
            if (r) *r = 1; if (g) *g = 1; if (b) *b = 1; if (a) *a = 1;
            if (auto* c = TryGetComponent<Camera2dComponent>(u)) { auto bg = c->background; if (r)*r=bg.r; if (g)*g=bg.g; if (b)*b=bg.b; if (a)*a=bg.a; }
        }
        static void native_camera2d_set_background(uint64_t u, float r, float g, float b, float a) { if (auto* c = TryGetComponent<Camera2dComponent>(u)) c->setBackgroundColor({r,g,b,a}); }

        static void native_boxcollider2d_get_offset(uint64_t u, float* x, float* y) {
            if (x) *x = 0; if (y) *y = 0;
            if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) { if (x)*x=c->offset.x; if (y)*y=c->offset.y; }
        }
        static void native_boxcollider2d_set_offset(uint64_t u, float x, float y) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->offset = {x,y}; }
        static void native_boxcollider2d_get_size(uint64_t u, float* x, float* y) {
            if (x) *x = 0; if (y) *y = 0;
            if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) { if (x)*x=c->size.x; if (y)*y=c->size.y; }
        }
        static void native_boxcollider2d_set_size(uint64_t u, float x, float y) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->size = {x,y}; }
        static float native_boxcollider2d_get_density(uint64_t u) { auto* c = TryGetComponent<BoxCollider2dComponent>(u); return c ? c->density : 1.0f; }
        static void native_boxcollider2d_set_density(uint64_t u, float v) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->density = v; }
        static float native_boxcollider2d_get_friction(uint64_t u) { auto* c = TryGetComponent<BoxCollider2dComponent>(u); return c ? c->friction : 0.5f; }
        static void native_boxcollider2d_set_friction(uint64_t u, float v) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->friction = v; }
        static float native_boxcollider2d_get_restitution(uint64_t u) { auto* c = TryGetComponent<BoxCollider2dComponent>(u); return c ? c->restitution : 0; }
        static void native_boxcollider2d_set_restitution(uint64_t u, float v) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->restitution = v; }
        static float native_boxcollider2d_get_restitution_threshold(uint64_t u) { auto* c = TryGetComponent<BoxCollider2dComponent>(u); return c ? c->restitution_threshold : 0.5f; }
        static void native_boxcollider2d_set_restitution_threshold(uint64_t u, float v) { if (auto* c = TryGetComponent<BoxCollider2dComponent>(u)) c->restitution_threshold = v; }

        static int32_t native_mesh_renderer_get_value(uint64_t u) { auto* c = TryGetComponent<MeshRendererComponent>(u); return c ? (c->lods.empty() ? 0 : 1) : 0; }
        static void native_mesh_renderer_set_value(uint64_t u, int32_t v) { if (auto* c = TryGetComponent<MeshRendererComponent>(u)) c->dirty = c->dirty || (v != 0); }

        static int32_t native_rigidbody2d_get_type(uint64_t u) { auto* c = TryGetComponent<Rigidbody2dComponent>(u); return c ? (int32_t)c->type : 0; }
        static void native_rigidbody2d_set_type(uint64_t u, int32_t v) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->type = (Rigidbody2dComponent::BodyType)v; }
        static float native_rigidbody2d_get_gravity_scale(uint64_t u) { auto* c = TryGetComponent<Rigidbody2dComponent>(u); return c ? c->gravity_scale : 1.0f; }
        static void native_rigidbody2d_set_gravity_scale(uint64_t u, float v) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->gravity_scale = v; }
        static int native_rigidbody2d_get_fixed_rotation(uint64_t u) { auto* c = TryGetComponent<Rigidbody2dComponent>(u); return c ? (c->fixed_rotation ? 1 : 0) : 0; }
        static void native_rigidbody2d_set_fixed_rotation(uint64_t u, int v) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->fixed_rotation = (v != 0); }
        static void native_rigidbody2d_set_linear_velocity(uint64_t u, float x, float y) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->setLinearVelocity({x,y}); }
        static void native_rigidbody2d_apply_force_to_center(uint64_t u, float x, float y, int wake) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->applyForceToCenter({x,y}, wake != 0); }
        static void native_rigidbody2d_apply_linear_impulse(uint64_t u, float x, float y, int wake) { if (auto* c = TryGetComponent<Rigidbody2dComponent>(u)) c->applyLinearImpulseToCenter({x,y}, wake != 0); }

        static Int32 native_sprite_renderer_get_texture_id(UInt64 u) { auto* c = TryGetComponent<SpriteRendererComponent>(u); return c ? (Int32)c->texture.getInstanceID() : 0; }
        static void native_sprite_renderer_set_texture_id(UInt64 u, Int32 v) {
            if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) {
                auto* tex = (Texture*)Object::FindObjectFromInstanceID(v);
                if (tex) c->texture = PPtr<Texture>(tex->getFileID(), tex->getUUID(), v);
            }
        }
        static int native_sprite_renderer_get_flip(uint64_t u) { auto* c = TryGetComponent<SpriteRendererComponent>(u); return c ? (c->flip ? 1 : 0) : 0; }
        static void native_sprite_renderer_set_flip(uint64_t u, int v) { if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) c->flip = (v != 0); }
        static void native_sprite_renderer_get_pivot(uint64_t u, float* x, float* y) {
            if (x) *x = 0; if (y) *y = 0;
            if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) { if (x)*x=c->pivot.x; if (y)*y=c->pivot.y; }
        }
        static void native_sprite_renderer_set_pivot(uint64_t u, float x, float y) { if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) c->pivot = {x,y}; }
        static float native_sprite_renderer_get_depth(uint64_t u) { auto* c = TryGetComponent<SpriteRendererComponent>(u); return c ? c->depth_ : 0; }
        static void native_sprite_renderer_set_depth(uint64_t u, float v) { if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) c->depth_ = v; }
        static void native_sprite_renderer_get_color(uint64_t u, float* r, float* g, float* b, float* a) {
            if (r) *r = 1; if (g) *g = 1; if (b) *b = 1; if (a) *a = 1;
            if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) { if (r)*r=c->color.r; if (g)*g=c->color.g; if (b)*b=c->color.b; if (a)*a=c->color.a; }
        }
        static void native_sprite_renderer_set_color(uint64_t u, float r, float g, float b, float a) { if (auto* c = TryGetComponent<SpriteRendererComponent>(u)) c->color = {r,g,b,a}; }

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

#define TILEMAP_GETSET(type, name, field, defval) \
        static type native_tilemap_get_##name(uint64_t u) { Entity e = TryGetEntityByUuid(u); return (e.valid() && e.hasComponent<TilemapComponent>()) ? e.getComponent<TilemapComponent>().field : (type)(defval); } \
        static void native_tilemap_set_##name(uint64_t u, type v) { Entity e = TryGetEntityByUuid(u); if (e.valid() && e.hasComponent<TilemapComponent>()) { e.getComponent<TilemapComponent>().field = v; e.getComponent<TilemapComponent>().dirty = true; } }
        TILEMAP_GETSET(uint32_t, map_width, map_width, 0)
        TILEMAP_GETSET(uint32_t, map_height, map_height, 0)
        TILEMAP_GETSET(uint32_t, tile_width, tile_width, 16)
        TILEMAP_GETSET(uint32_t, tile_height, tile_height, 16)
#undef TILEMAP_GETSET

        DEF_STR_RET(tile_layer_name);
        static const char* native_tile_layer_get_name(uint64_t u) { Entity e = TryGetEntityByUuid(u); if (e.valid() && e.hasComponent<TileLayerComponent>()) { _s_tile_layer_name = e.getComponent<TileLayerComponent>().layer_name; return _s_tile_layer_name.c_str(); } return ""; }
        static void native_tile_layer_set_name(uint64_t u, const char* v) { Entity e = TryGetEntityByUuid(u); if (e.valid() && e.hasComponent<TileLayerComponent>() && v) e.getComponent<TileLayerComponent>().layer_name = v; }

#define TILELAYER_GETSET(type, name, field, defval) \
        static type native_tile_layer_get_##name(uint64_t u) { Entity e = TryGetEntityByUuid(u); return (e.valid() && e.hasComponent<TileLayerComponent>()) ? e.getComponent<TileLayerComponent>().field : (type)(defval); } \
        static void native_tile_layer_set_##name(uint64_t u, type v) { Entity e = TryGetEntityByUuid(u); if (e.valid() && e.hasComponent<TileLayerComponent>()) e.getComponent<TileLayerComponent>().field = v; }
        TILELAYER_GETSET(uint32_t, width, layer_width, 0)
        TILELAYER_GETSET(uint32_t, height, layer_height, 0)
        TILELAYER_GETSET(int, visible, visible, 0)
        TILELAYER_GETSET(float, opacity, opacity, 1.0f)
        TILELAYER_GETSET(int32_t, offset_x, offset_x, 0)
        TILELAYER_GETSET(int32_t, offset_y, offset_y, 0)
#undef TILELAYER_GETSET

        DEF_STR_RET(asset_dir);
        static const char* native_get_asset_directory() { _s_asset_dir = Project::AssetDirectory().string(); return _s_asset_dir.c_str(); }

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
    X(native_tag_component_get_tag, const char*, (uint64_t e), e) \
    X(native_tag_component_set_tag, void, (uint64_t e, const char* v), e, v) \
    X(native_transform_get_position, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_transform_set_position, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_transform_get_rotation, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_transform_set_rotation, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_transform_get_scale, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_transform_set_scale, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_animation2d_get_anim_id, uint32_t, (uint64_t e), e) \
    X(native_animation2d_set_anim_id, void, (uint64_t e, uint32_t v), e, v) \
    X(native_animation2d_get_frame_id, uint64_t, (uint64_t e), e) \
    X(native_animation2d_set_frame_id, void, (uint64_t e, uint64_t v), e, v) \
    X(native_animation2d_get_time, float, (uint64_t e), e) \
    X(native_animation2d_set_time, void, (uint64_t e, float v), e, v) \
    X(native_animation2d_get_speed, float, (uint64_t e), e) \
    X(native_animation2d_set_speed, void, (uint64_t e, float v), e, v) \
    X(native_camera2d_get_type, int32_t, (uint64_t e), e) \
    X(native_camera2d_set_type, void, (uint64_t e, int32_t v), e, v) \
    X(native_camera2d_get_zoom, float, (uint64_t e), e) \
    X(native_camera2d_set_zoom, void, (uint64_t e, float v), e, v) \
    X(native_camera2d_get_background, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_camera2d_set_background, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_boxcollider2d_get_offset, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_boxcollider2d_set_offset, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_boxcollider2d_get_size, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_boxcollider2d_set_size, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_boxcollider2d_get_density, float, (uint64_t e), e) \
    X(native_boxcollider2d_set_density, void, (uint64_t e, float v), e, v) \
    X(native_boxcollider2d_get_friction, float, (uint64_t e), e) \
    X(native_boxcollider2d_set_friction, void, (uint64_t e, float v), e, v) \
    X(native_boxcollider2d_get_restitution, float, (uint64_t e), e) \
    X(native_boxcollider2d_set_restitution, void, (uint64_t e, float v), e, v) \
    X(native_boxcollider2d_get_restitution_threshold, float, (uint64_t e), e) \
    X(native_boxcollider2d_set_restitution_threshold, void, (uint64_t e, float v), e, v) \
    X(native_mesh_renderer_get_value, int32_t, (uint64_t e), e) \
    X(native_mesh_renderer_set_value, void, (uint64_t e, int32_t v), e, v) \
    X(native_rigidbody2d_get_type, int32_t, (uint64_t e), e) \
    X(native_rigidbody2d_set_type, void, (uint64_t e, int32_t v), e, v) \
    X(native_rigidbody2d_get_gravity_scale, float, (uint64_t e), e) \
    X(native_rigidbody2d_set_gravity_scale, void, (uint64_t e, float v), e, v) \
    X(native_rigidbody2d_get_fixed_rotation, int, (uint64_t e), e) \
    X(native_rigidbody2d_set_fixed_rotation, void, (uint64_t e, int v), e, v) \
    X(native_rigidbody2d_set_linear_velocity, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_rigidbody2d_apply_force_to_center, void, (uint64_t e, float x, float y, int wake), e, x, y, wake) \
    X(native_rigidbody2d_apply_linear_impulse, void, (uint64_t e, float x, float y, int wake), e, x, y, wake) \
    X(native_sprite_renderer_get_texture_id, int32_t, (uint64_t e), e) \
    X(native_sprite_renderer_set_texture_id, void, (uint64_t e, int32_t v), e, v) \
    X(native_sprite_renderer_get_flip, int, (uint64_t e), e) \
    X(native_sprite_renderer_set_flip, void, (uint64_t e, int v), e, v) \
    X(native_sprite_renderer_get_pivot, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_sprite_renderer_set_pivot, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_sprite_renderer_get_depth, float, (uint64_t e), e) \
    X(native_sprite_renderer_set_depth, void, (uint64_t e, float v), e, v) \
    X(native_sprite_renderer_get_color, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_sprite_renderer_set_color, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_create_entity, uint64_t, (const char* name), name) \
    X(native_destroy_entity, void, (uint64_t e), e) \
    X(native_tilemap_set_data, void, (uint64_t e, int w, int h, int tw, int th), e, w, h, tw, th) \
    X(native_tilemap_add_tileset, void, (uint64_t e, const char* json), e, json) \
    X(native_tile_layer_set_data, void, (uint64_t e, const uint32_t* tiles, int len, int w, int h, const char* name, int vis, float opac, int ox, int oy), e, tiles, len, w, h, name, vis, opac, ox, oy) \
    X(native_entity_set_parent, void, (uint64_t child, uint64_t parent), child, parent) \
    X(native_tilemap_get_map_width, uint32_t, (uint64_t e), e) \
    X(native_tilemap_set_map_width, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tilemap_get_map_height, uint32_t, (uint64_t e), e) \
    X(native_tilemap_set_map_height, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tilemap_get_tile_width, uint32_t, (uint64_t e), e) \
    X(native_tilemap_set_tile_width, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tilemap_get_tile_height, uint32_t, (uint64_t e), e) \
    X(native_tilemap_set_tile_height, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tile_layer_get_name, const char*, (uint64_t e), e) \
    X(native_tile_layer_set_name, void, (uint64_t e, const char* v), e, v) \
    X(native_tile_layer_get_width, uint32_t, (uint64_t e), e) \
    X(native_tile_layer_set_width, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tile_layer_get_height, uint32_t, (uint64_t e), e) \
    X(native_tile_layer_set_height, void, (uint64_t e, uint32_t v), e, v) \
    X(native_tile_layer_get_visible, int, (uint64_t e), e) \
    X(native_tile_layer_set_visible, void, (uint64_t e, int v), e, v) \
    X(native_tile_layer_get_opacity, float, (uint64_t e), e) \
    X(native_tile_layer_set_opacity, void, (uint64_t e, float v), e, v) \
    X(native_tile_layer_get_offset_x, int32_t, (uint64_t e), e) \
    X(native_tile_layer_set_offset_x, void, (uint64_t e, int32_t v), e, v) \
    X(native_tile_layer_get_offset_y, int32_t, (uint64_t e), e) \
    X(native_tile_layer_set_offset_y, void, (uint64_t e, int32_t v), e, v) \
    X(native_get_asset_directory, const char*, (), )

#define BIND_FIELD(name, ret, sig, invoke) ret (*name) sig;
        struct NativeBindings { FOR_EACH_NATIVE_BINDING(BIND_FIELD) };
#undef BIND_FIELD

        static NativeBindings s_bindings = {};

        static void FillBindings() {
#define BIND_ASSIGN(name, ret, sig, invoke) s_bindings.name = name;
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
