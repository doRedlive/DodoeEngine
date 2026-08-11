namespace GreenCake;

using System;
using System.Runtime.InteropServices;

internal static unsafe partial class NativeCalls
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
        // === NATIVE_BINDINGS_GENERATED_START ===
// === GENERATED BINDINGS START (do not edit manually) ===
        public delegate* unmanaged<ulong, uint> native_TilemapComponent_map_width_get;
        public delegate* unmanaged<ulong, uint, void> native_TilemapComponent_map_width_set;
        public delegate* unmanaged<ulong, uint> native_TilemapComponent_map_height_get;
        public delegate* unmanaged<ulong, uint, void> native_TilemapComponent_map_height_set;
        public delegate* unmanaged<ulong, uint> native_TilemapComponent_tile_width_get;
        public delegate* unmanaged<ulong, uint, void> native_TilemapComponent_tile_width_set;
        public delegate* unmanaged<ulong, uint> native_TilemapComponent_tile_height_get;
        public delegate* unmanaged<ulong, uint, void> native_TilemapComponent_tile_height_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_FoliageRendererInstance_position_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_FoliageRendererInstance_position_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_FoliageRendererInstance_rotation_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_FoliageRendererInstance_rotation_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_FoliageRendererInstance_scale_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_FoliageRendererInstance_scale_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_FoliageRendererInstance_color_tint_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_FoliageRendererInstance_color_tint_set;
        public delegate* unmanaged<ulong, float> native_FoliageRendererInstance_wind_phase_get;
        public delegate* unmanaged<ulong, float, void> native_FoliageRendererInstance_wind_phase_set;
        public delegate* unmanaged<ulong, float> native_FoliageRendererInstance_variation_get;
        public delegate* unmanaged<ulong, float, void> native_FoliageRendererInstance_variation_set;
        public delegate* unmanaged<ulong, bool> native_FoliageRendererComponent_visible_get;
        public delegate* unmanaged<ulong, bool, void> native_FoliageRendererComponent_visible_set;
        public delegate* unmanaged<ulong, bool> native_FoliageRendererComponent_cast_shadow_get;
        public delegate* unmanaged<ulong, bool, void> native_FoliageRendererComponent_cast_shadow_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_FoliageRendererComponent_instance_bounds_extent_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_FoliageRendererComponent_instance_bounds_extent_set;
        public delegate* unmanaged<ulong, float> native_Animation2dComponent_cur_time_duration_get;
        public delegate* unmanaged<ulong, float, void> native_Animation2dComponent_cur_time_duration_set;
        public delegate* unmanaged<ulong, float> native_Animation2dComponent_speed_get;
        public delegate* unmanaged<ulong, float, void> native_Animation2dComponent_speed_set;
        public delegate* unmanaged<ulong, float> native_CameraComponent_zoom_get;
        public delegate* unmanaged<ulong, float, void> native_CameraComponent_zoom_set;
        public delegate* unmanaged<ulong, float> native_CameraComponent_fov_get;
        public delegate* unmanaged<ulong, float, void> native_CameraComponent_fov_set;
        public delegate* unmanaged<ulong, float> native_CameraComponent_near_plane_get;
        public delegate* unmanaged<ulong, float, void> native_CameraComponent_near_plane_set;
        public delegate* unmanaged<ulong, float> native_CameraComponent_far_plane_get;
        public delegate* unmanaged<ulong, float, void> native_CameraComponent_far_plane_set;
        public delegate* unmanaged<ulong, float> native_CameraComponent_aspect_ratio_get;
        public delegate* unmanaged<ulong, float, void> native_CameraComponent_aspect_ratio_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_CameraComponent_background_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_CameraComponent_background_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_BoxCollider2dComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, void> native_BoxCollider2dComponent_offset_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_BoxCollider2dComponent_size_get;
        public delegate* unmanaged<ulong, float, float, void> native_BoxCollider2dComponent_size_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_density_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_restitution_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_restitution_threshold_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_restitution_threshold_set;
        public delegate* unmanaged<ulong, float> native_CircleRendererComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_CircleRendererComponent_radius_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_CircleRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_CircleRendererComponent_color_set;
        public delegate* unmanaged<ulong, uint> native_CircleRendererComponent_segments_get;
        public delegate* unmanaged<ulong, uint, void> native_CircleRendererComponent_segments_set;
        public delegate* unmanaged<ulong, float> native_CircleRendererComponent_thickness_get;
        public delegate* unmanaged<ulong, float, void> native_CircleRendererComponent_thickness_set;
        public delegate* unmanaged<ulong, float> native_Rigidbody2dComponent_gravity_scale_get;
        public delegate* unmanaged<ulong, float, void> native_Rigidbody2dComponent_gravity_scale_set;
        public delegate* unmanaged<ulong, bool> native_Rigidbody2dComponent_fixed_rotation_get;
        public delegate* unmanaged<ulong, bool, void> native_Rigidbody2dComponent_fixed_rotation_set;
        public delegate* unmanaged<ulong, bool> native_MeshRendererComponent_visible_get;
        public delegate* unmanaged<ulong, bool, void> native_MeshRendererComponent_visible_set;
        public delegate* unmanaged<ulong, bool> native_MeshRendererComponent_cast_shadow_get;
        public delegate* unmanaged<ulong, bool, void> native_MeshRendererComponent_cast_shadow_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_PointLightComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_PointLightComponent_color_set;
        public delegate* unmanaged<ulong, float> native_PointLightComponent_intensity_get;
        public delegate* unmanaged<ulong, float, void> native_PointLightComponent_intensity_set;
        public delegate* unmanaged<ulong, float> native_PointLightComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_PointLightComponent_radius_set;
        public delegate* unmanaged<ulong, float> native_PointLightComponent_range_get;
        public delegate* unmanaged<ulong, float, void> native_PointLightComponent_range_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_SpotLightComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_SpotLightComponent_color_set;
        public delegate* unmanaged<ulong, float> native_SpotLightComponent_intensity_get;
        public delegate* unmanaged<ulong, float, void> native_SpotLightComponent_intensity_set;
        public delegate* unmanaged<ulong, float> native_SpotLightComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_SpotLightComponent_radius_set;
        public delegate* unmanaged<ulong, float> native_SpotLightComponent_range_get;
        public delegate* unmanaged<ulong, float, void> native_SpotLightComponent_range_set;
        public delegate* unmanaged<ulong, float> native_SpotLightComponent_inner_angle_get;
        public delegate* unmanaged<ulong, float, void> native_SpotLightComponent_inner_angle_set;
        public delegate* unmanaged<ulong, float> native_SpotLightComponent_outer_angle_get;
        public delegate* unmanaged<ulong, float, void> native_SpotLightComponent_outer_angle_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_RectRendererComponent_size_get;
        public delegate* unmanaged<ulong, float, float, void> native_RectRendererComponent_size_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_RectRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_RectRendererComponent_color_set;
        public delegate* unmanaged<ulong, float> native_RectRendererComponent_thickness_get;
        public delegate* unmanaged<ulong, float, void> native_RectRendererComponent_thickness_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_LineRendererComponent_direction_get;
        public delegate* unmanaged<ulong, float, float, void> native_LineRendererComponent_direction_set;
        public delegate* unmanaged<ulong, float> native_LineRendererComponent_length_get;
        public delegate* unmanaged<ulong, float, void> native_LineRendererComponent_length_set;
        public delegate* unmanaged<ulong, float> native_LineRendererComponent_thickness_get;
        public delegate* unmanaged<ulong, float, void> native_LineRendererComponent_thickness_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_LineRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_LineRendererComponent_color_set;
        public delegate* unmanaged<ulong, float> native_SkyLightComponent_intensity_get;
        public delegate* unmanaged<ulong, float, void> native_SkyLightComponent_intensity_set;
        public delegate* unmanaged<ulong, int> native_SpriteRendererComponent_sprite_get;
        public delegate* unmanaged<ulong, int, void> native_SpriteRendererComponent_sprite_set;
        public delegate* unmanaged<ulong, bool> native_SpriteRendererComponent_flip_get;
        public delegate* unmanaged<ulong, bool, void> native_SpriteRendererComponent_flip_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_SpriteRendererComponent_pivot_get;
        public delegate* unmanaged<ulong, float, float, void> native_SpriteRendererComponent_pivot_set;
        public delegate* unmanaged<ulong, float> native_SpriteRendererComponent_depth_get;
        public delegate* unmanaged<ulong, float, void> native_SpriteRendererComponent_depth_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_SpriteRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_SpriteRendererComponent_color_set;
        public delegate* unmanaged<ulong, byte*> native_TagComponent_tag_get;
        public delegate* unmanaged<ulong, byte*, void> native_TagComponent_tag_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_TransformComponent_position_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_TransformComponent_position_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_TransformComponent_rotation_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_TransformComponent_rotation_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_TransformComponent_scale_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_TransformComponent_scale_set;
        public delegate* unmanaged<ulong, ulong> native_HierarchyComponent_parent_uuid_get;
        public delegate* unmanaged<ulong, ulong, void> native_HierarchyComponent_parent_uuid_set;
        public delegate* unmanaged<ulong, int> native_HierarchyComponent_child_count_get;
        public delegate* unmanaged<ulong, int, void> native_HierarchyComponent_child_count_set;
        public delegate* unmanaged<ulong, byte*> native_TileLayerComponent_layer_name_get;
        public delegate* unmanaged<ulong, byte*, void> native_TileLayerComponent_layer_name_set;
        public delegate* unmanaged<ulong, uint> native_TileLayerComponent_layer_width_get;
        public delegate* unmanaged<ulong, uint, void> native_TileLayerComponent_layer_width_set;
        public delegate* unmanaged<ulong, uint> native_TileLayerComponent_layer_height_get;
        public delegate* unmanaged<ulong, uint, void> native_TileLayerComponent_layer_height_set;
        public delegate* unmanaged<ulong, bool> native_TileLayerComponent_visible_get;
        public delegate* unmanaged<ulong, bool, void> native_TileLayerComponent_visible_set;
        public delegate* unmanaged<ulong, float> native_TileLayerComponent_opacity_get;
        public delegate* unmanaged<ulong, float, void> native_TileLayerComponent_opacity_set;
        public delegate* unmanaged<ulong, int> native_TileLayerComponent_offset_x_get;
        public delegate* unmanaged<ulong, int, void> native_TileLayerComponent_offset_x_set;
        public delegate* unmanaged<ulong, int> native_TileLayerComponent_offset_y_get;
        public delegate* unmanaged<ulong, int, void> native_TileLayerComponent_offset_y_set;
// === GENERATED BINDINGS END ===
// === NATIVE_BINDINGS_GENERATED_END ===
        public delegate* unmanaged<byte*, ulong>                                           native_create_entity;
        public delegate* unmanaged<ulong, void>                                            native_destroy_entity;
        public delegate* unmanaged<ulong, int, int, int, int, void>                         native_tilemap_set_data;
        public delegate* unmanaged<ulong, byte*, void>                                     native_tilemap_add_tileset;
        public delegate* unmanaged<ulong, uint*, int, int, int, byte*, int, float, int, int, void> native_tile_layer_set_data;
        public delegate* unmanaged<ulong, ulong, void>                                     native_entity_set_parent;
        public delegate* unmanaged<byte*>                                                  native_get_asset_directory;
        public delegate* unmanaged<int, byte*>                                            native_object_get_type_name;
        public delegate* unmanaged<byte*, int>                                            native_texture_load;
        public delegate* unmanaged<byte*, int>                                            native_sprite_load;
        public delegate* unmanaged<byte*, int, int>                                      native_world_load_scene;
        public delegate* unmanaged<byte*>                                                 native_world_get_active_scene_name;
        public delegate* unmanaged<byte*>                                                 native_world_get_active_scene_entities;
        public delegate* unmanaged<byte*, void>                                          native_world_unload_scene;
        public delegate* unmanaged<byte*, int, int>                                      native_world_load_scene_async;
        public delegate* unmanaged<int, int>                                             native_world_is_load_complete;
        // === UI Bindings ===
        public delegate* unmanaged<byte*, int>                                            native_ui_load_layout;
        public delegate* unmanaged<void>                                                  native_ui_clear_all;
        public delegate* unmanaged<byte*, byte*, uint>                                    native_ui_find_element;
        public delegate* unmanaged<uint, byte*, byte*, void>                              native_ui_element_set_property;
        public delegate* unmanaged<uint, byte*, byte*>                                    native_ui_element_get_property;
        public delegate* unmanaged<byte*, byte*, byte*, uint>                              native_ui_create_element;
        public delegate* unmanaged<uint, byte*, int>                                       native_ui_poll_event;
        public delegate* unmanaged<uint, int>                                             native_ui_UIElement_visible_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIElement_visible_set;
        public delegate* unmanaged<uint, float>                                            native_ui_UIElement_depth_get;
        public delegate* unmanaged<uint, float, void>                                     native_ui_UIElement_depth_set;
        public delegate* unmanaged<uint, float*, float*, void>                            native_ui_UIElement_position_get;
        public delegate* unmanaged<uint, float, float, void>                              native_ui_UIElement_position_set;
        public delegate* unmanaged<uint, float*, float*, void>                            native_ui_UIElement_size_get;
        public delegate* unmanaged<uint, float, float, void>                              native_ui_UIElement_size_set;
        public delegate* unmanaged<uint, float*, float*, float*, float*, void>             native_ui_UIWidget_color_get;
        public delegate* unmanaged<uint, float, float, float, float, void>                native_ui_UIWidget_color_set;
        public delegate* unmanaged<uint, float>                                            native_ui_UIWidget_alpha_get;
        public delegate* unmanaged<uint, float, void>                                     native_ui_UIWidget_alpha_set;
        public delegate* unmanaged<uint, byte*>                                           native_ui_UILabel_text_get;
        public delegate* unmanaged<uint, byte*, void>                                     native_ui_UILabel_text_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UILabel_font_size_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UILabel_font_size_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UIImage_preserve_aspect_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIImage_preserve_aspect_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UIImage_flip_h_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIImage_flip_h_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UIImage_flip_v_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIImage_flip_v_set;
        public delegate* unmanaged<uint, float*, float*, float*, float*, void>             native_ui_UIImage_color_get;
        public delegate* unmanaged<uint, float, float, float, float, void>                native_ui_UIImage_color_set;
        public delegate* unmanaged<uint, float>                                            native_ui_UIImage_alpha_get;
        public delegate* unmanaged<uint, float, void>                                     native_ui_UIImage_alpha_set;
        public delegate* unmanaged<uint, byte*>                                           native_ui_UIButton_label_get;
        public delegate* unmanaged<uint, byte*, void>                                     native_ui_UIButton_label_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UIButton_interactable_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIButton_interactable_set;
        public delegate* unmanaged<uint, float*, float*, float*, float*, void>             native_ui_UIPanel_background_color_get;
        public delegate* unmanaged<uint, float, float, float, float, void>                native_ui_UIPanel_background_color_set;
        public delegate* unmanaged<uint, int>                                             native_ui_UIPanel_clip_children_get;
        public delegate* unmanaged<uint, int, void>                                       native_ui_UIPanel_clip_children_set;
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
        var name = componentType.Name;
        var ptr = StrToPtr(name);
        try { return b->native_entity_has_component(entityId, ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityAddComponent(ulong entityId, CakeComponent component) { }

    internal static void Native_EntityAddComponent(ulong entityId, Type componentType)
    {
        var name = componentType.Name;
        var ptr = StrToPtr(name);
        try { b->native_entity_add_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityRemoveComponent(ulong entityId, Type componentType)
    {
        var name = componentType.Name;
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
        var name = componentType.Name;
        var ptr = StrToPtr(name);
        try { return b->native_component_exists(entityId, ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static float Native_TimeGetDeltaTime()
    {
        return b->native_time_get_delta_time();
    }

    internal static ulong Native_IDComponentGetID(ulong entityId) => b->native_id_component_get_id(entityId);

    internal static string Native_IDComponentGetName(ulong entityId) => PtrToStr(b->native_id_component_get_name(entityId));

    internal static void Native_IDComponentSetName(ulong entityId, string name)
    {
        var ptr = StrToPtr(name);
        try { b->native_id_component_set_name(entityId, ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static int Native_WorldLoadScene(string name, int mode)
    {
        var ptr = StrToPtr(name);
        try { return b->native_world_load_scene(ptr, mode); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static string Native_WorldGetActiveSceneName() => PtrToStr(b->native_world_get_active_scene_name());

    internal static string Native_WorldGetActiveSceneEntities() => PtrToStr(b->native_world_get_active_scene_entities());

    internal static int Native_WorldLoadSceneAsync(string name, int mode)
    {
        var ptr = StrToPtr(name);
        try { return b->native_world_load_scene_async(ptr, mode); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_WorldIsLoadComplete(int token)
    {
        return b->native_world_is_load_complete(token) != 0;
    }

    internal static void Native_WorldUnloadScene(string name)
    {
        var ptr = StrToPtr(name);
        try { b->native_world_unload_scene(ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
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

    internal static string Native_GetAssetDirectory() => PtrToStr(b->native_get_asset_directory());

    internal static string Native_ObjectGetTypeName(int instanceID) => PtrToStr(b->native_object_get_type_name(instanceID));

    internal static bool Native_ObjectIsAlive(int instanceID, int generation) => b->native_object_is_alive(instanceID, generation) != 0;

    internal static int Native_ObjectGeneration(int instanceID) => b->native_object_get_generation(instanceID);

    internal static int Native_TextureLoad(string path)
    {
        var ptr = StrToPtr(path);
        try { return b->native_texture_load(ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static int Native_SpriteLoad(string path)
    {
        var ptr = StrToPtr(path);
        try { return b->native_sprite_load(ptr); } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static int Native_LoadObject(string path, string typeName)
    {
        var pPtr = StrToPtr(path);
        var tPtr = StrToPtr(typeName);
        try { return b->native_load_object(pPtr, tPtr); }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)pPtr);
            Marshal.FreeCoTaskMem((IntPtr)tPtr);
        }
    }

    // === UI Wrapper Methods ===

    internal static bool Native_UILoadLayout(string filePath)
    {
        var ptr = StrToPtr(filePath);
        try { return b->native_ui_load_layout(ptr) != 0; } finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_UIClearAll()
    {
        b->native_ui_clear_all();
    }

    internal static uint Native_UIFindElement(string elementId, string typeName)
    {
        var idPtr = StrToPtr(elementId);
        var typePtr = StrToPtr(typeName);
        try { return b->native_ui_find_element(idPtr, typePtr); }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)idPtr);
            Marshal.FreeCoTaskMem((IntPtr)typePtr);
        }
    }

    internal static void Native_UISetProperty(uint elementId, string propName, string value)
    {
        var nPtr = StrToPtr(propName);
        var vPtr = StrToPtr(value);
        try { b->native_ui_element_set_property(elementId, nPtr, vPtr); }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)nPtr);
            Marshal.FreeCoTaskMem((IntPtr)vPtr);
        }
    }

    internal static string Native_UIGetProperty(uint elementId, string propName)
    {
        var nPtr = StrToPtr(propName);
        try { return PtrToStr(b->native_ui_element_get_property(elementId, nPtr)); }
        finally { Marshal.FreeCoTaskMem((IntPtr)nPtr); }
    }

    internal static uint Native_UICreateElement(string type, string id, string parentId)
    {
        var tPtr = StrToPtr(type);
        var iPtr = StrToPtr(id);
        var pPtr = StrToPtr(parentId ?? "");
        try { return b->native_ui_create_element(tPtr, iPtr, pPtr); }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)tPtr);
            Marshal.FreeCoTaskMem((IntPtr)iPtr);
            Marshal.FreeCoTaskMem((IntPtr)pPtr);
        }
    }

    internal static int Native_UIPollEvent(uint elementId, string eventName)
    {
        var ptr = StrToPtr(eventName);
        try { return b->native_ui_poll_event(elementId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }
}
