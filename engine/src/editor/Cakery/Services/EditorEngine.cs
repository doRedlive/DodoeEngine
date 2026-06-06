// do@Redlive
using Dodoe.Bindings;

namespace Cakery.Services;

public static class EditorEngine
{
    private static SWIGTYPE_p_void? _ctx;

    public static SWIGTYPE_p_void Ctx => _ctx!;

    public static string Version => DodoeRuntime.dodoe_get_version();

    public static void Initialize() => _ctx = DodoeRuntime.dodoe_context_create();
    public static void Shutdown() { if (_ctx != null) { DodoeRuntime.dodoe_context_destroy(_ctx); _ctx = null; } }
    public static void Tick() => DodoeRuntime.dodoe_context_tick(Ctx);

    public static ulong EntityCreate() => DodoeRuntime.dodoe_entity_create(Ctx);
    public static void EntityDestroy(ulong e) => DodoeRuntime.dodoe_entity_destroy(Ctx, e);
    public static string EntityGetName(ulong e) => DodoeRuntime.dodoe_entity_get_name(Ctx, e);
    public static void EntitySetName(ulong e, string n) => DodoeRuntime.dodoe_entity_set_name(Ctx, e, n);

    public static void EntitySetPosition(ulong e, float x, float y)
    { var p = new DodoeVec2 { x = x, y = y }; DodoeRuntime.dodoe_entity_set_position(Ctx, e, p); }

    public static (float x, float y) EntityGetPosition(ulong e)
    { var p = DodoeRuntime.dodoe_entity_get_position(Ctx, e); return (p.x, p.y); }

    public static void EntitySetScale(ulong e, float x, float y)
    { var s = new DodoeVec2 { x = x, y = y }; DodoeRuntime.dodoe_entity_set_scale(Ctx, e, s); }

    public static (float x, float y) EntityGetScale(ulong e)
    { var s = DodoeRuntime.dodoe_entity_get_scale(Ctx, e); return (s.x, s.y); }

    public static void EntitySetRotation(ulong e, float r) => DodoeRuntime.dodoe_entity_set_rotation(Ctx, e, r);
    public static float EntityGetRotation(ulong e) => DodoeRuntime.dodoe_entity_get_rotation(Ctx, e);

    public static int EntityGetChildCount(ulong p) => DodoeRuntime.dodoe_entity_get_child_count(Ctx, p);
    public static ulong EntityGetChildAt(ulong p, int i) => DodoeRuntime.dodoe_entity_get_child_at(Ctx, p, i);
    public static void EntitySetParent(ulong c, ulong p) => DodoeRuntime.dodoe_entity_set_parent(Ctx, c, p);

    public static bool EntityHasComponent(ulong e, string t) => DodoeRuntime.dodoe_entity_has_component(Ctx, e, t) != 0;
    public static void EntityAddComponent(ulong e, string t) => DodoeRuntime.dodoe_entity_add_component(Ctx, e, t);
    public static void EntityRemoveComponent(ulong e, string t) => DodoeRuntime.dodoe_entity_remove_component(Ctx, e, t);
    public static int EntityGetComponentCount(ulong e) => DodoeRuntime.dodoe_entity_get_component_count(Ctx, e);
    public static string EntityGetComponentType(ulong e, int i) => DodoeRuntime.dodoe_entity_get_component_type(Ctx, e, i);

    public static void WorldSetState(int s) => DodoeRuntime.dodoe_world_set_state(Ctx, s);
    public static void SelectEntity(ulong e) => DodoeRuntime.dodoe_editor_select_entity(Ctx, e);
    public static ulong GetSelectedEntity() => DodoeRuntime.dodoe_editor_get_selected_entity(Ctx);

    public static ulong TextureLoad(string p) => DodoeRuntime.dodoe_texture_load(Ctx, p);
    public static DodoeTextureInfo TextureGetInfo(ulong id) => DodoeRuntime.dodoe_texture_get_info(Ctx, id);

    public static int AssetGetCount(string t) => DodoeRuntime.dodoe_asset_get_count(Ctx, t);
    public static DodoeAssetRef AssetGetAt(string t, int i) => DodoeRuntime.dodoe_asset_get_at(Ctx, t, i);

    public static void ViewportAttach(IntPtr nativeHandle, int w, int hh) => DodoeRuntime.dodoe_viewport_attach(Ctx, new SWIGTYPE_p_void(nativeHandle, false), w, hh);
    public static void ViewportResize(int w, int h) => DodoeRuntime.dodoe_viewport_resize(Ctx, w, h);
    public static void ViewportDetach() => DodoeRuntime.dodoe_viewport_detach(Ctx);
}
