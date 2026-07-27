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
#include "runtime/function/ui/ui_manager.h"
#include "runtime/function/ui/ui_button.h"
#include "runtime/function/ui/ui_label.h"
#include "runtime/function/ui/ui_image.h"
#include "runtime/function/ui/ui_panel.h"
#include "runtime/function/ui/ui_widget.h"
#include "runtime/function/ui/ui_interactive.h"

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

        static Scene* GetCurrentScene() { Scene* s = GetWorld()->getActiveScene(); DO_ASSERT(s); return s; }

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

        template <typename T>
        static T* FindUIElementById(uint32_t id) {
            auto* ui = GetUIManager();
            if (!ui || !ui->getRoot()) return nullptr;
            DynamicArray<UIElement*> queue;
            queue.push_back(ui->getRoot());
            while (!queue.empty()) {
                auto* cur = queue.front();
                queue.erase(queue.begin());
                if (static_cast<uint32_t>(cur->getId()) == id)
                    return dynamic_cast<T*>(cur);
                for (auto& c : cur->getChildren()) queue.push_back(c.get());
            }
            return nullptr;
        }

        // === UIElement ===
        static int native_ui_UIElement_visible_get(uint32_t id) { if (auto* e = FindUIElementById<UIElement>(id)) return e->isVisible(); return 0; }
        static void native_ui_UIElement_visible_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIElement>(id)) e->setVisible(v != 0); }
        static float native_ui_UIElement_depth_get(uint32_t id) { if (auto* e = FindUIElementById<UIElement>(id)) return e->getDepth(); return 0; }
        static void native_ui_UIElement_depth_set(uint32_t id, float v) { if (auto* e = FindUIElementById<UIElement>(id)) e->setDepth(v); }
        static void native_ui_UIElement_position_get(uint32_t id, float* x, float* y) { if (x) *x = 0; if (y) *y = 0; if (auto* e = FindUIElementById<UIElement>(id)) { auto p = e->getPosition(); if (x) *x = p.x; if (y) *y = p.y; } }
        static void native_ui_UIElement_position_set(uint32_t id, float x, float y) { if (auto* e = FindUIElementById<UIElement>(id)) e->setPosition({x, y}); }
        static void native_ui_UIElement_size_get(uint32_t id, float* x, float* y) { if (x) *x = 0; if (y) *y = 0; if (auto* e = FindUIElementById<UIElement>(id)) { auto s = e->getSize(); if (x) *x = s.x; if (y) *y = s.y; } }
        static void native_ui_UIElement_size_set(uint32_t id, float x, float y) { if (auto* e = FindUIElementById<UIElement>(id)) e->setSize({x, y}); }

        // === UIWidget ===
        static void native_ui_UIWidget_color_get(uint32_t id, float* r, float* g, float* b, float* a) { if (r) *r = 1; if (g) *g = 1; if (b) *b = 1; if (a) *a = 1; if (auto* e = FindUIElementById<UIWidget>(id)) { auto c = e->getColor(); if (r) *r = c.r; if (g) *g = c.g; if (b) *b = c.b; if (a) *a = c.a; } }
        static void native_ui_UIWidget_color_set(uint32_t id, float r, float g, float b, float a) { if (auto* e = FindUIElementById<UIWidget>(id)) e->setColor({r, g, b, a}); }
        static float native_ui_UIWidget_alpha_get(uint32_t id) { if (auto* e = FindUIElementById<UIWidget>(id)) return e->getAlpha(); return 1.0f; }
        static void native_ui_UIWidget_alpha_set(uint32_t id, float v) { if (auto* e = FindUIElementById<UIWidget>(id)) e->setAlpha(v); }

        // === UILabel ===
        static const char* native_ui_UILabel_text_get(uint32_t id) { DEF_STR_RET(ui_lbl_text); if (auto* e = FindUIElementById<UILabel>(id)) { _s_ui_lbl_text = e->getText(); return _s_ui_lbl_text.c_str(); } return ""; }
        static void native_ui_UILabel_text_set(uint32_t id, const char* v) { if (auto* e = FindUIElementById<UILabel>(id)) { if (v) e->setText(v); } }
        static int native_ui_UILabel_font_size_get(uint32_t id) { if (auto* e = FindUIElementById<UILabel>(id)) return e->getFontSize(); return 0; }
        static void native_ui_UILabel_font_size_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UILabel>(id)) e->setFontSize(v); }

        // === UIImage ===
        static int native_ui_UIImage_preserve_aspect_get(uint32_t id) { if (auto* e = FindUIElementById<UIImage>(id)) return e->isPreserveAspect(); return 0; }
        static void native_ui_UIImage_preserve_aspect_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIImage>(id)) e->setPreserveAspect(v != 0); }
        static int native_ui_UIImage_flip_h_get(uint32_t id) { if (auto* e = FindUIElementById<UIImage>(id)) return e->isFlippedH(); return 0; }
        static void native_ui_UIImage_flip_h_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIImage>(id)) e->setFlippedH(v != 0); }
        static int native_ui_UIImage_flip_v_get(uint32_t id) { if (auto* e = FindUIElementById<UIImage>(id)) return e->isFlippedV(); return 0; }
        static void native_ui_UIImage_flip_v_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIImage>(id)) e->setFlippedV(v != 0); }
        static void native_ui_UIImage_color_get(uint32_t id, float* r, float* g, float* b, float* a) { if (r) *r = 1; if (g) *g = 1; if (b) *b = 1; if (a) *a = 1; if (auto* e = FindUIElementById<UIImage>(id)) { auto c = e->getColor(); if (r) *r = c.r; if (g) *g = c.g; if (b) *b = c.b; if (a) *a = c.a; } }
        static void native_ui_UIImage_color_set(uint32_t id, float r, float g, float b, float a) { if (auto* e = FindUIElementById<UIImage>(id)) e->setColor({r, g, b, a}); }
        static float native_ui_UIImage_alpha_get(uint32_t id) { if (auto* e = FindUIElementById<UIImage>(id)) return e->getAlpha(); return 1.0f; }
        static void native_ui_UIImage_alpha_set(uint32_t id, float v) { if (auto* e = FindUIElementById<UIImage>(id)) e->setAlpha(v); }

        // === UIButton ===
        static const char* native_ui_UIButton_label_get(uint32_t id) { DEF_STR_RET(ui_btn_label); if (auto* e = FindUIElementById<UIButton>(id)) { _s_ui_btn_label = e->getLabel(); return _s_ui_btn_label.c_str(); } return ""; }
        static void native_ui_UIButton_label_set(uint32_t id, const char* v) { if (auto* e = FindUIElementById<UIButton>(id)) { if (v) e->setLabel(v); } }
        static int native_ui_UIButton_interactable_get(uint32_t id) { if (auto* e = FindUIElementById<UIButton>(id)) return e->isInteractable(); return 0; }
        static void native_ui_UIButton_interactable_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIButton>(id)) e->setInteractable(v != 0); }

        // === UIPanel ===
        static void native_ui_UIPanel_background_color_get(uint32_t id, float* r, float* g, float* b, float* a) { if (r) *r = 0; if (g) *g = 0; if (b) *b = 0; if (a) *a = 0; if (auto* e = FindUIElementById<UIPanel>(id)) { auto c = e->getBackgroundColor(); if (r) *r = c.r; if (g) *g = c.g; if (b) *b = c.b; if (a) *a = c.a; } }
        static void native_ui_UIPanel_background_color_set(uint32_t id, float r, float g, float b, float a) { if (auto* e = FindUIElementById<UIPanel>(id)) e->setBackgroundColor({r, g, b, a}); }
        static int native_ui_UIPanel_clip_children_get(uint32_t id) { if (auto* e = FindUIElementById<UIPanel>(id)) return e->isClipChildrenEnabled(); return 0; }
        static void native_ui_UIPanel_clip_children_set(uint32_t id, int v) { if (auto* e = FindUIElementById<UIPanel>(id)) e->setClipChildren(v != 0); }

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
            auto tex = Texture2D::Load(String(path));
            return tex ? (int)tex->getInstanceID() : 0;
        }

        static int native_world_load_scene(const char* name, int mode) {
            if (!name) return 0;
            auto* world = GetWorld();
            if (!world) return 0;
            world->loadScene(name, static_cast<LoadSceneMode>(mode));
            return 1;
        }

        static void native_world_unload_scene(const char* name) {
            if (!name) return;
            auto* world = GetWorld();
            if (!world) return;
            world->unloadScene(name);
        }

        static std::mutex s_async_load_mutex{};
        static std::atomic<int> s_async_load_next_token{1};
        static UnorderedMap<int, std::future<Scene*>> s_async_load_futures{};

        static int native_world_load_scene_async(const char* name, int mode) {
            if (!name) return -1;
            auto* world = GetWorld();
            if (!world) return -1;
            auto future = world->loadSceneAsync(name, static_cast<LoadSceneMode>(mode));
            const int token = s_async_load_next_token.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(s_async_load_mutex);
            s_async_load_futures[token] = std::move(future);
            return token;
        }

        static int native_world_is_load_complete(int token) {
            std::lock_guard<std::mutex> lock(s_async_load_mutex);
            auto it = s_async_load_futures.find(token);
            if (it == s_async_load_futures.end()) return 0;
            if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                it->second.get();
                s_async_load_futures.erase(it);
                return 1;
            }
            return 0;
        }

        static int native_ui_load_layout(const char* path) {
            if (!path || path[0] == '\0') return 0;
            auto* ui = GetUIManager();
            if (!ui) return 0;
            return ui->loadLayout(String(path)) ? 1 : 0;
        }

        static void native_ui_clear_all() {
            auto* ui = GetUIManager();
            if (ui) ui->clearAll();
        }

        static uint32_t native_ui_find_element(const char* id_str, const char* type_name) {
            if (!id_str || id_str[0] == '\0') return 0;
            auto* ui = GetUIManager();
            if (!ui) return 0;

            auto* elem = ui->findElementById(String(id_str));
            if (!elem) return 0;

            if (type_name && strlen(type_name) > 0) {
                String tn(type_name);
                Bool match = false;
                if (tn == "Button")        match = dynamic_cast<UIButton*>(elem) != nullptr;
                else if (tn == "Label")    match = dynamic_cast<UILabel*>(elem) != nullptr;
                else if (tn == "Image")    match = dynamic_cast<UIImage*>(elem) != nullptr;
                else if (tn == "Panel")    match = dynamic_cast<UIPanel*>(elem) != nullptr;
                else if (tn == "UIElement") match = true;
                else match = true; // unknown type, allow
                if (!match) return 0;
            }

            return static_cast<uint32_t>(std::hash<String>{}(id_str));
        }

        DEF_STR_RET(ui_prop_value);

        static void native_ui_element_set_property(uint32_t element_id, const char* prop_name, const char* value) {
            if (!prop_name || !value) return;
            auto* ui = GetUIManager();
            if (!ui) return;

            UIElement* elem = ui->getRoot();
            if (!elem) return;

            DynamicArray<UIElement*> queue;
            queue.push_back(elem);
            while (!queue.empty()) {
                auto* cur = queue.front();
                queue.erase(queue.begin());
                if (static_cast<uint32_t>(cur->getId()) == element_id) {
                    elem = cur;
                    break;
                }
                for (auto& c : cur->getChildren()) queue.push_back(c.get());
                elem = nullptr;
            }
            if (!elem) return;

            String pn(prop_name);
            String sv(value);

            if (pn == "visible")         elem->setVisible(sv == "true" || sv == "1");
            else if (pn == "depth")      elem->setDepth(static_cast<Float>(std::stod(sv)));
            else if (pn == "position") {
                auto comma = sv.find(',');
                if (comma != String::npos) {
                    Float x = static_cast<Float>(std::stod(sv.substr(0, comma)));
                    Float y = static_cast<Float>(std::stod(sv.substr(comma + 1)));
                    elem->setPosition({x, y});
                }
            }
            else if (pn == "size") {
                auto comma = sv.find(',');
                if (comma != String::npos) {
                    Float x = static_cast<Float>(std::stod(sv.substr(0, comma)));
                    Float y = static_cast<Float>(std::stod(sv.substr(comma + 1)));
                    elem->setSize({x, y});
                }
            }
            else if (pn == "color" || pn == "tint") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem)) {
                    Float r = 1, g = 1, b = 1, a = 1;
                    auto c1 = sv.find(','), c2 = (c1 != String::npos) ? sv.find(',', c1 + 1) : String::npos;
                    auto c3 = (c2 != String::npos) ? sv.find(',', c2 + 1) : String::npos;
                    if (c1 != String::npos && c2 != String::npos && c3 != String::npos) {
                        r = static_cast<Float>(std::stod(sv.substr(0, c1)));
                        g = static_cast<Float>(std::stod(sv.substr(c1 + 1, c2 - c1 - 1)));
                        b = static_cast<Float>(std::stod(sv.substr(c2 + 1, c3 - c2 - 1)));
                        a = static_cast<Float>(std::stod(sv.substr(c3 + 1)));
                    }
                    widget->setColor(Color(r, g, b, a));
                }
            }
            else if (pn == "alpha") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem))
                    widget->setAlpha(static_cast<Float>(std::stod(sv)));
            }
            else if (pn == "text") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    label->setText(sv);
            }
            else if (pn == "font_size") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    label->setFontSize(std::stoi(sv));
            }
            else if (pn == "preserve_aspect") {
                if (auto* image = dynamic_cast<UIImage*>(elem))
                    image->setPreserveAspect(sv == "true" || sv == "1");
            }
            else if (pn == "flip_h") {
                if (auto* image = dynamic_cast<UIImage*>(elem))
                    image->setFlippedH(sv == "true" || sv == "1");
            }
            else if (pn == "flip_v") {
                if (auto* image = dynamic_cast<UIImage*>(elem))
                    image->setFlippedV(sv == "true" || sv == "1");
            }
            else if (pn == "flip") {
                if (auto* image = dynamic_cast<UIImage*>(elem)) {
                    auto comma = sv.find(',');
                    Bool fh = false, fv = false;
                    if (comma != String::npos) {
                        auto h = sv.substr(0, comma);
                        auto v = sv.substr(comma + 1);
                        fh = (h == "true" || h == "1");
                        fv = (v == "true" || v == "1");
                    }
                    image->setFlipped(fh, fv);
                }
            }
            else if (pn == "label") {
                if (auto* button = dynamic_cast<UIButton*>(elem))
                    button->setLabel(sv);
            }
            else if (pn == "interactable") {
                if (auto* interactive = dynamic_cast<UIInteractive*>(elem))
                    interactive->setInteractable(sv == "true" || sv == "1");
            }
            else if (pn == "background_color") {
                if (auto* panel = dynamic_cast<UIPanel*>(elem)) {
                    Float r = 0, g = 0, b = 0, a = 1;
                    auto c1 = sv.find(','), c2 = (c1 != String::npos) ? sv.find(',', c1 + 1) : String::npos;
                    auto c3 = (c2 != String::npos) ? sv.find(',', c2 + 1) : String::npos;
                    if (c1 != String::npos && c2 != String::npos && c3 != String::npos) {
                        r = static_cast<Float>(std::stod(sv.substr(0, c1)));
                        g = static_cast<Float>(std::stod(sv.substr(c1 + 1, c2 - c1 - 1)));
                        b = static_cast<Float>(std::stod(sv.substr(c2 + 1, c3 - c2 - 1)));
                        a = static_cast<Float>(std::stod(sv.substr(c3 + 1)));
                    }
                    panel->setBackgroundColor(Color(r, g, b, a));
                }
            }
            else if (pn == "clip_children") {
                if (auto* panel = dynamic_cast<UIPanel*>(elem))
                    panel->setClipChildren(sv == "true" || sv == "1");
            }
        }

        static const char* native_ui_element_get_property(uint32_t element_id, const char* prop_name) {
            _s_ui_prop_value.clear();
            if (!prop_name) return "";
            auto* ui = GetUIManager();
            if (!ui || !ui->getRoot()) return "";

            UIElement* elem = nullptr;
            DynamicArray<UIElement*> queue;
            queue.push_back(ui->getRoot());
            while (!queue.empty()) {
                auto* cur = queue.front();
                queue.erase(queue.begin());
                if (static_cast<uint32_t>(cur->getId()) == element_id) {
                    elem = cur;
                    break;
                }
                for (auto& c : cur->getChildren()) queue.push_back(c.get());
            }
            if (!elem) return "";

            String pn(prop_name);

            if (pn == "visible")     _s_ui_prop_value = elem->isVisible() ? "true" : "false";
            else if (pn == "depth")  _s_ui_prop_value = std::to_string(elem->getDepth());
            else if (pn == "position") {
                auto p = elem->getPosition();
                _s_ui_prop_value = std::to_string(p.x) + "," + std::to_string(p.y);
            }
            else if (pn == "size") {
                auto s = elem->getSize();
                _s_ui_prop_value = std::to_string(s.x) + "," + std::to_string(s.y);
            }
            else if (pn == "color" || pn == "tint") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem)) {
                    auto c = widget->getColor();
                    _s_ui_prop_value = std::to_string(c.r) + "," + std::to_string(c.g) + ","
                        + std::to_string(c.b) + "," + std::to_string(c.a);
                }
            }
            else if (pn == "alpha") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem))
                    _s_ui_prop_value = std::to_string(widget->getAlpha());
            }
            else if (pn == "text") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    _s_ui_prop_value = label->getText();
            }
            else if (pn == "font_size") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    _s_ui_prop_value = std::to_string(label->getFontSize());
            }
            else if (pn == "label") {
                if (auto* button = dynamic_cast<UIButton*>(elem))
                    _s_ui_prop_value = button->getLabel();
            }
            else if (pn == "interactable") {
                if (auto* interactive = dynamic_cast<UIInteractive*>(elem))
                    _s_ui_prop_value = interactive->isInteractable() ? "true" : "false";
            }
            else if (pn == "preserve_aspect") {
                if (auto* image = dynamic_cast<UIImage*>(elem))
                    _s_ui_prop_value = image->isPreserveAspect() ? "true" : "false";
            }
            else if (pn == "background_color") {
                if (auto* panel = dynamic_cast<UIPanel*>(elem)) {
                    auto c = panel->getBackgroundColor();
                    _s_ui_prop_value = std::to_string(c.r) + "," + std::to_string(c.g) + ","
                        + std::to_string(c.b) + "," + std::to_string(c.a);
                }
            }
            else if (pn == "clip_children") {
                if (auto* panel = dynamic_cast<UIPanel*>(elem))
                    _s_ui_prop_value = panel->isClipChildrenEnabled() ? "true" : "false";
            }

            return _s_ui_prop_value.c_str();
        }

        static uint32_t native_ui_create_element(const char* type, const char* id, const char* parent_id) {
            if (!type || !id) return 0;
            auto* ui = GetUIManager();
            if (!ui) return 0;
            auto* elem = ui->createElement(String(type), String(id),
                parent_id ? String(parent_id) : String());
            if (!elem) return 0;
            return static_cast<uint32_t>(elem->getId());
        }

        static int native_ui_poll_event(uint32_t element_id, const char* event_name) {
            if (!event_name) return 0;
            auto* ui = GetUIManager();
            if (!ui || !ui->getRoot()) return 0;

            UIElement* elem = nullptr;
            DynamicArray<UIElement*> queue;
            queue.push_back(ui->getRoot());
            while (!queue.empty()) {
                auto* cur = queue.front();
                queue.erase(queue.begin());
                if (static_cast<uint32_t>(cur->getId()) == element_id) {
                    elem = cur;
                    break;
                }
                for (auto& c : cur->getChildren()) queue.push_back(c.get());
            }
            if (!elem) return 0;

            auto* interactive = dynamic_cast<UIInteractive*>(elem);
            if (!interactive) return 0;

            String en(event_name);
            if (en == "clicked")          return interactive->pollClicked() ? 1 : 0;
            if (en == "entered")          return interactive->pollEntered() ? 1 : 0;
            if (en == "exited")           return interactive->pollExited() ? 1 : 0;
            if (en == "is_hovered")       return interactive->isHovered() ? 1 : 0;
            if (en == "is_pressed")       return interactive->isPressed() ? 1 : 0;
            return 0;
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
    X(native_texture_load, int, (const char* path), path) \
    X(native_world_load_scene, int, (const char* name, int mode), name, mode) \
    X(native_world_unload_scene, void, (const char* name), name) \
    X(native_world_load_scene_async, int, (const char* name, int mode), name, mode) \
    X(native_world_is_load_complete, int, (int token), token) \
    X(native_ui_load_layout, int, (const char* path), path) \
    X(native_ui_clear_all, void, (), ) \
    X(native_ui_find_element, uint32_t, (const char* id_str, const char* type_name), id_str, type_name) \
    X(native_ui_element_set_property, void, (uint32_t element_id, const char* prop_name, const char* value), element_id, prop_name, value) \
    X(native_ui_element_get_property, const char*, (uint32_t element_id, const char* prop_name), element_id, prop_name) \
    X(native_ui_create_element, uint32_t, (const char* type, const char* id, const char* parent_id), type, id, parent_id) \
    X(native_ui_poll_event, int, (uint32_t element_id, const char* event_name), element_id, event_name) \
    X(native_ui_UIElement_visible_get, int, (uint32_t id), id) \
    X(native_ui_UIElement_visible_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIElement_depth_get, float, (uint32_t id), id) \
    X(native_ui_UIElement_depth_set, void, (uint32_t id, float v), id, v) \
    X(native_ui_UIElement_position_get, void, (uint32_t id, float* x, float* y), id, x, y) \
    X(native_ui_UIElement_position_set, void, (uint32_t id, float x, float y), id, x, y) \
    X(native_ui_UIElement_size_get, void, (uint32_t id, float* x, float* y), id, x, y) \
    X(native_ui_UIElement_size_set, void, (uint32_t id, float x, float y), id, x, y) \
    X(native_ui_UIWidget_color_get, void, (uint32_t id, float* r, float* g, float* b, float* a), id, r, g, b, a) \
    X(native_ui_UIWidget_color_set, void, (uint32_t id, float r, float g, float b, float a), id, r, g, b, a) \
    X(native_ui_UIWidget_alpha_get, float, (uint32_t id), id) \
    X(native_ui_UIWidget_alpha_set, void, (uint32_t id, float v), id, v) \
    X(native_ui_UILabel_text_get, const char*, (uint32_t id), id) \
    X(native_ui_UILabel_text_set, void, (uint32_t id, const char* v), id, v) \
    X(native_ui_UILabel_font_size_get, int, (uint32_t id), id) \
    X(native_ui_UILabel_font_size_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIImage_preserve_aspect_get, int, (uint32_t id), id) \
    X(native_ui_UIImage_preserve_aspect_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIImage_flip_h_get, int, (uint32_t id), id) \
    X(native_ui_UIImage_flip_h_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIImage_flip_v_get, int, (uint32_t id), id) \
    X(native_ui_UIImage_flip_v_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIImage_color_get, void, (uint32_t id, float* r, float* g, float* b, float* a), id, r, g, b, a) \
    X(native_ui_UIImage_color_set, void, (uint32_t id, float r, float g, float b, float a), id, r, g, b, a) \
    X(native_ui_UIImage_alpha_get, float, (uint32_t id), id) \
    X(native_ui_UIImage_alpha_set, void, (uint32_t id, float v), id, v) \
    X(native_ui_UIButton_label_get, const char*, (uint32_t id), id) \
    X(native_ui_UIButton_label_set, void, (uint32_t id, const char* v), id, v) \
    X(native_ui_UIButton_interactable_get, int, (uint32_t id), id) \
    X(native_ui_UIButton_interactable_set, void, (uint32_t id, int v), id, v) \
    X(native_ui_UIPanel_background_color_get, void, (uint32_t id, float* r, float* g, float* b, float* a), id, r, g, b, a) \
    X(native_ui_UIPanel_background_color_set, void, (uint32_t id, float r, float g, float b, float a), id, r, g, b, a) \
    X(native_ui_UIPanel_clip_children_get, int, (uint32_t id), id) \
    X(native_ui_UIPanel_clip_children_set, void, (uint32_t id, int v), id, v)

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
