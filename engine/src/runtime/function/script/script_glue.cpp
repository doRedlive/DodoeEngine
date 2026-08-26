#include "script_glue.h"

#include <cstring>

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/input/input.h"
#include "runtime/function/input/input_serialization.h"
#include "runtime/function/script/script_engine.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/physics/physics_system.h"
#include "runtime/function/world/components.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/entity_requests.h"
#include "runtime/function/world/systems/physics2d_system.h"
#include "runtime/function/world/systems/physics3d_system.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/core/project/project.h"
#include "runtime/function/render/pixel2d/sprite_manager.h"
#include "runtime/function/render/material/material.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/animation/animation.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/core/utils/json.h"
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

        static std::unordered_map<String, std::function<int(Entity)>> s_EntityHasComponentFuncUmap;
        static std::unordered_map<String, std::function<void(Entity)>> s_EntityAddComponentFuncUmap;
        static std::unordered_map<String, std::function<void(Entity)>> s_EntityRemoveComponentFuncUmap;

        template<typename... T> struct ComponentGroup { };

        using NativeComponents = ComponentGroup<
            IDComponent, TagComponent, TransformComponent,
            AnimatorComponent, CameraComponent, BoxCollider2dComponent,
            MeshRendererComponent, Rigidbody2dComponent, SpriteRendererComponent,
            AudioSourceComponent, AudioListenerComponent,
            TilemapComponent, TileLayerComponent
        >;

        template<typename TC>
        static String ResolveManagedComponentName() {
            std::string_view tn = typeid(TC).name();
            size_t p = tn.find_last_of(':');
            std::string_view n = (p == std::string_view::npos) ? tn : tn.substr(p + 1);
            if (n.starts_with("struct ")) n.remove_prefix(7);
            else if (n.starts_with("class ")) n.remove_prefix(6);
            return String(n);
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
                Entity e = s->tryGetEntityByUUID(UUID(uuid));
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

#define DEF_STR_RET(id) thread_local static String _s_##id

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

        static int native_input_register_action_map(const char* map_name, int priority) {
            return Input::RegisterActionMap(map_name ? String(map_name) : String{}, priority) ? 1 : 0;
        }
        static int native_input_set_action_map_enabled(const char* map_name, int enabled) {
            return Input::SetActionMapEnabled(map_name ? StringView(map_name) : StringView{}, enabled != 0) ? 1 : 0;
        }
        static int native_input_set_action_map_consume(const char* map_name, int consume) {
            return Input::SetActionMapConsume(map_name ? StringView(map_name) : StringView{}, consume != 0) ? 1 : 0;
        }
        static int native_input_push_context(const char* map_name) {
            return Input::PushInputContext(map_name ? StringView(map_name) : StringView{}) ? 1 : 0;
        }
        static int native_input_pop_context(const char* map_name) {
            return Input::PopInputContext(map_name ? StringView(map_name) : StringView{}) ? 1 : 0;
        }
        static int native_input_register_action(const char* map_name, const char* action_name, int value_type) {
            return Input::RegisterAction(map_name ? StringView(map_name) : StringView{},
                                          action_name ? String(action_name) : String{},
                                          static_cast<InputActionValueType>(value_type)) ? 1 : 0;
        }
        static int native_input_bind_key(const char* map_name, const char* action_name, int key, float scale) {
            return Input::BindKey(map_name ? StringView(map_name) : StringView{},
                                  action_name ? StringView(action_name) : StringView{},
                                  static_cast<KeyCode>(key), scale) ? 1 : 0;
        }
        static int native_input_unregister_action_map(const char* map_name) {
            return Input::UnregisterActionMap(map_name ? StringView(map_name) : StringView{}) ? 1 : 0;
        }
        static int native_input_bind_key2d(const char* map_name, const char* action_name, int key, float x, float y) {
            return Input::BindKey2D(map_name ? StringView(map_name) : StringView{},
                                    action_name ? StringView(action_name) : StringView{},
                                    static_cast<KeyCode>(key), Vector2f(x, y)) ? 1 : 0;
        }
        static int native_input_bind_mouse_button(const char* map_name, const char* action_name, int button, float scale) {
            return Input::BindMouseButton(map_name ? StringView(map_name) : StringView{},
                                          action_name ? StringView(action_name) : StringView{},
                                          static_cast<MouseCode>(button), scale) ? 1 : 0;
        }
        static int native_input_bind_mouse_delta(const char* map_name, const char* action_name, float scale) {
            return Input::BindMouseDelta(map_name ? StringView(map_name) : StringView{},
                                         action_name ? StringView(action_name) : StringView{}, scale) ? 1 : 0;
        }
        static int native_input_bind_mouse_wheel(const char* map_name, const char* action_name, float scale) {
            return Input::BindMouseWheel(map_name ? StringView(map_name) : StringView{},
                                         action_name ? StringView(action_name) : StringView{}, scale) ? 1 : 0;
        }
        static void native_input_get_mouse_position(float* x, float* y) {
            const Vector2f v = Input::GetMousePosition();
            if (x) *x = v.x;
            if (y) *y = v.y;
        }
        static void native_input_get_mouse_delta(float* x, float* y) {
            const Vector2f v = Input::GetMouseDelta();
            if (x) *x = v.x;
            if (y) *y = v.y;
        }
        static void native_input_get_mouse_wheel(float* x, float* y) {
            const Vector2f v = Input::GetMouseWheel();
            if (x) *x = v.x;
            if (y) *y = v.y;
        }
        static int native_input_is_action_down(const char* action_name) {
            return Input::IsActionDown(action_name ? StringView(action_name) : StringView{}) ? 1 : 0;
        }
        static int native_input_was_action_pressed(const char* action_name) {
            return Input::WasActionPressed(action_name ? StringView(action_name) : StringView{}) ? 1 : 0;
        }
        static int native_input_was_action_released(const char* action_name) {
            return Input::WasActionReleased(action_name ? StringView(action_name) : StringView{}) ? 1 : 0;
        }
        static float native_input_get_action_axis(const char* action_name) {
            return Input::GetActionAxis(action_name ? StringView(action_name) : StringView{});
        }
        static void native_input_get_action_vector2(const char* action_name, float* x, float* y) {
            const Vector2f value = Input::GetActionVector2(action_name ? StringView(action_name) : StringView{});
            if (x) *x = value.x;
            if (y) *y = value.y;
        }
        static int native_input_set_binding_interaction(const char* map_name, const char* action_name,
                                                        int interaction, float hold_seconds) {
            return Input::SetBindingInteraction(map_name ? StringView(map_name) : StringView{},
                                                action_name ? StringView(action_name) : StringView{},
                                                static_cast<InputInteraction>(interaction), hold_seconds) ? 1 : 0;
        }
        static int native_input_load_action_asset(const char* path) {
            return Input::LoadActionAsset(path ? String(path) : String{}) ? 1 : 0;
        }
        static int native_input_bind_gamepad_button(const char* map_name, const char* action_name, int button,
                                                    uint32_t device_id, float scale) {
            return Input::BindGamepadButton(map_name ? StringView(map_name) : StringView{},
                                            action_name ? StringView(action_name) : StringView{},
                                            static_cast<GamepadButtonCode>(button), device_id, scale) ? 1 : 0;
        }
        static int native_input_bind_gamepad_axis(const char* map_name, const char* action_name, int axis,
                                                  uint32_t device_id, float scale) {
            return Input::BindGamepadAxis(map_name ? StringView(map_name) : StringView{},
                                          action_name ? StringView(action_name) : StringView{},
                                          static_cast<GamepadAxisCode>(axis), device_id, scale) ? 1 : 0;
        }
        static int native_input_bind_gamepad_stick(const char* map_name, const char* action_name, int stick_axis,
                                                   uint32_t device_id, float scale) {
            return Input::BindGamepadStick(map_name ? StringView(map_name) : StringView{},
                                           action_name ? StringView(action_name) : StringView{},
                                           static_cast<GamepadAxisCode>(stick_axis), device_id, scale) ? 1 : 0;
        }
        static int native_input_bind_composite(const char* map_name, const char* action_name, const char* parts_json,
                                               uint32_t device_id) {
            DynamicArray<InputBinding> parts;
            if (parts_json) {
                try {
                    const Json j = Json::parse(parts_json);
                    if (j.is_array()) {
                        for (const auto& part : j) parts.push_back(ParseInputBinding(part));
                    }
                } catch (const Json::exception&) {
                }
            }
            return Input::BindComposite(map_name ? StringView(map_name) : StringView{},
                                        action_name ? StringView(action_name) : StringView{}, parts, device_id) ? 1 : 0;
        }
        static int native_input_set_binding_tap_params(const char* map_name, const char* action_name,
                                                       int binding_index, int tap_count, float tap_window) {
            return Input::SetBindingTapParams(map_name ? StringView(map_name) : StringView{},
                                              action_name ? StringView(action_name) : StringView{},
                                              static_cast<Size_t>(binding_index), tap_count, tap_window) ? 1 : 0;
        }
        static int native_input_set_binding_repeat_params(const char* map_name, const char* action_name,
                                                          int binding_index, float repeat_delay, float repeat_rate) {
            return Input::SetBindingRepeatParams(map_name ? StringView(map_name) : StringView{},
                                                 action_name ? StringView(action_name) : StringView{},
                                                 static_cast<Size_t>(binding_index), repeat_delay, repeat_rate) ? 1 : 0;
        }
        static int native_input_set_binding_processor(const char* map_name, const char* action_name,
                                                      int binding_index, int type, float a, float b) {
            InputProcessor processor;
            processor.type = static_cast<InputProcessorType>(type);
            processor.a = a;
            processor.b = b;
            return Input::SetBindingProcessor(map_name ? StringView(map_name) : StringView{},
                                              action_name ? StringView(action_name) : StringView{},
                                              static_cast<Size_t>(binding_index), processor) ? 1 : 0;
        }
        static uint32_t native_input_find_action_id(const char* map_name, const char* action_name) {
            return Input::FindActionId(map_name ? StringView(map_name) : StringView{},
                                       action_name ? StringView(action_name) : StringView{});
        }
        static uint32_t native_input_find_action_id_q(const char* qualified_name) {
            return Input::FindActionId(qualified_name ? StringView(qualified_name) : StringView{});
        }
        static int native_input_is_action_down_id(uint32_t action_id) { return Input::IsActionDown(action_id) ? 1 : 0; }
        static int native_input_was_action_pressed_id(uint32_t action_id) {
            return Input::WasActionPressed(action_id) ? 1 : 0;
        }
        static int native_input_was_action_released_id(uint32_t action_id) {
            return Input::WasActionReleased(action_id) ? 1 : 0;
        }
        static float native_input_get_action_axis_id(uint32_t action_id) { return Input::GetActionAxis(action_id); }
        static void native_input_get_action_vector2_id(uint32_t action_id, float* x, float* y) {
            const Vector2f value = Input::GetActionVector2(action_id);
            if (x) *x = value.x;
            if (y) *y = value.y;
        }
        static void ForwardInputEvent(const InputActionEvent& event) {
            if (!s_ScriptEngine) return;
            auto call = s_ScriptEngine->getCallFn();
            if (!call) return;
            int phase = static_cast<int>(event.phase);
            int vtype = 0;
            int vbool = 0;
            float v0 = 0.0f;
            float v1 = 0.0f;
            if (const auto* b = std::get_if<Bool>(&event.value)) { vtype = 0; vbool = *b ? 1 : 0; }
            else if (const auto* f = std::get_if<Float>(&event.value)) { vtype = 1; v0 = *f; }
            else if (const auto* v = std::get_if<Vector2f>(&event.value)) { vtype = 2; v0 = v->x; v1 = v->y; }
            uint32_t action_id = event.action_id;
            void* args[6] = { &action_id, &phase, &vtype, &vbool, &v0, &v1 };
            call("input_action_event", args, nullptr);
        }
        static uint64_t native_input_subscribe(const char* action_name, int phase) {
            if (!action_name) return 0;
            return Input::Subscribe(StringView(action_name), static_cast<InputActionPhase>(phase), ForwardInputEvent);
        }
        static uint64_t native_input_subscribe_id(uint32_t action_id, int phase) {
            return Input::Subscribe(action_id, static_cast<InputActionPhase>(phase), ForwardInputEvent);
        }
        static void native_input_unsubscribe(uint64_t subscription_id) { Input::Unsubscribe(subscription_id); }
        static int native_input_set_binding_override(const char* map_name, const char* action_name,
                                                     int binding_index, const char* binding_json) {
            if (!binding_json) return 0;
            try {
                const InputBinding binding = ParseInputBinding(Json::parse(binding_json));
                return Input::SetBindingOverride(map_name ? StringView(map_name) : StringView{},
                                                 action_name ? StringView(action_name) : StringView{},
                                                 static_cast<Size_t>(binding_index), binding) ? 1 : 0;
            } catch (const Json::exception&) {
                return 0;
            }
        }
        static int native_input_clear_binding_override(const char* map_name, const char* action_name,
                                                       int binding_index) {
            return Input::ClearBindingOverride(map_name ? StringView(map_name) : StringView{},
                                               action_name ? StringView(action_name) : StringView{},
                                               static_cast<Size_t>(binding_index)) ? 1 : 0;
        }
        static int native_input_begin_rebind_session(const char* map_name, const char* action_name,
                                                     int binding_index) {
            return Input::BeginRebindSession(map_name ? StringView(map_name) : StringView{},
                                             action_name ? StringView(action_name) : StringView{},
                                             static_cast<Size_t>(binding_index)) ? 1 : 0;
        }
        static void native_input_cancel_rebind_session() { Input::CancelRebindSession(); }
        static int native_input_is_rebind_session_active() { return Input::IsRebindSessionActive() ? 1 : 0; }
        static int native_input_load_config_overrides(const char* project_path, const char* user_path) {
            return Input::LoadConfigOverrides(project_path ? FsPath(project_path) : FsPath{},
                                              user_path ? FsPath(user_path) : FsPath{}) ? 1 : 0;
        }
        static int native_input_save_user_config_overrides(const char* user_path) {
            return Input::SaveUserConfigOverrides(user_path ? FsPath(user_path) : FsPath{}) ? 1 : 0;
        }
        static int native_input_is_gamepad_connected(uint32_t device_id) {
            return Input::IsGamepadConnected(device_id) ? 1 : 0;
        }
        static int native_input_is_gamepad_button_down(uint32_t device_id, int button) {
            return Input::IsGamepadButtonDown(device_id, static_cast<GamepadButtonCode>(button)) ? 1 : 0;
        }
        static int native_input_is_gamepad_button_pressed(uint32_t device_id, int button) {
            return Input::IsGamepadButtonPressed(device_id, static_cast<GamepadButtonCode>(button)) ? 1 : 0;
        }
        static int native_input_is_gamepad_button_released(uint32_t device_id, int button) {
            return Input::IsGamepadButtonReleased(device_id, static_cast<GamepadButtonCode>(button)) ? 1 : 0;
        }
        static float native_input_get_gamepad_axis(uint32_t device_id, int axis) {
            return Input::GetGamepadAxis(device_id, static_cast<GamepadAxisCode>(axis));
        }
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
            Entity e = s->getEntityByUUID(UUID(u));
            if (!e.valid()) { DO_ERROR("destroy_entity: {} not found", u); return; }
            s->destroyEntity(e);
            if (GetScriptSystem()) if (auto* r = GetScriptSystem()->getScriptRuntime()) r->removeEntityFromManagedWorld(u);
        }

        static void native_entity_enqueue_destroy(uint64_t u) {
            World* world = GetWorld();
            if (!world) return;
            world->getCommandBuffer().destroyEntity(UUID(u));
        }

        static void native_entity_enqueue_add_component(uint64_t u, const char* type) {
            World* world = GetWorld();
            if (!world || !type) return;
            auto it = s_EntityAddComponentFuncUmap.find(type);
            if (it == s_EntityAddComponentFuncUmap.end()) return;
            world->getCommandBuffer().addComponentByName(UUID(u), it->second);
        }

        static void native_entity_enqueue_remove_component(uint64_t u, const char* type) {
            World* world = GetWorld();
            if (!world || !type) return;
            auto it = s_EntityRemoveComponentFuncUmap.find(type);
            if (it == s_EntityRemoveComponentFuncUmap.end()) return;
            world->getCommandBuffer().removeComponentByName(UUID(u), it->second);
        }

        static void native_entity_enqueue_add_managed(uint64_t u, const char* type) {
            World* world = GetWorld();
            if (!world || !type) return;
            world->getCommandBuffer().addManagedComponent(UUID(u), type);
        }

        static void native_entity_enqueue_remove_managed(uint64_t u, const char* type) {
            World* world = GetWorld();
            if (!world || !type) return;
            world->getCommandBuffer().removeManagedComponent(UUID(u), type);
        }

        static void native_Rigidbody2dComponent_SetVelocity(uint64_t u, float x, float y) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) setVelocity2d(e, { x, y });
        }

        static void native_Rigidbody2dComponent_ApplyForce(uint64_t u, float x, float y) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) applyForce2d(e, { x, y });
        }

        static void native_Rigidbody2dComponent_ApplyImpulse(uint64_t u, float x, float y) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) applyImpulse2d(e, { x, y });
        }

        static int native_Rigidbody2dComponent_type_get(uint64_t u) {
            auto* c = TryGetComponent<Rigidbody2dComponent>(u);
            return c ? static_cast<int>(c->type) : 0;
        }
        static void native_Rigidbody2dComponent_type_set(uint64_t u, int v) {
            auto* c = TryGetComponent<Rigidbody2dComponent>(u);
            if (c && v >= 0 && v <= 2) c->type = static_cast<Rigidbody2dComponent::BodyType>(v);
        }
        static void native_Rigidbody2dComponent_position_get(uint64_t u, float* x, float* y) {
            if (x) *x = 0; if (y) *y = 0;
            Entity e = TryGetEntityByUuid(u);
            if (!e.valid()) return;
            Scene* s = GetCurrentScene(); if (!s) return;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return;
            Vector2f p = ps->getBodyPosition(s->registry(), e);
            if (x) *x = p.x;
            if (y) *y = p.y;
        }
        static void native_Rigidbody2dComponent_move_position(uint64_t u, float x, float y) {
            Entity e = TryGetEntityByUuid(u);
            if (!e.valid()) return;
            Scene* s = GetCurrentScene(); if (!s) return;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return;
            ps->moveBodyPosition(s->registry(), e, { x, y });
        }
        static void native_Rigidbody2dComponent_velocity_get(uint64_t u, float* x, float* y) {
            if (x) *x = 0; if (y) *y = 0;
            Entity e = TryGetEntityByUuid(u);
            if (!e.valid()) return;
            Scene* s = GetCurrentScene(); if (!s) return;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return;
            Vector2f v = ps->getBodyLinearVelocity(s->registry(), e);
            if (x) *x = v.x;
            if (y) *y = v.y;
        }
        static float native_time_get_fixed_delta_time() {
            PhysicsSystem* physics_system = GetPhysicsSystem();
            return physics_system ? physics_system->getFixedDt() : 1.0f / 60.0f;
        }
        static bool native_spriterenderercomponent_visible_get(uint64_t u) {
            auto* c = TryGetComponent<SpriteRendererComponent>(u);
            return c ? true : false;
        }
        static void native_spriterenderercomponent_visible_set(uint64_t u, bool v) {
        }

        static thread_local DynamicArray<Collision2dEvent> s_collision_pending{};

        static int native_physics2d_poll_event_count() {
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            DynamicArray<Collision2dEvent> ev;
            ps->takeCollisionEvents(s->registry(), ev);
            s_collision_pending = std::move(ev);
            return static_cast<int>(s_collision_pending.size());
        }
        static int native_physics2d_get_event(int idx, void* outStruct) {
            auto* p = static_cast<float*>(outStruct);
            if (!p) return 0;
            if (idx < 0 || idx >= static_cast<int>(s_collision_pending.size())) return 0;
            const auto& e = s_collision_pending[static_cast<size_t>(idx)];
            ui32 ea = e.entity_a, eb = e.entity_b;
            std::memcpy(p + 0, &ea, sizeof(ea));
            std::memcpy(p + 1, &eb, sizeof(eb));
            p[2] = e.point.x;   p[3] = e.point.y;
            p[4] = e.normal.x;  p[5] = e.normal.y;
            p[6] = e.relative_speed;
            p[7] = e.is_sensor ? 1.0f : 0.0f;
            p[8] = static_cast<float>(static_cast<int>(e.phase));
            return 1;
        }
        static int native_physics2d_raycast(float ox, float oy, float dx, float dy, float md,
                                            uint32_t layer, uint32_t mask, float minFraction,
                                            void* outHits, int capacity) {
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            DynamicArray<RaycastHit2d> hits;
            ps->raycast(s->registry(), { ox, oy }, { dx, dy }, md, Query2dFilter{ layer, mask }, hits);
            if (minFraction > 0.0f) {
                auto pend = std::remove_if(hits.begin(), hits.end(),
                    [=](const RaycastHit2d& h) { return h.fraction < minFraction; });
                hits.resize(static_cast<size_t>(pend - hits.begin()));
            }
            if (capacity <= 0 || !outHits) return static_cast<int>(hits.size());
            auto* dst = static_cast<float*>(outHits);
            int stride = 8;
            int n = std::min<int>(static_cast<int>(hits.size()), capacity);
            for (int i = 0; i < n; ++i) {
                const auto& h = hits[static_cast<size_t>(i)];
                float* base = dst + i * stride;
                base[0] = [&]() { float f; ui32 ent = h.entity; std::memcpy(&f, &ent, sizeof(ent)); return f; }();
                base[1] = h.point.x; base[2] = h.point.y;
                base[3] = h.normal.x; base[4] = h.normal.y;
                base[5] = h.fraction;
                base[6] = static_cast<float>(layer);
                base[7] = static_cast<float>(mask);
            }
            return n;
        }
        static int native_physics2d_boxcast(float cx, float cy, float hx, float hy,
                                            float angle, float dx, float dy, float md,
                                            uint32_t layer, uint32_t mask,
                                            void* outHits, int capacity) {
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            DynamicArray<RaycastHit2d> hits;
            ps->boxCast(s->registry(), { cx, cy }, { hx, hy }, angle, { dx, dy }, md,
                        Query2dFilter{ layer, mask }, hits);
            if (capacity <= 0 || !outHits) return static_cast<int>(hits.size());
            auto* dst = static_cast<float*>(outHits);
            int stride = 8;
            int n = std::min<int>(static_cast<int>(hits.size()), capacity);
            for (int i = 0; i < n; ++i) {
                const auto& h = hits[static_cast<size_t>(i)];
                float* base = dst + i * stride;
                base[0] = [&]() { float f; ui32 ent = h.entity; std::memcpy(&f, &ent, sizeof(ent)); return f; }();
                base[1] = h.point.x; base[2] = h.point.y;
                base[3] = h.normal.x; base[4] = h.normal.y;
                base[5] = h.fraction;
                base[6] = static_cast<float>(layer);
                base[7] = static_cast<float>(mask);
            }
            return n;
        }
        static int native_physics2d_overlap_aabb(float cx, float cy, float hx, float hy,
                                                 uint32_t layer, uint32_t mask,
                                                 void* outEntityIds, int capacity, int outStrideWords) {
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            DynamicArray<uint32_t> ents;
            ps->overlapAABB(s->registry(), { cx, cy }, { hx, hy }, Query2dFilter{ layer, mask }, ents);
            if (capacity <= 0 || !outEntityIds) return static_cast<int>(ents.size());
            auto* dst = static_cast<uint32_t*>(outEntityIds);
            int stride = std::max<int>(1, outStrideWords);
            int n = std::min<int>(static_cast<int>(ents.size()), capacity);
            for (int i = 0; i < n; ++i) dst[i * stride] = ents[static_cast<size_t>(i)];
            return n;
        }
        static void native_physics2d_ignore_collision(uint64_t a, uint64_t b, bool ignore) {
            Entity ea = TryGetEntityByUuid(a), eb = TryGetEntityByUuid(b);
            if (!ea.valid() || !eb.valid()) return;
            Scene* s = GetCurrentScene(); if (!s) return;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return;
            ps->ignoreCollision(s->registry(), ea, eb, ignore);
        }
        static int native_physics2d_get_collider_contacts(uint64_t u, void* outEntityIds, int capacity, int outStrideWords) {
            Entity e = TryGetEntityByUuid(u); if (!e.valid()) return 0;
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            DynamicArray<ui32> ents;
            ps->getColliderContacts(s->registry(), e, ents);
            auto* dst = static_cast<ui32*>(outEntityIds);
            if (!dst || capacity <= 0) return static_cast<int>(ents.size());
            int stride = std::max<int>(1, outStrideWords);
            int n = std::min<int>(static_cast<int>(ents.size()), capacity);
            for (int i = 0; i < n; ++i) dst[i * stride] = ents[static_cast<size_t>(i)];
            return n;
        }
        static int native_physics2d_collider_distance(uint64_t a, uint64_t b, void* outDist) {
            Entity ea = TryGetEntityByUuid(a), eb = TryGetEntityByUuid(b);
            if (!ea.valid() || !eb.valid()) return 0;
            Scene* s = GetCurrentScene(); if (!s) return 0;
            Physics2dSystem* ps = GetWorld()->findRuntimeSystem<Physics2dSystem>(); if (!ps) return 0;
            float dist = 0.0f;
            bool ok = ps->colliderDistance(s->registry(), ea, eb, dist);
            if (outDist) *static_cast<float*>(outDist) = dist;
            return ok ? 1 : 0;
        }

        static void native_RigidbodyComponent_SetVelocity(uint64_t u, float x, float y, float z) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) setVelocity3d(e, { x, y, z });
        }

        static void native_RigidbodyComponent_ApplyForce(uint64_t u, float x, float y, float z) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) applyForce3d(e, { x, y, z });
        }

        static void native_RigidbodyComponent_ApplyImpulse(uint64_t u, float x, float y, float z) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) applyImpulse3d(e, { x, y, z });
        }

        static void native_RigidbodyComponent_Teleport(uint64_t u, float px, float py, float pz, float qx, float qy, float qz, float qw) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) teleportEntity(e, { px, py, pz }, Quaternion(qw, qx, qy, qz));
        }

        static void native_AnimatorComponent_Play(uint64_t u, const char* name) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) animatorPlay(e, name ? name : "");
        }

        static void native_AnimatorComponent_Stop(uint64_t u) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) animatorStop(e);
        }

        static void native_AnimatorComponent_Resume(uint64_t u) {
            Entity e = TryGetEntityByUuid(u);
            if (e.valid()) animatorResume(e);
        }

        static void native_AudioSourceComponent_Play(uint64_t u) { if (auto* c = TryGetComponent<AudioSourceComponent>(u)) c->play(); }
        static void native_AudioSourceComponent_Stop(uint64_t u) { if (auto* c = TryGetComponent<AudioSourceComponent>(u)) c->stop(); }
        static void native_AudioSourceComponent_Pause(uint64_t u) { if (auto* c = TryGetComponent<AudioSourceComponent>(u)) c->pause(); }
        static void native_AudioSourceComponent_UnPause(uint64_t u) { if (auto* c = TryGetComponent<AudioSourceComponent>(u)) c->unPause(); }
        static int native_AudioSourceComponent_IsPlaying(uint64_t u) {
            auto* c = TryGetComponent<AudioSourceComponent>(u);
            return c && c->isPlaying() ? 1 : 0;
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
            auto& m = e.getComponent<TilemapComponent>();
            Tileset* ts = nullptr;
            try {
                Json j = Json::parse(json_str);
                String source;
                if (j.contains("Source")) source = j["Source"].get<String>();
                ObjectID ref;
                if (!source.empty()) {
                    ref = ResourceManager::Self().getAssetManager()->resolvePathToRef(FileID(source));
                }
                if (ref.isValid()) {
                    ts = Tileset::Create(ref, source);
                    ts->clear();
                } else {
                    ts = Tileset::CreateTransient();
                }
                if (j.contains("Name")) ts->name = j["Name"].get<String>();
                if (j.contains("FirstGid")) ts->first_gid = j["FirstGid"].get<UInt32>();
                if (j.contains("TileWidth")) ts->tile_width = j["TileWidth"].get<UInt32>();
                if (j.contains("TileHeight")) ts->tile_height = j["TileHeight"].get<UInt32>();
                if (j.contains("Columns")) ts->columns = j["Columns"].get<UInt32>();
                if (j.contains("TileCount")) ts->tile_count = j["TileCount"].get<UInt32>();
                if (j.contains("ImagePath")) ts->image_path = j["ImagePath"].get<String>();
                if (j.contains("TextureId")) ts->texture_id = j["TextureId"].get<UInt32>();
            } catch (...) { DO_ERROR("tilemap_add_tileset parse error"); return; }
            m.tilesets.push_back(PPtr<Tileset>(ts)); m.dirty = true;
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

        static int native_object_is_alive(int instanceID, int generation) {
            return Object::isAlive((InstanceID)instanceID, (UInt32)generation) ? 1 : 0;
        }

        static int native_object_get_generation(int instanceID) {
            auto* obj = Object::FindObjectFromInstanceID((InstanceID)instanceID);
            return obj ? (int)obj->getGeneration() : 0;
        }

        static int native_texture_load(const char* path) {
            if (!path || path[0] == '\0') return 0;
            auto* tex = ResourceManager::Self().loadObjectByPath<Texture2D>(FileID(String(path)));
            return tex ? (int)tex->getInstanceID() : 0;
        }

        static int native_sprite_load(const char* path) {
            if (!path || path[0] == '\0') return 0;
            auto* sprite = ResourceManager::Self().loadObjectByPath<Sprite>(FileID(String(path)));
            return sprite ? (int)sprite->getInstanceID() : 0;
        }

        static int native_load_object(const char* path, const char* type_name) {
            if (!path || path[0] == '\0' || !type_name) return 0;
            const FileID file_id{String(path)};
            if (strcmp(type_name, "Texture2D") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<Texture2D>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            if (strcmp(type_name, "Sprite") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<Sprite>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            if (strcmp(type_name, "Material") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<Material>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            if (strcmp(type_name, "AnimationClip") == 0 || strcmp(type_name, "Anim2DClip") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<Anim2DClip>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            if (strcmp(type_name, "Mesh") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<Mesh>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            if (strcmp(type_name, "AnimatorController") == 0) {
                auto* obj = ResourceManager::Self().loadObjectByPath<AnimatorController>(file_id);
                return obj ? (int)obj->getInstanceID() : 0;
            }
            return 0;
        }

        static int native_world_load_scene(const char* name, int mode) {
            if (!name) return 0;
            auto* world = GetWorld();
            if (!world) return 0;
            return world->loadScene(name, static_cast<LoadSceneMode>(mode)) ? 1 : 0;
        }

        static const char* native_world_get_active_scene_name() {
            auto* world = GetWorld();
            if (!world) return "";
            auto* scene = world->getActiveScene();
            return scene ? scene->getName().c_str() : "";
        }

        static const char* native_world_get_active_scene_entities() {
            DEF_STR_RET(world_entities);
            _s_world_entities = "[]";
            auto* world = GetWorld();
            if (!world) return _s_world_entities.c_str();
            auto* scene = world->getActiveScene();
            if (!scene) return _s_world_entities.c_str();

            Json arr = Json::array();
            for (Entity e : scene->getEntities()) {
                if (!e.valid()) continue;
                Json item = Json::object();
                item["id"] = static_cast<uint64_t>(e.uuid());
                item["name"] = e.name();
                arr.push_back(std::move(item));
            }
            _s_world_entities = arr.dump();
            return _s_world_entities.c_str();
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
            else if (pn == "depth")      elem->setDepth(static_cast<Float>(std::strtod(sv.c_str(), nullptr)));
            else if (pn == "position") {
                auto comma = sv.find(',');
                if (comma != String::npos) {
                    Float x = static_cast<Float>(std::strtod(sv.substr(0, comma).c_str(), nullptr));
                    Float y = static_cast<Float>(std::strtod(sv.substr(comma + 1).c_str(), nullptr));
                    elem->setPosition({x, y});
                }
            }
            else if (pn == "size") {
                auto comma = sv.find(',');
                if (comma != String::npos) {
                    Float x = static_cast<Float>(std::strtod(sv.substr(0, comma).c_str(), nullptr));
                    Float y = static_cast<Float>(std::strtod(sv.substr(comma + 1).c_str(), nullptr));
                    elem->setSize({x, y});
                }
            }
            else if (pn == "color" || pn == "tint") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem)) {
                    Float r = 1, g = 1, b = 1, a = 1;
                    auto c1 = sv.find(','), c2 = (c1 != String::npos) ? sv.find(',', c1 + 1) : String::npos;
                    auto c3 = (c2 != String::npos) ? sv.find(',', c2 + 1) : String::npos;
                    if (c1 != String::npos && c2 != String::npos && c3 != String::npos) {
                        r = static_cast<Float>(std::strtod(sv.substr(0, c1).c_str(), nullptr));
                        g = static_cast<Float>(std::strtod(sv.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr));
                        b = static_cast<Float>(std::strtod(sv.substr(c2 + 1, c3 - c2 - 1).c_str(), nullptr));
                        a = static_cast<Float>(std::strtod(sv.substr(c3 + 1).c_str(), nullptr));
                    }
                    widget->setColor(Color(r, g, b, a));
                }
            }
            else if (pn == "alpha") {
                if (auto* widget = dynamic_cast<UIWidget*>(elem))
                    widget->setAlpha(static_cast<Float>(std::strtod(sv.c_str(), nullptr)));
            }
            else if (pn == "text") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    label->setText(sv);
            }
            else if (pn == "font_size") {
                if (auto* label = dynamic_cast<UILabel*>(elem))
                    label->setFontSize(static_cast<int>(std::strtol(sv.c_str(), nullptr, 10)));
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
                        r = static_cast<Float>(std::strtod(sv.substr(0, c1).c_str(), nullptr));
                        g = static_cast<Float>(std::strtod(sv.substr(c1 + 1, c2 - c1 - 1).c_str(), nullptr));
                        b = static_cast<Float>(std::strtod(sv.substr(c2 + 1, c3 - c2 - 1).c_str(), nullptr));
                        a = static_cast<Float>(std::strtod(sv.substr(c3 + 1).c_str(), nullptr));
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
    X(native_input_register_action_map, int, (const char* map_name, int priority), map_name, priority) \
    X(native_input_set_action_map_enabled, int, (const char* map_name, int enabled), map_name, enabled) \
    X(native_input_set_action_map_consume, int, (const char* map_name, int consume), map_name, consume) \
    X(native_input_push_context, int, (const char* map_name), map_name) \
    X(native_input_pop_context, int, (const char* map_name), map_name) \
    X(native_input_register_action, int, (const char* map_name, const char* action_name, int value_type), map_name, action_name, value_type) \
    X(native_input_bind_key, int, (const char* map_name, const char* action_name, int key, float scale), map_name, action_name, key, scale) \
    X(native_input_unregister_action_map, int, (const char* map_name), map_name) \
    X(native_input_bind_key2d, int, (const char* map_name, const char* action_name, int key, float x, float y), map_name, action_name, key, x, y) \
    X(native_input_bind_mouse_button, int, (const char* map_name, const char* action_name, int button, float scale), map_name, action_name, button, scale) \
    X(native_input_bind_mouse_delta, int, (const char* map_name, const char* action_name, float scale), map_name, action_name, scale) \
    X(native_input_bind_mouse_wheel, int, (const char* map_name, const char* action_name, float scale), map_name, action_name, scale) \
    X(native_input_get_mouse_position, void, (float* x, float* y), x, y) \
    X(native_input_get_mouse_delta, void, (float* x, float* y), x, y) \
    X(native_input_get_mouse_wheel, void, (float* x, float* y), x, y) \
    X(native_input_is_action_down, int, (const char* action_name), action_name) \
    X(native_input_was_action_pressed, int, (const char* action_name), action_name) \
    X(native_input_was_action_released, int, (const char* action_name), action_name) \
    X(native_input_get_action_axis, float, (const char* action_name), action_name) \
    X(native_input_get_action_vector2, void, (const char* action_name, float* x, float* y), action_name, x, y) \
    X(native_input_set_binding_interaction, int, (const char* map_name, const char* action_name, int interaction, float hold_seconds), map_name, action_name, interaction, hold_seconds) \
    X(native_input_load_action_asset, int, (const char* path), path) \
    X(native_input_bind_gamepad_button, int, (const char* map_name, const char* action_name, int button, uint32_t device_id, float scale), map_name, action_name, button, device_id, scale) \
    X(native_input_bind_gamepad_axis, int, (const char* map_name, const char* action_name, int axis, uint32_t device_id, float scale), map_name, action_name, axis, device_id, scale) \
    X(native_input_bind_gamepad_stick, int, (const char* map_name, const char* action_name, int stick_axis, uint32_t device_id, float scale), map_name, action_name, stick_axis, device_id, scale) \
    X(native_input_bind_composite, int, (const char* map_name, const char* action_name, const char* parts_json, uint32_t device_id), map_name, action_name, parts_json, device_id) \
    X(native_input_set_binding_tap_params, int, (const char* map_name, const char* action_name, int binding_index, int tap_count, float tap_window), map_name, action_name, binding_index, tap_count, tap_window) \
    X(native_input_set_binding_repeat_params, int, (const char* map_name, const char* action_name, int binding_index, float repeat_delay, float repeat_rate), map_name, action_name, binding_index, repeat_delay, repeat_rate) \
    X(native_input_set_binding_processor, int, (const char* map_name, const char* action_name, int binding_index, int type, float a, float b), map_name, action_name, binding_index, type, a, b) \
    X(native_input_find_action_id, uint32_t, (const char* map_name, const char* action_name), map_name, action_name) \
    X(native_input_find_action_id_q, uint32_t, (const char* qualified_name), qualified_name) \
    X(native_input_is_action_down_id, int, (uint32_t action_id), action_id) \
    X(native_input_was_action_pressed_id, int, (uint32_t action_id), action_id) \
    X(native_input_was_action_released_id, int, (uint32_t action_id), action_id) \
    X(native_input_get_action_axis_id, float, (uint32_t action_id), action_id) \
    X(native_input_get_action_vector2_id, void, (uint32_t action_id, float* x, float* y), action_id, x, y) \
    X(native_input_subscribe, uint64_t, (const char* action_name, int phase), action_name, phase) \
    X(native_input_subscribe_id, uint64_t, (uint32_t action_id, int phase), action_id, phase) \
    X(native_input_unsubscribe, void, (uint64_t subscription_id), subscription_id) \
    X(native_input_set_binding_override, int, (const char* map_name, const char* action_name, int binding_index, const char* binding_json), map_name, action_name, binding_index, binding_json) \
    X(native_input_clear_binding_override, int, (const char* map_name, const char* action_name, int binding_index), map_name, action_name, binding_index) \
    X(native_input_begin_rebind_session, int, (const char* map_name, const char* action_name, int binding_index), map_name, action_name, binding_index) \
    X(native_input_cancel_rebind_session, void, (), ) \
    X(native_input_is_rebind_session_active, int, (), ) \
    X(native_input_load_config_overrides, int, (const char* project_path, const char* user_path), project_path, user_path) \
    X(native_input_save_user_config_overrides, int, (const char* user_path), user_path) \
    X(native_input_is_gamepad_connected, int, (uint32_t device_id), device_id) \
    X(native_input_is_gamepad_button_down, int, (uint32_t device_id, int button), device_id, button) \
    X(native_input_is_gamepad_button_pressed, int, (uint32_t device_id, int button), device_id, button) \
    X(native_input_is_gamepad_button_released, int, (uint32_t device_id, int button), device_id, button) \
    X(native_input_get_gamepad_axis, float, (uint32_t device_id, int axis), device_id, axis) \
    X(native_time_get_delta_time, float, (), ) \
    X(native_id_component_get_id, uint64_t, (uint64_t e), e) \
    X(native_id_component_get_name, const char*, (uint64_t e), e) \
    X(native_id_component_set_name, void, (uint64_t e, const char* v), e, v) \
    X(native_entity_enqueue_destroy, void, (uint64_t e), e) \
    X(native_entity_enqueue_add_component, void, (uint64_t e, const char* type), e, type) \
    X(native_entity_enqueue_remove_component, void, (uint64_t e, const char* type), e, type) \
    X(native_entity_enqueue_add_managed, void, (uint64_t e, const char* type), e, type) \
    X(native_entity_enqueue_remove_managed, void, (uint64_t e, const char* type), e, type) \
    X(native_Rigidbody2dComponent_SetVelocity, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_Rigidbody2dComponent_ApplyForce, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_Rigidbody2dComponent_ApplyImpulse, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_Rigidbody2dComponent_type_get, int, (uint64_t e), e) \
    X(native_Rigidbody2dComponent_type_set, void, (uint64_t e, int v), e, v) \
    X(native_Rigidbody2dComponent_position_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_Rigidbody2dComponent_move_position, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_Rigidbody2dComponent_velocity_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_time_get_fixed_delta_time, float, (), ) \
    X(native_spriterenderercomponent_visible_get, bool, (uint64_t e), e) \
    X(native_spriterenderercomponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_physics2d_poll_event_count, int, (), ) \
    X(native_physics2d_get_event, int, (int idx, void* outStruct), idx, outStruct) \
    X(native_physics2d_raycast, int, (float ox, float oy, float dx, float dy, float md, uint layer, uint mask, float minFrac, void* outHits, int cap), ox, oy, dx, dy, md, layer, mask, minFrac, outHits, cap) \
    X(native_physics2d_boxcast, int, (float cx, float cy, float hx, float hy, float ang, float dx, float dy, float md, uint layer, uint mask, void* outHits, int cap), cx, cy, hx, hy, ang, dx, dy, md, layer, mask, outHits, cap) \
    X(native_physics2d_overlap_aabb, int, (float cx, float cy, float hx, float hy, uint layer, uint mask, void* outIds, int cap, int stride), cx, cy, hx, hy, layer, mask, outIds, cap, stride) \
    X(native_physics2d_ignore_collision, void, (uint64_t a, uint64_t b, bool ignore), a, b, ignore) \
    X(native_physics2d_get_collider_contacts, int, (uint64_t e, void* outIds, int cap, int stride), e, outIds, cap, stride) \
    X(native_physics2d_collider_distance, int, (uint64_t a, uint64_t b, void* outDist), a, b, outDist) \
    X(native_RigidbodyComponent_SetVelocity, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_RigidbodyComponent_ApplyForce, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_RigidbodyComponent_ApplyImpulse, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_RigidbodyComponent_Teleport, void, (uint64_t e, float px, float py, float pz, float qx, float qy, float qz, float qw), e, px, py, pz, qx, qy, qz, qw) \
    X(native_AnimatorComponent_Play, void, (uint64_t e, const char* name), e, name) \
    X(native_AnimatorComponent_Stop, void, (uint64_t e), e) \
    X(native_AnimatorComponent_Resume, void, (uint64_t e), e) \
    X(native_AudioSourceComponent_Play, void, (uint64_t e), e) \
    X(native_AudioSourceComponent_Stop, void, (uint64_t e), e) \
    X(native_AudioSourceComponent_Pause, void, (uint64_t e), e) \
    X(native_AudioSourceComponent_UnPause, void, (uint64_t e), e) \
    X(native_AudioSourceComponent_IsPlaying, int, (uint64_t e), e) \
    /* === NATIVE_BINDINGS_GENERATED_START === */ \
X(native_Rigidbody2dComponent_gravity_scale_get, float, (uint64_t e), e) \
    X(native_Rigidbody2dComponent_gravity_scale_set, void, (uint64_t e, float v), e, v) \
    X(native_Rigidbody2dComponent_fixed_rotation_get, bool, (uint64_t e), e) \
    X(native_Rigidbody2dComponent_fixed_rotation_set, void, (uint64_t e, bool v), e, v) \
    X(native_CameraComponent_zoom_get, float, (uint64_t e), e) \
    X(native_CameraComponent_zoom_set, void, (uint64_t e, float v), e, v) \
    X(native_CameraComponent_fov_get, float, (uint64_t e), e) \
    X(native_CameraComponent_fov_set, void, (uint64_t e, float v), e, v) \
    X(native_CameraComponent_near_plane_get, float, (uint64_t e), e) \
    X(native_CameraComponent_near_plane_set, void, (uint64_t e, float v), e, v) \
    X(native_CameraComponent_far_plane_get, float, (uint64_t e), e) \
    X(native_CameraComponent_far_plane_set, void, (uint64_t e, float v), e, v) \
    X(native_CameraComponent_aspect_ratio_get, float, (uint64_t e), e) \
    X(native_CameraComponent_aspect_ratio_set, void, (uint64_t e, float v), e, v) \
    X(native_CameraComponent_background_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_CameraComponent_background_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_AnimationDriveModeComponent_enabled_get, bool, (uint64_t e), e) \
    X(native_AnimationDriveModeComponent_enabled_set, void, (uint64_t e, bool v), e, v) \
    X(native_AnimatorComponent_controller_get, int, (uint64_t e), e) \
    X(native_AnimatorComponent_controller_set, void, (uint64_t e, int v), e, v) \
    X(native_AnimatorComponent_speed_get, float, (uint64_t e), e) \
    X(native_AnimatorComponent_speed_set, void, (uint64_t e, float v), e, v) \
    X(native_AnimatorComponent_play_on_awake_get, bool, (uint64_t e), e) \
    X(native_AnimatorComponent_play_on_awake_set, void, (uint64_t e, bool v), e, v) \
    X(native_BoneAttachmentComponent_bone_name_get, const char*, (uint64_t e), e) \
    X(native_BoneAttachmentComponent_bone_name_set, void, (uint64_t e, const char* v), e, v) \
    X(native_BoneAttachmentComponent_local_offset_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_BoneAttachmentComponent_local_offset_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_BoneAttachmentComponent_follow_rotation_get, bool, (uint64_t e), e) \
    X(native_BoneAttachmentComponent_follow_rotation_set, void, (uint64_t e, bool v), e, v) \
    X(native_BoxColliderComponent_offset_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_BoxColliderComponent_offset_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_BoxColliderComponent_rotation_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_BoxColliderComponent_rotation_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_BoxColliderComponent_size_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_BoxColliderComponent_size_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_BoxColliderComponent_is_sensor_get, bool, (uint64_t e), e) \
    X(native_BoxColliderComponent_is_sensor_set, void, (uint64_t e, bool v), e, v) \
    X(native_BoxColliderComponent_layer_get, uint, (uint64_t e), e) \
    X(native_BoxColliderComponent_layer_set, void, (uint64_t e, uint v), e, v) \
    X(native_BoxColliderComponent_mask_get, uint, (uint64_t e), e) \
    X(native_BoxColliderComponent_mask_set, void, (uint64_t e, uint v), e, v) \
    X(native_BoxColliderComponent_density_get, float, (uint64_t e), e) \
    X(native_BoxColliderComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxColliderComponent_friction_get, float, (uint64_t e), e) \
    X(native_BoxColliderComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxColliderComponent_restitution_get, float, (uint64_t e), e) \
    X(native_BoxColliderComponent_restitution_set, void, (uint64_t e, float v), e, v) \
    X(native_CapsuleColliderComponent_offset_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_CapsuleColliderComponent_offset_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_CapsuleColliderComponent_rotation_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_CapsuleColliderComponent_rotation_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_CapsuleColliderComponent_radius_get, float, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_CapsuleColliderComponent_half_height_get, float, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_half_height_set, void, (uint64_t e, float v), e, v) \
    X(native_CapsuleColliderComponent_is_sensor_get, bool, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_is_sensor_set, void, (uint64_t e, bool v), e, v) \
    X(native_CapsuleColliderComponent_layer_get, uint, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_layer_set, void, (uint64_t e, uint v), e, v) \
    X(native_CapsuleColliderComponent_mask_get, uint, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_mask_set, void, (uint64_t e, uint v), e, v) \
    X(native_CapsuleColliderComponent_density_get, float, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_CapsuleColliderComponent_friction_get, float, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_CapsuleColliderComponent_restitution_get, float, (uint64_t e), e) \
    X(native_CapsuleColliderComponent_restitution_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleRendererComponent_radius_get, float, (uint64_t e), e) \
    X(native_CircleRendererComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_CircleRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_CircleRendererComponent_segments_get, uint, (uint64_t e), e) \
    X(native_CircleRendererComponent_segments_set, void, (uint64_t e, uint v), e, v) \
    X(native_CircleRendererComponent_thickness_get, float, (uint64_t e), e) \
    X(native_CircleRendererComponent_thickness_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_offset_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_BoxCollider2dComponent_offset_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_BoxCollider2dComponent_size_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_BoxCollider2dComponent_size_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_BoxCollider2dComponent_is_sensor_get, bool, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_is_sensor_set, void, (uint64_t e, bool v), e, v) \
    X(native_BoxCollider2dComponent_layer_get, uint, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_layer_set, void, (uint64_t e, uint v), e, v) \
    X(native_BoxCollider2dComponent_mask_get, uint, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_mask_set, void, (uint64_t e, uint v), e, v) \
    X(native_BoxCollider2dComponent_density_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_friction_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_restitution_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_restitution_set, void, (uint64_t e, float v), e, v) \
    X(native_BoxCollider2dComponent_restitution_threshold_get, float, (uint64_t e), e) \
    X(native_BoxCollider2dComponent_restitution_threshold_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleCollider2dComponent_offset_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_CircleCollider2dComponent_offset_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_CircleCollider2dComponent_radius_get, float, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleCollider2dComponent_is_sensor_get, bool, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_is_sensor_set, void, (uint64_t e, bool v), e, v) \
    X(native_CircleCollider2dComponent_layer_get, uint, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_layer_set, void, (uint64_t e, uint v), e, v) \
    X(native_CircleCollider2dComponent_mask_get, uint, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_mask_set, void, (uint64_t e, uint v), e, v) \
    X(native_CircleCollider2dComponent_density_get, float, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleCollider2dComponent_friction_get, float, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleCollider2dComponent_restitution_get, float, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_restitution_set, void, (uint64_t e, float v), e, v) \
    X(native_CircleCollider2dComponent_restitution_threshold_get, float, (uint64_t e), e) \
    X(native_CircleCollider2dComponent_restitution_threshold_set, void, (uint64_t e, float v), e, v) \
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
    X(native_FoliageRendererComponent_mesh_get, int, (uint64_t e), e) \
    X(native_FoliageRendererComponent_mesh_set, void, (uint64_t e, int v), e, v) \
    X(native_FoliageRendererComponent_visible_get, bool, (uint64_t e), e) \
    X(native_FoliageRendererComponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_FoliageRendererComponent_cast_shadow_get, bool, (uint64_t e), e) \
    X(native_FoliageRendererComponent_cast_shadow_set, void, (uint64_t e, bool v), e, v) \
    X(native_FoliageRendererComponent_instance_bounds_extent_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_FoliageRendererComponent_instance_bounds_extent_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_DistanceJoint2dComponent_target_entity_get, uint64_t, (uint64_t e), e) \
    X(native_DistanceJoint2dComponent_target_entity_set, void, (uint64_t e, uint64_t v), e, v) \
    X(native_DistanceJoint2dComponent_local_anchor_a_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_DistanceJoint2dComponent_local_anchor_a_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_DistanceJoint2dComponent_local_anchor_b_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_DistanceJoint2dComponent_local_anchor_b_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_DistanceJoint2dComponent_length_get, float, (uint64_t e), e) \
    X(native_DistanceJoint2dComponent_length_set, void, (uint64_t e, float v), e, v) \
    X(native_DistanceJoint2dComponent_frequency_get, float, (uint64_t e), e) \
    X(native_DistanceJoint2dComponent_frequency_set, void, (uint64_t e, float v), e, v) \
    X(native_DistanceJoint2dComponent_damping_ratio_get, float, (uint64_t e), e) \
    X(native_DistanceJoint2dComponent_damping_ratio_set, void, (uint64_t e, float v), e, v) \
    X(native_RevoluteJoint2dComponent_target_entity_get, uint64_t, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_target_entity_set, void, (uint64_t e, uint64_t v), e, v) \
    X(native_RevoluteJoint2dComponent_local_anchor_a_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_RevoluteJoint2dComponent_local_anchor_a_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_RevoluteJoint2dComponent_local_anchor_b_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_RevoluteJoint2dComponent_local_anchor_b_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_RevoluteJoint2dComponent_enable_limit_get, bool, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_enable_limit_set, void, (uint64_t e, bool v), e, v) \
    X(native_RevoluteJoint2dComponent_lower_angle_get, float, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_lower_angle_set, void, (uint64_t e, float v), e, v) \
    X(native_RevoluteJoint2dComponent_upper_angle_get, float, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_upper_angle_set, void, (uint64_t e, float v), e, v) \
    X(native_RevoluteJoint2dComponent_enable_motor_get, bool, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_enable_motor_set, void, (uint64_t e, bool v), e, v) \
    X(native_RevoluteJoint2dComponent_motor_speed_get, float, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_motor_speed_set, void, (uint64_t e, float v), e, v) \
    X(native_RevoluteJoint2dComponent_max_motor_torque_get, float, (uint64_t e), e) \
    X(native_RevoluteJoint2dComponent_max_motor_torque_set, void, (uint64_t e, float v), e, v) \
    X(native_MeshRendererComponent_mesh_get, int, (uint64_t e), e) \
    X(native_MeshRendererComponent_mesh_set, void, (uint64_t e, int v), e, v) \
    X(native_MeshRendererComponent_section_index_get, int, (uint64_t e), e) \
    X(native_MeshRendererComponent_section_index_set, void, (uint64_t e, int v), e, v) \
    X(native_MeshRendererComponent_visible_get, bool, (uint64_t e), e) \
    X(native_MeshRendererComponent_visible_set, void, (uint64_t e, bool v), e, v) \
    X(native_MeshRendererComponent_cast_shadow_get, bool, (uint64_t e), e) \
    X(native_MeshRendererComponent_cast_shadow_set, void, (uint64_t e, bool v), e, v) \
    X(native_RectRendererComponent_size_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_RectRendererComponent_size_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_RectRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_RectRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
    X(native_RectRendererComponent_thickness_get, float, (uint64_t e), e) \
    X(native_RectRendererComponent_thickness_set, void, (uint64_t e, float v), e, v) \
    X(native_RigidbodyComponent_gravity_scale_get, float, (uint64_t e), e) \
    X(native_RigidbodyComponent_gravity_scale_set, void, (uint64_t e, float v), e, v) \
    X(native_RigidbodyComponent_linear_damping_get, float, (uint64_t e), e) \
    X(native_RigidbodyComponent_linear_damping_set, void, (uint64_t e, float v), e, v) \
    X(native_RigidbodyComponent_angular_damping_get, float, (uint64_t e), e) \
    X(native_RigidbodyComponent_angular_damping_set, void, (uint64_t e, float v), e, v) \
    X(native_RigidbodyComponent_mass_override_get, float, (uint64_t e), e) \
    X(native_RigidbodyComponent_mass_override_set, void, (uint64_t e, float v), e, v) \
    X(native_RigidbodyComponent_lock_rotation_get, bool, (uint64_t e), e) \
    X(native_RigidbodyComponent_lock_rotation_set, void, (uint64_t e, bool v), e, v) \
    X(native_RigidbodyComponent_is_bullet_get, bool, (uint64_t e), e) \
    X(native_RigidbodyComponent_is_bullet_set, void, (uint64_t e, bool v), e, v) \
    X(native_RigidbodyComponent_enabled_get, bool, (uint64_t e), e) \
    X(native_RigidbodyComponent_enabled_set, void, (uint64_t e, bool v), e, v) \
    X(native_SphereColliderComponent_offset_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_SphereColliderComponent_offset_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_SphereColliderComponent_rotation_get, void, (uint64_t e, float* x, float* y, float* z), e, x, y, z) \
    X(native_SphereColliderComponent_rotation_set, void, (uint64_t e, float x, float y, float z), e, x, y, z) \
    X(native_SphereColliderComponent_radius_get, float, (uint64_t e), e) \
    X(native_SphereColliderComponent_radius_set, void, (uint64_t e, float v), e, v) \
    X(native_SphereColliderComponent_is_sensor_get, bool, (uint64_t e), e) \
    X(native_SphereColliderComponent_is_sensor_set, void, (uint64_t e, bool v), e, v) \
    X(native_SphereColliderComponent_layer_get, uint, (uint64_t e), e) \
    X(native_SphereColliderComponent_layer_set, void, (uint64_t e, uint v), e, v) \
    X(native_SphereColliderComponent_mask_get, uint, (uint64_t e), e) \
    X(native_SphereColliderComponent_mask_set, void, (uint64_t e, uint v), e, v) \
    X(native_SphereColliderComponent_density_get, float, (uint64_t e), e) \
    X(native_SphereColliderComponent_density_set, void, (uint64_t e, float v), e, v) \
    X(native_SphereColliderComponent_friction_get, float, (uint64_t e), e) \
    X(native_SphereColliderComponent_friction_set, void, (uint64_t e, float v), e, v) \
    X(native_SphereColliderComponent_restitution_get, float, (uint64_t e), e) \
    X(native_SphereColliderComponent_restitution_set, void, (uint64_t e, float v), e, v) \
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
    X(native_TagComponent_tag_get, const char*, (uint64_t e), e) \
    X(native_TagComponent_tag_set, void, (uint64_t e, const char* v), e, v) \
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
    X(native_SpriteRendererComponent_sprite_get, int, (uint64_t e), e) \
    X(native_SpriteRendererComponent_sprite_set, void, (uint64_t e, int v), e, v) \
    X(native_SpriteRendererComponent_flip_get, bool, (uint64_t e), e) \
    X(native_SpriteRendererComponent_flip_set, void, (uint64_t e, bool v), e, v) \
    X(native_SpriteRendererComponent_pivot_get, void, (uint64_t e, float* x, float* y), e, x, y) \
    X(native_SpriteRendererComponent_pivot_set, void, (uint64_t e, float x, float y), e, x, y) \
    X(native_SpriteRendererComponent_depth_get, float, (uint64_t e), e) \
    X(native_SpriteRendererComponent_depth_set, void, (uint64_t e, float v), e, v) \
    X(native_SpriteRendererComponent_color_get, void, (uint64_t e, float* r, float* g, float* b, float* a), e, r, g, b, a) \
    X(native_SpriteRendererComponent_color_set, void, (uint64_t e, float r, float g, float b, float a), e, r, g, b, a) \
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
    X(native_TilemapComponent_map_width_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_map_width_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_map_height_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_map_height_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_tile_width_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_tile_width_set, void, (uint64_t e, uint v), e, v) \
    X(native_TilemapComponent_tile_height_get, uint, (uint64_t e), e) \
    X(native_TilemapComponent_tile_height_set, void, (uint64_t e, uint v), e, v) \
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
    X(native_object_is_alive, int, (int instanceID, int generation), instanceID, generation) \
    X(native_object_get_generation, int, (int instanceID), instanceID) \
    X(native_texture_load, int, (const char* path), path) \
    X(native_sprite_load, int, (const char* path), path) \
    X(native_load_object, int, (const char* path, const char* type_name), path, type_name) \
    X(native_world_load_scene, int, (const char* name, int mode), name, mode) \
    X(native_world_get_active_scene_name, const char*, (), ) \
    X(native_world_get_active_scene_entities, const char*, (), ) \
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
