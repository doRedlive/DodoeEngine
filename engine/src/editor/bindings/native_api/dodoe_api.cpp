// do@Redlive
// C API — fully wired to Dodoe engine.
// Model B: Avalonia drives the frame loop, C++ is a shared library.

#include "dodoe_api.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/thread/task_scheduler.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/framework/texture.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/window/window.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/asset_database.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include <cstring>
#include <algorithm>
#include <string>

using namespace dodoe;

// ============================================================
// Internal helpers
// ============================================================

namespace {

inline SystemContext* SC(void* ctx)      { return static_cast<SystemContext*>(ctx); }

inline World* W(void* ctx) {
    auto* sc = SC(ctx);
    return sc ? sc->world.get() : nullptr;
}

inline Scene* currentScene(void* ctx) {
    auto* world = W(ctx);
    return world ? world->getCurrentScene() : nullptr;
}

inline TextureManager* TM(void* ctx) {
    auto* sc = SC(ctx);
    if (!sc || !sc->render_system) return nullptr;
    return Renderer::GetTextureManager();
}

inline entt::entity toEntt(DodoeHandle h) { return static_cast<entt::entity>(static_cast<UInt32>(h)); }
inline DodoeHandle fromEntt(entt::entity e) { return static_cast<DodoeHandle>(static_cast<UInt32>(e)); }

inline Entity makeEntity(void* ctx, entt::entity h) {
    auto* scene = currentScene(ctx);
    if (scene && h != entt::null) return Entity(scene, h);
    return Entity();
}

// ============================================================
// Component type registry
// ============================================================

enum class CompType : int {
    Transform, SpriteRenderer, Rigidbody2D, BoxCollider2D,
    Animation2D, Camera2D, MeshRenderer, Tilemap, TileLayer, Hierarchy,
    Count
};

CompType resolveCompType(const char* name) {
    if (!name) return CompType::Transform;
    if (std::strcmp(name, "TransformComponent") == 0)        return CompType::Transform;
    if (std::strcmp(name, "SpriteRendererComponent") == 0)   return CompType::SpriteRenderer;
    if (std::strcmp(name, "Rigidbody2dComponent") == 0)      return CompType::Rigidbody2D;
    if (std::strcmp(name, "BoxCollider2dComponent") == 0)    return CompType::BoxCollider2D;
    if (std::strcmp(name, "Animation2dComponent") == 0)      return CompType::Animation2D;
    if (std::strcmp(name, "Camera2dComponent") == 0)         return CompType::Camera2D;
    if (std::strcmp(name, "MeshRendererComponent") == 0)     return CompType::MeshRenderer;
    if (std::strcmp(name, "TilemapComponent") == 0)          return CompType::Tilemap;
    if (std::strcmp(name, "TileLayerComponent") == 0)        return CompType::TileLayer;
    if (std::strcmp(name, "HierarchyComponent") == 0)        return CompType::Hierarchy;
    return CompType::Transform;
}

const char* compTypeName(CompType t) {
    switch (t) {
        case CompType::Transform:       return "TransformComponent";
        case CompType::SpriteRenderer:  return "SpriteRendererComponent";
        case CompType::Rigidbody2D:     return "Rigidbody2dComponent";
        case CompType::BoxCollider2D:   return "BoxCollider2dComponent";
        case CompType::Animation2D:     return "Animation2dComponent";
        case CompType::Camera2D:        return "Camera2dComponent";
        case CompType::MeshRenderer:    return "MeshRendererComponent";
        case CompType::Tilemap:         return "TilemapComponent";
        case CompType::TileLayer:       return "TileLayerComponent";
        case CompType::Hierarchy:       return "HierarchyComponent";
        default:                        return "";
    }
}

#define HAS_COMP_BODY(C, CC)  case C: return entity.hasComponent<CC>()
bool entityHasComp(Entity& entity, CompType type) {
    switch (type) {
        HAS_COMP_BODY(CompType::Transform,       TransformComponent);
        HAS_COMP_BODY(CompType::SpriteRenderer,  SpriteRendererComponent);
        HAS_COMP_BODY(CompType::Rigidbody2D,     Rigidbody2dComponent);
        HAS_COMP_BODY(CompType::BoxCollider2D,   BoxCollider2dComponent);
        HAS_COMP_BODY(CompType::Animation2D,     Animation2dComponent);
        HAS_COMP_BODY(CompType::Camera2D,        Camera2dComponent);
        HAS_COMP_BODY(CompType::MeshRenderer,    MeshRendererComponent);
        HAS_COMP_BODY(CompType::Tilemap,         TilemapComponent);
        HAS_COMP_BODY(CompType::TileLayer,       TileLayerComponent);
        HAS_COMP_BODY(CompType::Hierarchy,       HierarchyComponent);
        default: return false;
    }
}

#define ADD_COMP_BODY(C, CC) case C: if (!entity.hasComponent<CC>()) entity.addComponent<CC>(); break
void entityAddComp(Entity& entity, CompType type) {
    switch (type) {
        ADD_COMP_BODY(CompType::Transform,       TransformComponent);
        ADD_COMP_BODY(CompType::SpriteRenderer,  SpriteRendererComponent);
        ADD_COMP_BODY(CompType::Rigidbody2D,     Rigidbody2dComponent);
        ADD_COMP_BODY(CompType::BoxCollider2D,   BoxCollider2dComponent);
        ADD_COMP_BODY(CompType::Animation2D,     Animation2dComponent);
        ADD_COMP_BODY(CompType::Camera2D,        Camera2dComponent);
        ADD_COMP_BODY(CompType::MeshRenderer,    MeshRendererComponent);
        ADD_COMP_BODY(CompType::Tilemap,         TilemapComponent);
        ADD_COMP_BODY(CompType::TileLayer,       TileLayerComponent);
        ADD_COMP_BODY(CompType::Hierarchy,       HierarchyComponent);
        default: break;
    }
}

#define REMOVE_COMP_BODY(C, CC) case C: if (entity.hasComponent<CC>()) entity.removeComponent<CC>(); break
void entityRemoveComp(Entity& entity, CompType type) {
    switch (type) {
        REMOVE_COMP_BODY(CompType::Transform,       TransformComponent);
        REMOVE_COMP_BODY(CompType::SpriteRenderer,  SpriteRendererComponent);
        REMOVE_COMP_BODY(CompType::Rigidbody2D,     Rigidbody2dComponent);
        REMOVE_COMP_BODY(CompType::BoxCollider2D,   BoxCollider2dComponent);
        REMOVE_COMP_BODY(CompType::Animation2D,     Animation2dComponent);
        REMOVE_COMP_BODY(CompType::Camera2D,        Camera2dComponent);
        REMOVE_COMP_BODY(CompType::MeshRenderer,    MeshRendererComponent);
        REMOVE_COMP_BODY(CompType::Tilemap,         TilemapComponent);
        REMOVE_COMP_BODY(CompType::TileLayer,       TileLayerComponent);
        REMOVE_COMP_BODY(CompType::Hierarchy,       HierarchyComponent);
        default: break;
    }
}
#undef HAS_COMP_BODY
#undef ADD_COMP_BODY
#undef REMOVE_COMP_BODY

// thread-local buffer for const char* returns
thread_local static std::string tl_buffer;

// editor selected entity
static DodoeHandle s_selected_entity = 0;

// engine singleton — Scope ensures proper destruction
static Scope<Application> s_app{nullptr};

} // anonymous namespace

// ============================================================
// Group 1: Context lifecycle (the real one)
// ============================================================

DODOE_API void* dodoe_context_create(void) {
    if (s_app) return static_cast<void*>(&s_app->context());

    ApplicationSpecification spec;
    spec.name              = "Cakery";
    spec.width             = 1600;
    spec.height            = 900;
    spec.window_resizeable = true;
    spec.custom_titlebar   = false;
    spec.render_settings.api      = RenderBackendApiType::Vulkan;
    spec.render_settings.pipeline = RenderingPipelineType::Only2D;

    s_app = create_scope<Application>(spec);
    auto* ctx = &s_app->context();

    TaskScheduler::Self();
    EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(s_app.get());
    ctx->startRuntime();
    ctx->layer_stack.attach();

    return static_cast<void*>(ctx);
}

DODOE_API void dodoe_context_destroy(void* ctx) {
    if (!s_app) return;
    auto* sc = SC(ctx);

    sc->layer_stack.detach();
    sc->finalizeRuntime();
    EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(s_app.get());
    s_app.reset();
}

DODOE_API void dodoe_context_tick(void* ctx) {
    auto* sc = SC(ctx);
    if (!sc || !s_app) return;

    EventSystem::Poll();
    sc->tickOneFrame();
    EventSystem::Handle();
}

DODOE_API const char* dodoe_get_version(void) {
    return "Dodoe 1.0.0";
}

DODOE_API void* dodoe_get_native_window(void* ctx) {
    auto* sc = SC(ctx);
    if (!sc || !sc->getWindowManager()) return nullptr;
    auto* wm = sc->getWindowManager();
    auto* win = wm->getWindow();
    if (!win) return nullptr;

    auto* glfw = win->getNativeWindow();
    if (!glfw) return nullptr;

#ifdef DO_PLATFORM_WINDOWS
    return static_cast<void*>(glfwGetWin32Window(glfw));
#else
    return nullptr;
#endif
}

DODOE_API void dodoe_show_window(void* ctx, bool show) {
    auto* sc = SC(ctx);
    if (!sc || !sc->getWindowManager()) return;
    auto* win = sc->getWindowManager()->getWindow();
    if (!win || !win->getNativeWindow()) return;

    if (show)
        glfwShowWindow(win->getNativeWindow());
    else
        glfwHideWindow(win->getNativeWindow());
}

// ============================================================
// Group 2: ECS Entity operations
// ============================================================

DODOE_API DodoeHandle dodoe_entity_create(void* ctx) {
    auto* scene = currentScene(ctx);
    if (!scene) return 0;
    Entity e = scene->createEntity("New Entity");
    return e.valid() ? fromEntt(e.handle()) : 0;
}

DODOE_API void dodoe_entity_destroy(void* ctx, DodoeHandle handle) {
    auto* scene = currentScene(ctx);
    if (!scene || handle == 0) return;
    Entity e = makeEntity(ctx, toEntt(handle));
    if (e.valid()) scene->destroyEntity(e);
}

DODOE_API void dodoe_entity_set_name(void* ctx, DodoeHandle handle, const char* name) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (e.valid() && name) e.getComponent<IDComponent>().setName(name);
}

DODOE_API const char* dodoe_entity_get_name(void* ctx, DodoeHandle handle) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return "";
    tl_buffer = e.name();
    return tl_buffer.c_str();
}

DODOE_API void dodoe_entity_set_position(void* ctx, DodoeHandle handle, DodoeVec2 pos) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return;
    e.getComponent<TransformComponent>().setPosition(Vector3f(pos.x, pos.y, 0.0f));
}

DODOE_API DodoeVec2 dodoe_entity_get_position(void* ctx, DodoeHandle handle) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return {0, 0};
    const auto& p = e.getComponent<TransformComponent>().getPosition();
    return {p.x, p.y};
}

DODOE_API void dodoe_entity_set_scale(void* ctx, DodoeHandle handle, DodoeVec2 scale) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return;
    e.getComponent<TransformComponent>().setScale(Vector3f(scale.x, scale.y, 1.0f));
}

DODOE_API DodoeVec2 dodoe_entity_get_scale(void* ctx, DodoeHandle handle) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return {1, 1};
    const auto& s = e.getComponent<TransformComponent>().getScale();
    return {s.x, s.y};
}

DODOE_API void dodoe_entity_set_rotation(void* ctx, DodoeHandle handle, float rad) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return;
    e.getComponent<TransformComponent>().setRotation(Vector3f(0, 0, rad));
}

DODOE_API float dodoe_entity_get_rotation(void* ctx, DodoeHandle handle) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return 0;
    return e.getComponent<TransformComponent>().getRotation().z;
}

DODOE_API int dodoe_entity_get_child_count(void* ctx, DodoeHandle parent) {
    Entity e = makeEntity(ctx, toEntt(parent));
    if (!e.valid() || !e.hasComponent<HierarchyComponent>()) return 0;
    return static_cast<int>(e.getComponent<HierarchyComponent>().children.size());
}

DODOE_API DodoeHandle dodoe_entity_get_child_at(void* ctx, DodoeHandle parent, int index) {
    Entity e = makeEntity(ctx, toEntt(parent));
    if (!e.valid() || !e.hasComponent<HierarchyComponent>()) return 0;
    auto& hc = e.getComponent<HierarchyComponent>();
    if (index >= 0 && static_cast<size_t>(index) < hc.children.size())
        return fromEntt(hc.children[index].handle());
    return 0;
}

DODOE_API void dodoe_entity_set_parent(void* ctx, DodoeHandle child, DodoeHandle parent) {
    Entity child_e  = makeEntity(ctx, toEntt(child));
    Entity parent_e = makeEntity(ctx, toEntt(parent));
    if (!child_e.valid()) return;

    if (!child_e.hasComponent<HierarchyComponent>())  child_e.addComponent<HierarchyComponent>();
    if (parent_e.valid() && !parent_e.hasComponent<HierarchyComponent>())
        parent_e.addComponent<HierarchyComponent>();

    auto& chc = child_e.getComponent<HierarchyComponent>();
    if (chc.parent.valid()) {
        auto& old = chc.parent.getComponent<HierarchyComponent>();
        auto& sibs = old.children;
        sibs.erase(std::remove_if(sibs.begin(), sibs.end(),
            [&](const Entity& s) { return s.handle() == child_e.handle(); }), sibs.end());
    }
    if (parent_e.valid()) {
        auto& phc = parent_e.getComponent<HierarchyComponent>();
        chc.parent = parent_e;
        chc.parent_uuid = parent_e.uuid();
        phc.children.push_back(child_e);
        phc.child_count = static_cast<int>(phc.children.size());
    } else {
        chc.parent = Entity();
        chc.parent_uuid = Uuid();
    }
    chc.dirty = true;
}

DODOE_API int dodoe_entity_has_component(void* ctx, DodoeHandle handle, const char* tn) {
    Entity e = makeEntity(ctx, toEntt(handle));
    return (e.valid() && entityHasComp(e, resolveCompType(tn))) ? 1 : 0;
}

DODOE_API void dodoe_entity_add_component(void* ctx, DodoeHandle handle, const char* tn) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (e.valid()) entityAddComp(e, resolveCompType(tn));
}

DODOE_API void dodoe_entity_remove_component(void* ctx, DodoeHandle handle, const char* tn) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (e.valid()) entityRemoveComp(e, resolveCompType(tn));
}

DODOE_API int dodoe_entity_get_component_count(void* ctx, DodoeHandle handle) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return 0;
    int count = 0;
    for (int i = 0; i < static_cast<int>(CompType::Count); ++i)
        if (entityHasComp(e, static_cast<CompType>(i))) ++count;
    return count;
}

DODOE_API const char* dodoe_entity_get_component_type(void* ctx, DodoeHandle handle, int idx) {
    Entity e = makeEntity(ctx, toEntt(handle));
    if (!e.valid()) return "";
    int seen = 0;
    for (int i = 0; i < static_cast<int>(CompType::Count); ++i) {
        auto ct = static_cast<CompType>(i);
        if (entityHasComp(e, ct)) { if (seen == idx) return compTypeName(ct); ++seen; }
    }
    return "";
}

// ============================================================
// Group 3: Texture operations
// ============================================================

DODOE_API DodoeTextureId dodoe_texture_load(void* ctx, const char* path) {
    auto* tm = TM(ctx);
    if (!tm || !path || !path[0]) return 0;
    Ref<Texture> t = tm->loadTexture(path);
    return t ? static_cast<DodoeTextureId>(t->getInstanceID()) : 0;
}

DODOE_API DodoeTextureInfo dodoe_texture_get_info(void* ctx, DodoeTextureId id) {
    DodoeTextureInfo info = {};
    auto* tm = TM(ctx);
    if (!tm || id == 0) return info;
    Ref<Texture> t = tm->findTexture(static_cast<InstanceID>(id));
    if (!t) return info;
    info.id = id;  tl_buffer = t->getPath();
    info.path = tl_buffer.c_str();  info.width = t->getWidth();  info.height = t->getHeight();
    return info;
}

DODOE_API int dodoe_texture_get_loaded_count(void* ctx) {
    (void)ctx;
    return 0;
}

// ============================================================
// Group 4: Asset system
// ============================================================

DODOE_API int dodoe_asset_get_count(void* ctx, const char* asset_type) {
    (void)ctx;
    auto& rm = ResourceManager::Self();
    auto* am = rm.getAssetManager();
    if (!am || !asset_type) return 0;
    AssetType type = Asset::assetTypeFromString(asset_type);
    return (type == AssetType::Unknown) ? 0 : static_cast<int>(am->getAssetCountOfType(type));
}

DODOE_API DodoeAssetRef dodoe_asset_get_at(void* ctx, const char* asset_type, int index) {
    DodoeAssetRef ref = {};
    (void)ctx;
    auto& rm = ResourceManager::Self();
    auto* am = rm.getAssetManager();
    if (!am || !asset_type || index < 0) return ref;
    AssetType type = Asset::assetTypeFromString(asset_type);
    if (type == AssetType::Unknown) return ref;
    auto* db = am->getDatabase();
    if (!db) return ref;
    auto file_ids = db->getAssetsOfType(type);
    if (index >= static_cast<int>(file_ids.size())) return ref;
    const FileID& fid = file_ids[index];
    AssetMetaData meta = db->getMetaData(fid);
    ref.id = static_cast<DodoeAssetId>(fid.getID());
    ref.path_id = static_cast<DodoeTextureId>(fid.getID());
    tl_buffer = meta.source_path;  ref.path = tl_buffer.c_str();  ref.type = asset_type;
    return ref;
}

// ============================================================
// Group 5: Editor selection & viewport
// ============================================================

DODOE_API void dodoe_world_set_state(void* ctx, int state) {
    auto* world = W(ctx);
    if (!world) return;
    switch (state) {
        case 0: world->setState(WorldState::Simulation); break;
        case 1: world->setState(WorldState::Runtime);    break;
        case 2: world->setState(WorldState::Pause);      break;
    }
}

DODOE_API void dodoe_editor_select_entity(void* ctx, DodoeHandle handle) {
    (void)ctx;
    s_selected_entity = handle;
}

DODOE_API DodoeHandle dodoe_editor_get_selected_entity(void* ctx) {
    (void)ctx;
    return s_selected_entity;
}

DODOE_API void dodoe_viewport_attach(void* ctx, void* native_handle, int width, int height) {
    (void)ctx; (void)native_handle; (void)width; (void)height;
}

DODOE_API void dodoe_viewport_resize(void* ctx, int width, int height) {
    (void)ctx; (void)width; (void)height;
}

DODOE_API void dodoe_viewport_detach(void* ctx) {
    (void)ctx;
}
