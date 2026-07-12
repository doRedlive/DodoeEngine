namespace GreenCake;

using System;
using System.Runtime.InteropServices;

internal static unsafe class NativeCalls
{
    [StructLayout(LayoutKind.Sequential)]
    public struct NativeBindings
    {
        public delegate* unmanaged<byte*, void>                                           native_log;
        public delegate* unmanaged<ulong, byte*, int>                                     native_entity_has_component;
        public delegate* unmanaged<ulong, byte*, int>                                     native_component_exists;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_add_component;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_remove_component;
        public delegate* unmanaged<int, int>                                              native_is_key_down;
        public delegate* unmanaged<float>                                                 native_time_get_delta_time;
        public delegate* unmanaged<ulong, ulong>                                          native_id_component_get_id;
        public delegate* unmanaged<ulong, byte*>                                          native_id_component_get_name;
        public delegate* unmanaged<ulong, byte*, void>                                    native_id_component_set_name;
        public delegate* unmanaged<ulong, byte*>                                          native_tag_component_get_tag;
        public delegate* unmanaged<ulong, byte*, void>                                    native_tag_component_set_tag;
        public delegate* unmanaged<ulong, float*, float*, float*, void>                    native_transform_get_position;
        public delegate* unmanaged<ulong, float, float, float, void>                       native_transform_set_position;
        public delegate* unmanaged<ulong, float*, float*, float*, void>                    native_transform_get_rotation;
        public delegate* unmanaged<ulong, float, float, float, void>                       native_transform_set_rotation;
        public delegate* unmanaged<ulong, float*, float*, float*, void>                    native_transform_get_scale;
        public delegate* unmanaged<ulong, float, float, float, void>                       native_transform_set_scale;
        public delegate* unmanaged<ulong, uint>                                           native_animation2d_get_anim_id;
        public delegate* unmanaged<ulong, uint, void>                                     native_animation2d_set_anim_id;
        public delegate* unmanaged<ulong, ulong>                                          native_animation2d_get_frame_id;
        public delegate* unmanaged<ulong, ulong, void>                                    native_animation2d_set_frame_id;
        public delegate* unmanaged<ulong, float>                                          native_animation2d_get_time;
        public delegate* unmanaged<ulong, float, void>                                    native_animation2d_set_time;
        public delegate* unmanaged<ulong, float>                                          native_animation2d_get_speed;
        public delegate* unmanaged<ulong, float, void>                                    native_animation2d_set_speed;
        public delegate* unmanaged<ulong, int>                                            native_camera2d_get_type;
        public delegate* unmanaged<ulong, int, void>                                      native_camera2d_set_type;
        public delegate* unmanaged<ulong, float>                                          native_camera2d_get_zoom;
        public delegate* unmanaged<ulong, float, void>                                    native_camera2d_set_zoom;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void>            native_camera2d_get_background;
        public delegate* unmanaged<ulong, float, float, float, float, void>                native_camera2d_set_background;
        public delegate* unmanaged<ulong, float*, float*, void>                            native_boxcollider2d_get_offset;
        public delegate* unmanaged<ulong, float, float, void>                              native_boxcollider2d_set_offset;
        public delegate* unmanaged<ulong, float*, float*, void>                            native_boxcollider2d_get_size;
        public delegate* unmanaged<ulong, float, float, void>                              native_boxcollider2d_set_size;
        public delegate* unmanaged<ulong, float>                                           native_boxcollider2d_get_density;
        public delegate* unmanaged<ulong, float, void>                                     native_boxcollider2d_set_density;
        public delegate* unmanaged<ulong, float>                                           native_boxcollider2d_get_friction;
        public delegate* unmanaged<ulong, float, void>                                     native_boxcollider2d_set_friction;
        public delegate* unmanaged<ulong, float>                                           native_boxcollider2d_get_restitution;
        public delegate* unmanaged<ulong, float, void>                                     native_boxcollider2d_set_restitution;
        public delegate* unmanaged<ulong, float>                                           native_boxcollider2d_get_restitution_threshold;
        public delegate* unmanaged<ulong, float, void>                                     native_boxcollider2d_set_restitution_threshold;
        public delegate* unmanaged<ulong, int>                                             native_mesh_renderer_get_value;
        public delegate* unmanaged<ulong, int, void>                                       native_mesh_renderer_set_value;
        public delegate* unmanaged<ulong, int>                                             native_rigidbody2d_get_type;
        public delegate* unmanaged<ulong, int, void>                                       native_rigidbody2d_set_type;
        public delegate* unmanaged<ulong, float>                                           native_rigidbody2d_get_gravity_scale;
        public delegate* unmanaged<ulong, float, void>                                     native_rigidbody2d_set_gravity_scale;
        public delegate* unmanaged<ulong, int>                                             native_rigidbody2d_get_fixed_rotation;
        public delegate* unmanaged<ulong, int, void>                                       native_rigidbody2d_set_fixed_rotation;
        public delegate* unmanaged<ulong, float, float, void>                               native_rigidbody2d_set_linear_velocity;
        public delegate* unmanaged<ulong, float, float, int, void>                          native_rigidbody2d_apply_force_to_center;
        public delegate* unmanaged<ulong, float, float, int, void>                          native_rigidbody2d_apply_linear_impulse;
        public delegate* unmanaged<ulong, int>                                             native_sprite_renderer_get_texture_id;
        public delegate* unmanaged<ulong, int, void>                                       native_sprite_renderer_set_texture_id;
        public delegate* unmanaged<ulong, int>                                             native_sprite_renderer_get_flip;
        public delegate* unmanaged<ulong, int, void>                                       native_sprite_renderer_set_flip;
        public delegate* unmanaged<ulong, float*, float*, void>                             native_sprite_renderer_get_pivot;
        public delegate* unmanaged<ulong, float, float, void>                               native_sprite_renderer_set_pivot;
        public delegate* unmanaged<ulong, float>                                           native_sprite_renderer_get_depth;
        public delegate* unmanaged<ulong, float, void>                                     native_sprite_renderer_set_depth;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void>             native_sprite_renderer_get_color;
        public delegate* unmanaged<ulong, float, float, float, float, void>                 native_sprite_renderer_set_color;
        public delegate* unmanaged<byte*, ulong>                                           native_create_entity;
        public delegate* unmanaged<ulong, void>                                            native_destroy_entity;
        public delegate* unmanaged<ulong, int, int, int, int, void>                         native_tilemap_set_data;
        public delegate* unmanaged<ulong, byte*, void>                                     native_tilemap_add_tileset;
        public delegate* unmanaged<ulong, uint*, int, int, int, byte*, int, float, int, int, void> native_tile_layer_set_data;
        public delegate* unmanaged<ulong, ulong, void>                                     native_entity_set_parent;
        public delegate* unmanaged<ulong, uint>                                            native_tilemap_get_map_width;
        public delegate* unmanaged<ulong, uint, void>                                      native_tilemap_set_map_width;
        public delegate* unmanaged<ulong, uint>                                            native_tilemap_get_map_height;
        public delegate* unmanaged<ulong, uint, void>                                      native_tilemap_set_map_height;
        public delegate* unmanaged<ulong, uint>                                            native_tilemap_get_tile_width;
        public delegate* unmanaged<ulong, uint, void>                                      native_tilemap_set_tile_width;
        public delegate* unmanaged<ulong, uint>                                            native_tilemap_get_tile_height;
        public delegate* unmanaged<ulong, uint, void>                                      native_tilemap_set_tile_height;
        public delegate* unmanaged<ulong, byte*>                                           native_tile_layer_get_name;
        public delegate* unmanaged<ulong, byte*, void>                                     native_tile_layer_set_name;
        public delegate* unmanaged<ulong, uint>                                            native_tile_layer_get_width;
        public delegate* unmanaged<ulong, uint, void>                                      native_tile_layer_set_width;
        public delegate* unmanaged<ulong, uint>                                            native_tile_layer_get_height;
        public delegate* unmanaged<ulong, uint, void>                                      native_tile_layer_set_height;
        public delegate* unmanaged<ulong, int>                                             native_tile_layer_get_visible;
        public delegate* unmanaged<ulong, int, void>                                       native_tile_layer_set_visible;
        public delegate* unmanaged<ulong, float>                                           native_tile_layer_get_opacity;
        public delegate* unmanaged<ulong, float, void>                                     native_tile_layer_set_opacity;
        public delegate* unmanaged<ulong, int>                                             native_tile_layer_get_offset_x;
        public delegate* unmanaged<ulong, int, void>                                       native_tile_layer_set_offset_x;
        public delegate* unmanaged<ulong, int>                                             native_tile_layer_get_offset_y;
        public delegate* unmanaged<ulong, int, void>                                       native_tile_layer_set_offset_y;
        public delegate* unmanaged<byte*>                                                  native_get_asset_directory;
    }

    private static NativeBindings* b;

    internal static void Bind(NativeBindings* bindings)
    {
        b = bindings;
    }

    private static string PtrToStr(byte* ptr)
    {
        if (ptr == null) return "";
        return Marshal.PtrToStringUTF8((IntPtr)ptr);
    }

    private static byte* StrToPtr(string s)
    {
        if (s == null) s = "";
        return (byte*)Marshal.StringToCoTaskMemUTF8(s);
    }

    internal static void Native_Log(string message)
    {
        var ptr = StrToPtr(message);
        try { b->native_log(ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_EntityHasComponent(ulong entityId, Type componentType)
    {
        var name = componentType.FullName ?? componentType.Name;
        var ptr = StrToPtr(name);
        try { return b->native_entity_has_component(entityId, ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityAddComponent(ulong entityId, Component component) { }

    internal static void Native_EntityRemoveComponent(ulong entityId, Type componentType)
    {
        var name = componentType.FullName ?? componentType.Name;
        var ptr = StrToPtr(name);
        try { b->native_entity_remove_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_IsKeyDown(KeyCode keycode)
    {
        return b->native_is_key_down((int)keycode) != 0;
    }

    internal static bool Native_ComponentExists(ulong entityId, Type componentType)
    {
        var name = componentType.FullName ?? componentType.Name;
        var ptr = StrToPtr(name);
        try { return b->native_component_exists(entityId, ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static float Native_TimeGetDeltaTime()
    {
        return b->native_time_get_delta_time();
    }

    internal static void Native_TransfromComponentGetPosition(ulong entityId, out Vector3f position)
    {
        float x, y, z;
        b->native_transform_get_position(entityId, &x, &y, &z);
        position = new Vector3f(x, y, z);
    }

    internal static void Native_TransfromComponentSetPosition(ulong entityId, ref Vector3f position)
    {
        b->native_transform_set_position(entityId, position.X, position.Y, position.Z);
    }

    internal static void Native_TransfromComponentGetRotation(ulong entityId, out Vector3f rotation)
    {
        float x, y, z;
        b->native_transform_get_rotation(entityId, &x, &y, &z);
        rotation = new Vector3f(x, y, z);
    }

    internal static void Native_TransfromComponentSetRotation(ulong entityId, ref Vector3f rotation)
    {
        b->native_transform_set_rotation(entityId, rotation.X, rotation.Y, rotation.Z);
    }

    internal static void Native_TransfromComponentGetScale(ulong entityId, out Vector3f scale)
    {
        float x, y, z;
        b->native_transform_get_scale(entityId, &x, &y, &z);
        scale = new Vector3f(x, y, z);
    }

    internal static void Native_TransfromComponentSetScale(ulong entityId, ref Vector3f scale)
    {
        b->native_transform_set_scale(entityId, scale.X, scale.Y, scale.Z);
    }

    internal static ulong Native_IDComponentGetID(ulong entityId) => b->native_id_component_get_id(entityId);

    internal static string Native_IDComponentGetName(ulong entityId) => PtrToStr(b->native_id_component_get_name(entityId));

    internal static void Native_IDComponentSetName(ulong entityId, string name)
    {
        var ptr = StrToPtr(name);
        try { b->native_id_component_set_name(entityId, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static string Native_TagComponentGetTag(ulong entityId) => PtrToStr(b->native_tag_component_get_tag(entityId));

    internal static void Native_TagComponentSetTag(ulong entityId, string tag)
    {
        var ptr = StrToPtr(tag);
        try { b->native_tag_component_set_tag(entityId, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static uint Native_Animation2dComponentGetCurrentAnimationID(ulong entityId) => b->native_animation2d_get_anim_id(entityId);
    internal static void Native_Animation2dComponentSetCurrentAnimationID(ulong entityId, uint v) => b->native_animation2d_set_anim_id(entityId, v);
    internal static ulong Native_Animation2dComponentGetCurrentFrameID(ulong entityId) => b->native_animation2d_get_frame_id(entityId);
    internal static void Native_Animation2dComponentSetCurrentFrameID(ulong entityId, ulong v) => b->native_animation2d_set_frame_id(entityId, v);
    internal static float Native_Animation2dComponentGetCurrentTimeDuration(ulong entityId) => b->native_animation2d_get_time(entityId);
    internal static void Native_Animation2dComponentSetCurrentTimeDuration(ulong entityId, float v) => b->native_animation2d_set_time(entityId, v);
    internal static float Native_Animation2dComponentGetSpeed(ulong entityId) => b->native_animation2d_get_speed(entityId);
    internal static void Native_Animation2dComponentSetSpeed(ulong entityId, float v) => b->native_animation2d_set_speed(entityId, v);

    internal static int Native_Camera2dComponentGetType(ulong entityId) => b->native_camera2d_get_type(entityId);
    internal static void Native_Camera2dComponentSetType(ulong entityId, int v) => b->native_camera2d_set_type(entityId, v);
    internal static float Native_Camera2dComponentGetZoom(ulong entityId) => b->native_camera2d_get_zoom(entityId);
    internal static void Native_Camera2dComponentSetZoom(ulong entityId, float v) => b->native_camera2d_set_zoom(entityId, v);

    internal static void Native_Camera2dComponentGetBackground(ulong entityId, out Color color)
    {
        float r, g, bg, a;
        b->native_camera2d_get_background(entityId, &r, &g, &bg, &a);
        color = new Color(r, g, bg, a);
    }

    internal static void Native_Camera2dComponentSetBackground(ulong entityId, ref Color color)
    {
        b->native_camera2d_set_background(entityId, color.R, color.G, color.B, color.A);
    }

    internal static void Native_BoxCollider2dComponentGetOffset(ulong entityId, out Vector2f offset)
    {
        float x, y;
        b->native_boxcollider2d_get_offset(entityId, &x, &y);
        offset = new Vector2f(x, y);
    }

    internal static void Native_BoxCollider2dComponentSetOffset(ulong entityId, ref Vector2f offset)
    {
        b->native_boxcollider2d_set_offset(entityId, offset.X, offset.Y);
    }

    internal static void Native_BoxCollider2dComponentGetSize(ulong entityId, out Vector2f size)
    {
        float x, y;
        b->native_boxcollider2d_get_size(entityId, &x, &y);
        size = new Vector2f(x, y);
    }

    internal static void Native_BoxCollider2dComponentSetSize(ulong entityId, ref Vector2f size)
    {
        b->native_boxcollider2d_set_size(entityId, size.X, size.Y);
    }

    internal static float Native_BoxCollider2dComponentGetDensity(ulong entityId) => b->native_boxcollider2d_get_density(entityId);
    internal static void Native_BoxCollider2dComponentSetDensity(ulong entityId, float v) => b->native_boxcollider2d_set_density(entityId, v);
    internal static float Native_BoxCollider2dComponentGetFriction(ulong entityId) => b->native_boxcollider2d_get_friction(entityId);
    internal static void Native_BoxCollider2dComponentSetFriction(ulong entityId, float v) => b->native_boxcollider2d_set_friction(entityId, v);
    internal static float Native_BoxCollider2dComponentGetRestitution(ulong entityId) => b->native_boxcollider2d_get_restitution(entityId);
    internal static void Native_BoxCollider2dComponentSetRestitution(ulong entityId, float v) => b->native_boxcollider2d_set_restitution(entityId, v);
    internal static float Native_BoxCollider2dComponentGetRestitutionThreshold(ulong entityId) => b->native_boxcollider2d_get_restitution_threshold(entityId);
    internal static void Native_BoxCollider2dComponentSetRestitutionThreshold(ulong entityId, float v) => b->native_boxcollider2d_set_restitution_threshold(entityId, v);

    internal static int Native_MeshRendererComponentGetValue(ulong entityId) => b->native_mesh_renderer_get_value(entityId);
    internal static void Native_MeshRendererComponentSetValue(ulong entityId, int v) => b->native_mesh_renderer_set_value(entityId, v);

    internal static int Native_Rigidbody2dComponentGetType(ulong entityId) => b->native_rigidbody2d_get_type(entityId);
    internal static void Native_Rigidbody2dComponentSetType(ulong entityId, int v) => b->native_rigidbody2d_set_type(entityId, v);
    internal static float Native_Rigidbody2dComponentGetGravityScale(ulong entityId) => b->native_rigidbody2d_get_gravity_scale(entityId);
    internal static void Native_Rigidbody2dComponentSetGravityScale(ulong entityId, float v) => b->native_rigidbody2d_set_gravity_scale(entityId, v);
    internal static bool Native_Rigidbody2dComponentGetFixedRotation(ulong entityId) => b->native_rigidbody2d_get_fixed_rotation(entityId) != 0;
    internal static void Native_Rigidbody2dComponentSetFixedRotation(ulong entityId, bool v) => b->native_rigidbody2d_set_fixed_rotation(entityId, v ? 1 : 0);

    internal static void Native_Rigidbody2dComponentSetLinearVelocity(ulong entityId, ref Vector2f velocity)
    {
        b->native_rigidbody2d_set_linear_velocity(entityId, velocity.X, velocity.Y);
    }

    internal static void Native_Rigidbody2dComponentApplyForceToCenter(ulong entityId, ref Vector2f force, bool wake)
    {
        b->native_rigidbody2d_apply_force_to_center(entityId, force.X, force.Y, wake ? 1 : 0);
    }

    internal static void Native_Rigidbody2dComponentApplyLinearImpulseToCenter(ulong entityId, ref Vector2f impulse, bool wake)
    {
        b->native_rigidbody2d_apply_linear_impulse(entityId, impulse.X, impulse.Y, wake ? 1 : 0);
    }

    internal static uint Native_SpriteRendererComponentGetTextureID(ulong entityId) => (uint)b->native_sprite_renderer_get_texture_id(entityId);
    internal static void Native_SpriteRendererComponentSetTextureID(ulong entityId, uint v) => b->native_sprite_renderer_set_texture_id(entityId, (int)v);
    internal static bool Native_SpriteRendererComponentGetFlip(ulong entityId) => b->native_sprite_renderer_get_flip(entityId) != 0;
    internal static void Native_SpriteRendererComponentSetFlip(ulong entityId, bool v) => b->native_sprite_renderer_set_flip(entityId, v ? 1 : 0);

    internal static void Native_SpriteRendererComponentGetPivot(ulong entityId, out Vector2f pivot)
    {
        float x, y;
        b->native_sprite_renderer_get_pivot(entityId, &x, &y);
        pivot = new Vector2f(x, y);
    }

    internal static void Native_SpriteRendererComponentSetPivot(ulong entityId, ref Vector2f pivot)
    {
        b->native_sprite_renderer_set_pivot(entityId, pivot.X, pivot.Y);
    }

    internal static float Native_SpriteRendererComponentGetDepth(ulong entityId) => b->native_sprite_renderer_get_depth(entityId);
    internal static void Native_SpriteRendererComponentSetDepth(ulong entityId, float v) => b->native_sprite_renderer_set_depth(entityId, v);

    internal static void Native_SpriteRendererComponentGetColor(ulong entityId, out Color color)
    {
        float r, g, bg, a;
        b->native_sprite_renderer_get_color(entityId, &r, &g, &bg, &a);
        color = new Color(r, g, bg, a);
    }

    internal static void Native_SpriteRendererComponentSetColor(ulong entityId, ref Color color)
    {
        b->native_sprite_renderer_set_color(entityId, color.R, color.G, color.B, color.A);
    }

    internal static ulong Native_CreateEntity(string name)
    {
        var ptr = StrToPtr(name);
        try { return b->native_create_entity(ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_DestroyEntity(ulong entityId) => b->native_destroy_entity(entityId);

    internal static void Native_TilemapSetData(ulong entityId, int w, int h, int tw, int th)
    {
        b->native_tilemap_set_data(entityId, w, h, tw, th);
    }

    internal static void Native_TilemapAddTileset(ulong entityId, string json)
    {
        var ptr = StrToPtr(json);
        try { b->native_tilemap_add_tileset(entityId, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_TileLayerSetData(ulong entityId, uint[] tiles, int w, int h, string name, bool visible, float opacity, int ox, int oy)
    {
        var namePtr = StrToPtr(name);
        fixed (uint* p = tiles)
        {
            try { b->native_tile_layer_set_data(entityId, p, tiles?.Length ?? 0, w, h, namePtr, visible ? 1 : 0, opacity, ox, oy); }
            finally { Marshal.FreeCoTaskMem((IntPtr)namePtr); }
        }
    }

    internal static void Native_EntitySetParent(ulong child, ulong parent) => b->native_entity_set_parent(child, parent);

    internal static uint Native_TilemapComponentGetMapWidth(ulong e) => b->native_tilemap_get_map_width(e);
    internal static void Native_TilemapComponentSetMapWidth(ulong e, uint v) => b->native_tilemap_set_map_width(e, v);
    internal static uint Native_TilemapComponentGetMapHeight(ulong e) => b->native_tilemap_get_map_height(e);
    internal static void Native_TilemapComponentSetMapHeight(ulong e, uint v) => b->native_tilemap_set_map_height(e, v);
    internal static uint Native_TilemapComponentGetTileWidth(ulong e) => b->native_tilemap_get_tile_width(e);
    internal static void Native_TilemapComponentSetTileWidth(ulong e, uint v) => b->native_tilemap_set_tile_width(e, v);
    internal static uint Native_TilemapComponentGetTileHeight(ulong e) => b->native_tilemap_get_tile_height(e);
    internal static void Native_TilemapComponentSetTileHeight(ulong e, uint v) => b->native_tilemap_set_tile_height(e, v);

    internal static string Native_TileLayerComponentGetLayerName(ulong e) => PtrToStr(b->native_tile_layer_get_name(e));

    internal static void Native_TileLayerComponentSetLayerName(ulong e, string v)
    {
        var ptr = StrToPtr(v);
        try { b->native_tile_layer_set_name(e, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static uint Native_TileLayerComponentGetLayerWidth(ulong e) => b->native_tile_layer_get_width(e);
    internal static void Native_TileLayerComponentSetLayerWidth(ulong e, uint v) => b->native_tile_layer_set_width(e, v);
    internal static uint Native_TileLayerComponentGetLayerHeight(ulong e) => b->native_tile_layer_get_height(e);
    internal static void Native_TileLayerComponentSetLayerHeight(ulong e, uint v) => b->native_tile_layer_set_height(e, v);
    internal static bool Native_TileLayerComponentGetVisible(ulong e) => b->native_tile_layer_get_visible(e) != 0;
    internal static void Native_TileLayerComponentSetVisible(ulong e, bool v) => b->native_tile_layer_set_visible(e, v ? 1 : 0);
    internal static float Native_TileLayerComponentGetOpacity(ulong e) => b->native_tile_layer_get_opacity(e);
    internal static void Native_TileLayerComponentSetOpacity(ulong e, float v) => b->native_tile_layer_set_opacity(e, v);
    internal static int Native_TileLayerComponentGetOffsetX(ulong e) => b->native_tile_layer_get_offset_x(e);
    internal static void Native_TileLayerComponentSetOffsetX(ulong e, int v) => b->native_tile_layer_set_offset_x(e, v);
    internal static int Native_TileLayerComponentGetOffsetY(ulong e) => b->native_tile_layer_get_offset_y(e);
    internal static void Native_TileLayerComponentSetOffsetY(ulong e, int v) => b->native_tile_layer_set_offset_y(e, v);

    internal static string Native_GetAssetDirectory() => PtrToStr(b->native_get_asset_directory());
}
