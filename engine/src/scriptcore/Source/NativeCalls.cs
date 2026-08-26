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
        public delegate* unmanaged<byte*, int, int>                                       native_input_register_action_map;
        public delegate* unmanaged<byte*, int, int>                                       native_input_set_action_map_enabled;
        public delegate* unmanaged<byte*, int, int>                                       native_input_set_action_map_consume;
        public delegate* unmanaged<byte*, int>                                             native_input_push_context;
        public delegate* unmanaged<byte*, int>                                             native_input_pop_context;
        public delegate* unmanaged<byte*, byte*, int, int>                                native_input_register_action;
        public delegate* unmanaged<byte*, byte*, int, float, int>                         native_input_bind_key;
        public delegate* unmanaged<byte*, int>                                             native_input_unregister_action_map;
        public delegate* unmanaged<byte*, byte*, int, float, float, int>                   native_input_bind_key2d;
        public delegate* unmanaged<byte*, byte*, int, float, int>                          native_input_bind_mouse_button;
        public delegate* unmanaged<byte*, byte*, float, int>                               native_input_bind_mouse_delta;
        public delegate* unmanaged<byte*, byte*, float, int>                               native_input_bind_mouse_wheel;
        public delegate* unmanaged<float*, float*, void>                                   native_input_get_mouse_position;
        public delegate* unmanaged<float*, float*, void>                                   native_input_get_mouse_delta;
        public delegate* unmanaged<float*, float*, void>                                   native_input_get_mouse_wheel;
        public delegate* unmanaged<byte*, int>                                             native_input_is_action_down;
        public delegate* unmanaged<byte*, int>                                             native_input_was_action_pressed;
        public delegate* unmanaged<byte*, int>                                             native_input_was_action_released;
        public delegate* unmanaged<byte*, float>                                           native_input_get_action_axis;
        public delegate* unmanaged<byte*, float*, float*, void>                            native_input_get_action_vector2;
        public delegate* unmanaged<byte*, byte*, int, float, int>                           native_input_set_binding_interaction;
        public delegate* unmanaged<byte*, int>                                             native_input_load_action_asset;
        public delegate* unmanaged<byte*, byte*, int, uint, float, int>                    native_input_bind_gamepad_button;
        public delegate* unmanaged<byte*, byte*, int, uint, float, int>                    native_input_bind_gamepad_axis;
        public delegate* unmanaged<byte*, byte*, int, uint, float, int>                    native_input_bind_gamepad_stick;
        public delegate* unmanaged<byte*, byte*, byte*, uint, int>                         native_input_bind_composite;
        public delegate* unmanaged<byte*, byte*, int, int, float, int>                     native_input_set_binding_tap_params;
        public delegate* unmanaged<byte*, byte*, int, float, float, int>                   native_input_set_binding_repeat_params;
        public delegate* unmanaged<byte*, byte*, int, int, float, float, int>              native_input_set_binding_processor;
        public delegate* unmanaged<byte*, byte*, uint>                                     native_input_find_action_id;
        public delegate* unmanaged<byte*, uint>                                            native_input_find_action_id_q;
        public delegate* unmanaged<uint, int>                                              native_input_is_action_down_id;
        public delegate* unmanaged<uint, int>                                              native_input_was_action_pressed_id;
        public delegate* unmanaged<uint, int>                                              native_input_was_action_released_id;
        public delegate* unmanaged<uint, float>                                            native_input_get_action_axis_id;
        public delegate* unmanaged<uint, float*, float*, void>                             native_input_get_action_vector2_id;
        public delegate* unmanaged<byte*, int, ulong>                                      native_input_subscribe;
        public delegate* unmanaged<uint, int, ulong>                                       native_input_subscribe_id;
        public delegate* unmanaged<ulong, void>                                            native_input_unsubscribe;
        public delegate* unmanaged<byte*, byte*, int, byte*, int>                          native_input_set_binding_override;
        public delegate* unmanaged<byte*, byte*, int, int>                                 native_input_clear_binding_override;
        public delegate* unmanaged<byte*, byte*, int, int>                                 native_input_begin_rebind_session;
        public delegate* unmanaged<void>                                                   native_input_cancel_rebind_session;
        public delegate* unmanaged<int>                                                    native_input_is_rebind_session_active;
        public delegate* unmanaged<byte*, byte*, int>                                      native_input_load_config_overrides;
        public delegate* unmanaged<byte*, int>                                             native_input_save_user_config_overrides;
        public delegate* unmanaged<uint, int>                                              native_input_is_gamepad_connected;
        public delegate* unmanaged<uint, int, int>                                         native_input_is_gamepad_button_down;
        public delegate* unmanaged<uint, int, int>                                         native_input_is_gamepad_button_pressed;
        public delegate* unmanaged<uint, int, int>                                         native_input_is_gamepad_button_released;
        public delegate* unmanaged<uint, int, float>                                       native_input_get_gamepad_axis;
        public delegate* unmanaged<float>                                                 native_time_get_delta_time;
        public delegate* unmanaged<ulong, ulong>                                          native_id_component_get_id;
        public delegate* unmanaged<ulong, byte*>                                          native_id_component_get_name;
        public delegate* unmanaged<ulong, byte*, void>                                    native_id_component_set_name;
        public delegate* unmanaged<ulong, void>                                           native_entity_enqueue_destroy;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_enqueue_add_component;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_enqueue_remove_component;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_enqueue_add_managed;
        public delegate* unmanaged<ulong, byte*, void>                                    native_entity_enqueue_remove_managed;
        public delegate* unmanaged<ulong, float, float, void>                             native_Rigidbody2dComponent_SetVelocity;
        public delegate* unmanaged<ulong, float, float, void>                             native_Rigidbody2dComponent_ApplyForce;
        public delegate* unmanaged<ulong, float, float, void>                             native_Rigidbody2dComponent_ApplyImpulse;
        public delegate* unmanaged<ulong, int>                                            native_Rigidbody2dComponent_type_get;
        public delegate* unmanaged<ulong, int, void>                                      native_Rigidbody2dComponent_type_set;
        public delegate* unmanaged<ulong, float*, float*, void>                           native_Rigidbody2dComponent_position_get;
        public delegate* unmanaged<ulong, float, float, void>                             native_Rigidbody2dComponent_move_position;
        public delegate* unmanaged<ulong, float*, float*, void>                           native_Rigidbody2dComponent_velocity_get;
        public delegate* unmanaged<float>                                                 native_time_get_fixed_delta_time;
        public delegate* unmanaged<ulong, bool>                                           native_spriterenderercomponent_visible_get;
        public delegate* unmanaged<ulong, bool, void>                                     native_spriterenderercomponent_visible_set;
        public delegate* unmanaged<int>                                                   native_physics2d_poll_event_count;
        public delegate* unmanaged<int, IntPtr, int>                                      native_physics2d_get_event;
        public delegate* unmanaged<float, float, float, float, float, uint, uint, float, IntPtr, int, int> native_physics2d_raycast;
        public delegate* unmanaged<float, float, float, float, float, float, float, float, uint, uint, IntPtr, int, int> native_physics2d_boxcast;
        public delegate* unmanaged<float, float, float, float, uint, uint, IntPtr, int, int, int> native_physics2d_overlap_aabb;
        public delegate* unmanaged<ulong, ulong, bool, void>                              native_physics2d_ignore_collision;
        public delegate* unmanaged<ulong, IntPtr, int, int, int>                          native_physics2d_get_collider_contacts;
        public delegate* unmanaged<ulong, ulong, IntPtr, int>                             native_physics2d_collider_distance;
        public delegate* unmanaged<ulong, float, float, float, void>                      native_RigidbodyComponent_SetVelocity;
        public delegate* unmanaged<ulong, float, float, float, void>                      native_RigidbodyComponent_ApplyForce;
        public delegate* unmanaged<ulong, float, float, float, void>                      native_RigidbodyComponent_ApplyImpulse;
        public delegate* unmanaged<ulong, float, float, float, float, float, float, float, void> native_RigidbodyComponent_Teleport;
        public delegate* unmanaged<ulong, byte*, void>                                    native_AnimatorComponent_Play;
        public delegate* unmanaged<ulong, void>                                           native_AnimatorComponent_Stop;
        public delegate* unmanaged<ulong, void>                                           native_AnimatorComponent_Resume;
        public delegate* unmanaged<ulong, void>                                           native_AudioSourceComponent_Play;
        public delegate* unmanaged<ulong, void>                                           native_AudioSourceComponent_Stop;
        public delegate* unmanaged<ulong, void>                                           native_AudioSourceComponent_Pause;
        public delegate* unmanaged<ulong, void>                                           native_AudioSourceComponent_UnPause;
        public delegate* unmanaged<ulong, int>                                            native_AudioSourceComponent_IsPlaying;
        // === NATIVE_BINDINGS_GENERATED_START ===
// === GENERATED BINDINGS START (do not edit manually) ===
        public delegate* unmanaged<ulong, int> native_AnimatorComponent_controller_get;
        public delegate* unmanaged<ulong, int, void> native_AnimatorComponent_controller_set;
        public delegate* unmanaged<ulong, float> native_AnimatorComponent_speed_get;
        public delegate* unmanaged<ulong, float, void> native_AnimatorComponent_speed_set;
        public delegate* unmanaged<ulong, bool> native_AnimatorComponent_play_on_awake_get;
        public delegate* unmanaged<ulong, bool, void> native_AnimatorComponent_play_on_awake_set;
        public delegate* unmanaged<ulong, bool> native_AnimationDriveModeComponent_enabled_get;
        public delegate* unmanaged<ulong, bool, void> native_AnimationDriveModeComponent_enabled_set;
        public delegate* unmanaged<ulong, byte*> native_BoneAttachmentComponent_bone_name_get;
        public delegate* unmanaged<ulong, byte*, void> native_BoneAttachmentComponent_bone_name_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_BoneAttachmentComponent_local_offset_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_BoneAttachmentComponent_local_offset_set;
        public delegate* unmanaged<ulong, bool> native_BoneAttachmentComponent_follow_rotation_get;
        public delegate* unmanaged<ulong, bool, void> native_BoneAttachmentComponent_follow_rotation_set;
        public delegate* unmanaged<ulong, float> native_SkyLightComponent_intensity_get;
        public delegate* unmanaged<ulong, float, void> native_SkyLightComponent_intensity_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_BoxColliderComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_BoxColliderComponent_offset_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_BoxColliderComponent_rotation_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_BoxColliderComponent_rotation_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_BoxColliderComponent_size_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_BoxColliderComponent_size_set;
        public delegate* unmanaged<ulong, bool> native_BoxColliderComponent_is_sensor_get;
        public delegate* unmanaged<ulong, bool, void> native_BoxColliderComponent_is_sensor_set;
        public delegate* unmanaged<ulong, uint> native_BoxColliderComponent_layer_get;
        public delegate* unmanaged<ulong, uint, void> native_BoxColliderComponent_layer_set;
        public delegate* unmanaged<ulong, uint> native_BoxColliderComponent_mask_get;
        public delegate* unmanaged<ulong, uint, void> native_BoxColliderComponent_mask_set;
        public delegate* unmanaged<ulong, float> native_BoxColliderComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_BoxColliderComponent_density_set;
        public delegate* unmanaged<ulong, float> native_BoxColliderComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_BoxColliderComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_BoxColliderComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_BoxColliderComponent_restitution_set;
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
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_CapsuleColliderComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_CapsuleColliderComponent_offset_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_CapsuleColliderComponent_rotation_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_CapsuleColliderComponent_rotation_set;
        public delegate* unmanaged<ulong, float> native_CapsuleColliderComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_CapsuleColliderComponent_radius_set;
        public delegate* unmanaged<ulong, float> native_CapsuleColliderComponent_half_height_get;
        public delegate* unmanaged<ulong, float, void> native_CapsuleColliderComponent_half_height_set;
        public delegate* unmanaged<ulong, bool> native_CapsuleColliderComponent_is_sensor_get;
        public delegate* unmanaged<ulong, bool, void> native_CapsuleColliderComponent_is_sensor_set;
        public delegate* unmanaged<ulong, uint> native_CapsuleColliderComponent_layer_get;
        public delegate* unmanaged<ulong, uint, void> native_CapsuleColliderComponent_layer_set;
        public delegate* unmanaged<ulong, uint> native_CapsuleColliderComponent_mask_get;
        public delegate* unmanaged<ulong, uint, void> native_CapsuleColliderComponent_mask_set;
        public delegate* unmanaged<ulong, float> native_CapsuleColliderComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_CapsuleColliderComponent_density_set;
        public delegate* unmanaged<ulong, float> native_CapsuleColliderComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_CapsuleColliderComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_CapsuleColliderComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_CapsuleColliderComponent_restitution_set;
        public delegate* unmanaged<ulong, float> native_CircleRendererComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_CircleRendererComponent_radius_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_CircleRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_CircleRendererComponent_color_set;
        public delegate* unmanaged<ulong, uint> native_CircleRendererComponent_segments_get;
        public delegate* unmanaged<ulong, uint, void> native_CircleRendererComponent_segments_set;
        public delegate* unmanaged<ulong, float> native_CircleRendererComponent_thickness_get;
        public delegate* unmanaged<ulong, float, void> native_CircleRendererComponent_thickness_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_BoxCollider2dComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, void> native_BoxCollider2dComponent_offset_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_BoxCollider2dComponent_size_get;
        public delegate* unmanaged<ulong, float, float, void> native_BoxCollider2dComponent_size_set;
        public delegate* unmanaged<ulong, bool> native_BoxCollider2dComponent_is_sensor_get;
        public delegate* unmanaged<ulong, bool, void> native_BoxCollider2dComponent_is_sensor_set;
        public delegate* unmanaged<ulong, uint> native_BoxCollider2dComponent_layer_get;
        public delegate* unmanaged<ulong, uint, void> native_BoxCollider2dComponent_layer_set;
        public delegate* unmanaged<ulong, uint> native_BoxCollider2dComponent_mask_get;
        public delegate* unmanaged<ulong, uint, void> native_BoxCollider2dComponent_mask_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_density_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_restitution_set;
        public delegate* unmanaged<ulong, float> native_BoxCollider2dComponent_restitution_threshold_get;
        public delegate* unmanaged<ulong, float, void> native_BoxCollider2dComponent_restitution_threshold_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_CircleCollider2dComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, void> native_CircleCollider2dComponent_offset_set;
        public delegate* unmanaged<ulong, float> native_CircleCollider2dComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_CircleCollider2dComponent_radius_set;
        public delegate* unmanaged<ulong, bool> native_CircleCollider2dComponent_is_sensor_get;
        public delegate* unmanaged<ulong, bool, void> native_CircleCollider2dComponent_is_sensor_set;
        public delegate* unmanaged<ulong, uint> native_CircleCollider2dComponent_layer_get;
        public delegate* unmanaged<ulong, uint, void> native_CircleCollider2dComponent_layer_set;
        public delegate* unmanaged<ulong, uint> native_CircleCollider2dComponent_mask_get;
        public delegate* unmanaged<ulong, uint, void> native_CircleCollider2dComponent_mask_set;
        public delegate* unmanaged<ulong, float> native_CircleCollider2dComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_CircleCollider2dComponent_density_set;
        public delegate* unmanaged<ulong, float> native_CircleCollider2dComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_CircleCollider2dComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_CircleCollider2dComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_CircleCollider2dComponent_restitution_set;
        public delegate* unmanaged<ulong, float> native_CircleCollider2dComponent_restitution_threshold_get;
        public delegate* unmanaged<ulong, float, void> native_CircleCollider2dComponent_restitution_threshold_set;
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
        public delegate* unmanaged<ulong, int> native_FoliageRendererComponent_mesh_get;
        public delegate* unmanaged<ulong, int, void> native_FoliageRendererComponent_mesh_set;
        public delegate* unmanaged<ulong, bool> native_FoliageRendererComponent_visible_get;
        public delegate* unmanaged<ulong, bool, void> native_FoliageRendererComponent_visible_set;
        public delegate* unmanaged<ulong, bool> native_FoliageRendererComponent_cast_shadow_get;
        public delegate* unmanaged<ulong, bool, void> native_FoliageRendererComponent_cast_shadow_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_FoliageRendererComponent_instance_bounds_extent_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_FoliageRendererComponent_instance_bounds_extent_set;
        public delegate* unmanaged<ulong, ulong> native_DistanceJoint2dComponent_target_entity_get;
        public delegate* unmanaged<ulong, ulong, void> native_DistanceJoint2dComponent_target_entity_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_DistanceJoint2dComponent_local_anchor_a_get;
        public delegate* unmanaged<ulong, float, float, void> native_DistanceJoint2dComponent_local_anchor_a_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_DistanceJoint2dComponent_local_anchor_b_get;
        public delegate* unmanaged<ulong, float, float, void> native_DistanceJoint2dComponent_local_anchor_b_set;
        public delegate* unmanaged<ulong, float> native_DistanceJoint2dComponent_length_get;
        public delegate* unmanaged<ulong, float, void> native_DistanceJoint2dComponent_length_set;
        public delegate* unmanaged<ulong, float> native_DistanceJoint2dComponent_frequency_get;
        public delegate* unmanaged<ulong, float, void> native_DistanceJoint2dComponent_frequency_set;
        public delegate* unmanaged<ulong, float> native_DistanceJoint2dComponent_damping_ratio_get;
        public delegate* unmanaged<ulong, float, void> native_DistanceJoint2dComponent_damping_ratio_set;
        public delegate* unmanaged<ulong, ulong> native_RevoluteJoint2dComponent_target_entity_get;
        public delegate* unmanaged<ulong, ulong, void> native_RevoluteJoint2dComponent_target_entity_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_RevoluteJoint2dComponent_local_anchor_a_get;
        public delegate* unmanaged<ulong, float, float, void> native_RevoluteJoint2dComponent_local_anchor_a_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_RevoluteJoint2dComponent_local_anchor_b_get;
        public delegate* unmanaged<ulong, float, float, void> native_RevoluteJoint2dComponent_local_anchor_b_set;
        public delegate* unmanaged<ulong, bool> native_RevoluteJoint2dComponent_enable_limit_get;
        public delegate* unmanaged<ulong, bool, void> native_RevoluteJoint2dComponent_enable_limit_set;
        public delegate* unmanaged<ulong, float> native_RevoluteJoint2dComponent_lower_angle_get;
        public delegate* unmanaged<ulong, float, void> native_RevoluteJoint2dComponent_lower_angle_set;
        public delegate* unmanaged<ulong, float> native_RevoluteJoint2dComponent_upper_angle_get;
        public delegate* unmanaged<ulong, float, void> native_RevoluteJoint2dComponent_upper_angle_set;
        public delegate* unmanaged<ulong, bool> native_RevoluteJoint2dComponent_enable_motor_get;
        public delegate* unmanaged<ulong, bool, void> native_RevoluteJoint2dComponent_enable_motor_set;
        public delegate* unmanaged<ulong, float> native_RevoluteJoint2dComponent_motor_speed_get;
        public delegate* unmanaged<ulong, float, void> native_RevoluteJoint2dComponent_motor_speed_set;
        public delegate* unmanaged<ulong, float> native_RevoluteJoint2dComponent_max_motor_torque_get;
        public delegate* unmanaged<ulong, float, void> native_RevoluteJoint2dComponent_max_motor_torque_set;
        public delegate* unmanaged<ulong, float> native_Rigidbody2dComponent_gravity_scale_get;
        public delegate* unmanaged<ulong, float, void> native_Rigidbody2dComponent_gravity_scale_set;
        public delegate* unmanaged<ulong, bool> native_Rigidbody2dComponent_fixed_rotation_get;
        public delegate* unmanaged<ulong, bool, void> native_Rigidbody2dComponent_fixed_rotation_set;
        public delegate* unmanaged<ulong, int> native_MeshRendererComponent_mesh_get;
        public delegate* unmanaged<ulong, int, void> native_MeshRendererComponent_mesh_set;
        public delegate* unmanaged<ulong, int> native_MeshRendererComponent_section_index_get;
        public delegate* unmanaged<ulong, int, void> native_MeshRendererComponent_section_index_set;
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
        public delegate* unmanaged<ulong, float> native_RigidbodyComponent_gravity_scale_get;
        public delegate* unmanaged<ulong, float, void> native_RigidbodyComponent_gravity_scale_set;
        public delegate* unmanaged<ulong, float> native_RigidbodyComponent_linear_damping_get;
        public delegate* unmanaged<ulong, float, void> native_RigidbodyComponent_linear_damping_set;
        public delegate* unmanaged<ulong, float> native_RigidbodyComponent_angular_damping_get;
        public delegate* unmanaged<ulong, float, void> native_RigidbodyComponent_angular_damping_set;
        public delegate* unmanaged<ulong, float> native_RigidbodyComponent_mass_override_get;
        public delegate* unmanaged<ulong, float, void> native_RigidbodyComponent_mass_override_set;
        public delegate* unmanaged<ulong, bool> native_RigidbodyComponent_lock_rotation_get;
        public delegate* unmanaged<ulong, bool, void> native_RigidbodyComponent_lock_rotation_set;
        public delegate* unmanaged<ulong, bool> native_RigidbodyComponent_is_bullet_get;
        public delegate* unmanaged<ulong, bool, void> native_RigidbodyComponent_is_bullet_set;
        public delegate* unmanaged<ulong, bool> native_RigidbodyComponent_enabled_get;
        public delegate* unmanaged<ulong, bool, void> native_RigidbodyComponent_enabled_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_SphereColliderComponent_offset_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_SphereColliderComponent_offset_set;
        public delegate* unmanaged<ulong, float*, float*, float*, void> native_SphereColliderComponent_rotation_get;
        public delegate* unmanaged<ulong, float, float, float, void> native_SphereColliderComponent_rotation_set;
        public delegate* unmanaged<ulong, float> native_SphereColliderComponent_radius_get;
        public delegate* unmanaged<ulong, float, void> native_SphereColliderComponent_radius_set;
        public delegate* unmanaged<ulong, bool> native_SphereColliderComponent_is_sensor_get;
        public delegate* unmanaged<ulong, bool, void> native_SphereColliderComponent_is_sensor_set;
        public delegate* unmanaged<ulong, uint> native_SphereColliderComponent_layer_get;
        public delegate* unmanaged<ulong, uint, void> native_SphereColliderComponent_layer_set;
        public delegate* unmanaged<ulong, uint> native_SphereColliderComponent_mask_get;
        public delegate* unmanaged<ulong, uint, void> native_SphereColliderComponent_mask_set;
        public delegate* unmanaged<ulong, float> native_SphereColliderComponent_density_get;
        public delegate* unmanaged<ulong, float, void> native_SphereColliderComponent_density_set;
        public delegate* unmanaged<ulong, float> native_SphereColliderComponent_friction_get;
        public delegate* unmanaged<ulong, float, void> native_SphereColliderComponent_friction_set;
        public delegate* unmanaged<ulong, float> native_SphereColliderComponent_restitution_get;
        public delegate* unmanaged<ulong, float, void> native_SphereColliderComponent_restitution_set;
        public delegate* unmanaged<ulong, float*, float*, void> native_LineRendererComponent_direction_get;
        public delegate* unmanaged<ulong, float, float, void> native_LineRendererComponent_direction_set;
        public delegate* unmanaged<ulong, float> native_LineRendererComponent_length_get;
        public delegate* unmanaged<ulong, float, void> native_LineRendererComponent_length_set;
        public delegate* unmanaged<ulong, float> native_LineRendererComponent_thickness_get;
        public delegate* unmanaged<ulong, float, void> native_LineRendererComponent_thickness_set;
        public delegate* unmanaged<ulong, float*, float*, float*, float*, void> native_LineRendererComponent_color_get;
        public delegate* unmanaged<ulong, float, float, float, float, void> native_LineRendererComponent_color_set;
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
        public delegate* unmanaged<int, int, int>                                         native_object_is_alive;
        public delegate* unmanaged<int, int>                                              native_object_get_generation;
        public delegate* unmanaged<byte*, int>                                            native_texture_load;
        public delegate* unmanaged<byte*, int>                                            native_sprite_load;
        public delegate* unmanaged<byte*, byte*, int>                                     native_load_object;
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
        var name = ComponentManager.GetNativeTypeName(componentType);
        var ptr = StrToPtr(name);
        try { return b->native_entity_has_component(entityId, ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityAddComponent(ulong entityId, CakeComponent component) { }

    internal static void Native_EntityAddComponent(ulong entityId, Type componentType)
    {
        var name = ComponentManager.GetNativeTypeName(componentType);
        var ptr = StrToPtr(name);
        try { b->native_entity_add_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityRemoveComponent(ulong entityId, Type componentType)
    {
        var name = ComponentManager.GetNativeTypeName(componentType);
        var ptr = StrToPtr(name);
        try { b->native_entity_remove_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputRegisterActionMap(string mapName, int priority)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_register_action_map(ptr, priority) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputSetActionMapEnabled(string mapName, bool enabled)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_set_action_map_enabled(ptr, enabled ? 1 : 0) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputSetActionMapConsume(string mapName, bool consume)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_set_action_map_consume(ptr, consume ? 1 : 0) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputPushContext(string mapName)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_push_context(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputPopContext(string mapName)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_pop_context(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputRegisterAction(string mapName, string actionName, int valueType)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_register_action(mapPtr, actionPtr, valueType) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputBindKey(string mapName, string actionName, KeyCode key, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_key(mapPtr, actionPtr, (int)key, scale) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputUnregisterActionMap(string mapName)
    {
        var ptr = StrToPtr(mapName);
        try { return b->native_input_unregister_action_map(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputBindKey2D(string mapName, string actionName, KeyCode key, float x, float y)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_key2d(mapPtr, actionPtr, (int)key, x, y) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputBindMouseButton(string mapName, string actionName, MouseCode button, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_mouse_button(mapPtr, actionPtr, (int)button, scale) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputBindMouseDelta(string mapName, string actionName, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_mouse_delta(mapPtr, actionPtr, scale) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputBindMouseWheel(string mapName, string actionName, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_mouse_wheel(mapPtr, actionPtr, scale) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static Vector2f Native_InputGetMousePosition()
    {
        float x = 0.0f, y = 0.0f;
        b->native_input_get_mouse_position(&x, &y);
        return new Vector2f(x, y);
    }

    internal static Vector2f Native_InputGetMouseDelta()
    {
        float x = 0.0f, y = 0.0f;
        b->native_input_get_mouse_delta(&x, &y);
        return new Vector2f(x, y);
    }

    internal static Vector2f Native_InputGetMouseWheel()
    {
        float x = 0.0f, y = 0.0f;
        b->native_input_get_mouse_wheel(&x, &y);
        return new Vector2f(x, y);
    }

    internal static bool Native_InputIsActionDown(string actionName)
    {
        var ptr = StrToPtr(actionName);
        try { return b->native_input_is_action_down(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputWasActionPressed(string actionName)
    {
        var ptr = StrToPtr(actionName);
        try { return b->native_input_was_action_pressed(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputWasActionReleased(string actionName)
    {
        var ptr = StrToPtr(actionName);
        try { return b->native_input_was_action_released(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static float Native_InputGetActionAxis(string actionName)
    {
        var ptr = StrToPtr(actionName);
        try { return b->native_input_get_action_axis(ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static Vector2f Native_InputGetActionVector2(string actionName)
    {
        var ptr = StrToPtr(actionName);
        try
        {
            float x = 0.0f, y = 0.0f;
            b->native_input_get_action_vector2(ptr, &x, &y);
            return new Vector2f(x, y);
        }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputSetBindingInteraction(string mapName, string actionName, int interaction, float holdSeconds)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_set_binding_interaction(mapPtr, actionPtr, interaction, holdSeconds) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
        }
    }

    internal static bool Native_InputLoadActionAsset(string path)
    {
        var ptr = StrToPtr(path);
        try { return b->native_input_load_action_asset(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputBindGamepadButton(string mapName, string actionName, int button, uint deviceId, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_gamepad_button(mapPtr, actionPtr, button, deviceId, scale) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputBindGamepadAxis(string mapName, string actionName, int axis, uint deviceId, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_gamepad_axis(mapPtr, actionPtr, axis, deviceId, scale) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputBindGamepadStick(string mapName, string actionName, int stickAxis, uint deviceId, float scale)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_bind_gamepad_stick(mapPtr, actionPtr, stickAxis, deviceId, scale) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputBindComposite(string mapName, string actionName, string partsJson, uint deviceId)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        var jsonPtr = StrToPtr(partsJson);
        try { return b->native_input_bind_composite(mapPtr, actionPtr, jsonPtr, deviceId) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
            Marshal.FreeCoTaskMem((IntPtr)jsonPtr);
        }
    }

    internal static bool Native_InputSetBindingTapParams(string mapName, string actionName, int bindingIndex, int tapCount, float tapWindow)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_set_binding_tap_params(mapPtr, actionPtr, bindingIndex, tapCount, tapWindow) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputSetBindingRepeatParams(string mapName, string actionName, int bindingIndex, float repeatDelay, float repeatRate)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_set_binding_repeat_params(mapPtr, actionPtr, bindingIndex, repeatDelay, repeatRate) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputSetBindingProcessor(string mapName, string actionName, int bindingIndex, int type, float a, float bVal)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_set_binding_processor(mapPtr, actionPtr, bindingIndex, type, a, bVal) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static uint Native_InputFindActionId(string mapName, string actionName)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_find_action_id(mapPtr, actionPtr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static uint Native_InputFindActionId(string qualifiedName)
    {
        var ptr = StrToPtr(qualifiedName);
        try { return b->native_input_find_action_id_q(ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputIsActionDown(uint actionId) => b->native_input_is_action_down_id(actionId) != 0;

    internal static bool Native_InputWasActionPressed(uint actionId) => b->native_input_was_action_pressed_id(actionId) != 0;

    internal static bool Native_InputWasActionReleased(uint actionId) => b->native_input_was_action_released_id(actionId) != 0;

    internal static float Native_InputGetActionAxis(uint actionId) => b->native_input_get_action_axis_id(actionId);

    internal static Vector2f Native_InputGetActionVector2(uint actionId)
    {
        float x = 0.0f, y = 0.0f;
        b->native_input_get_action_vector2_id(actionId, &x, &y);
        return new Vector2f(x, y);
    }

    internal static ulong Native_InputSubscribe(string actionName, int phase)
    {
        var ptr = StrToPtr(actionName);
        try { return b->native_input_subscribe(ptr, phase); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static ulong Native_InputSubscribe(uint actionId, int phase) => b->native_input_subscribe_id(actionId, phase);

    internal static void Native_InputUnsubscribe(ulong subscriptionId) => b->native_input_unsubscribe(subscriptionId);

    internal static bool Native_InputSetBindingOverride(string mapName, string actionName, int bindingIndex, string bindingJson)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        var jsonPtr = StrToPtr(bindingJson);
        try { return b->native_input_set_binding_override(mapPtr, actionPtr, bindingIndex, jsonPtr) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)mapPtr);
            Marshal.FreeCoTaskMem((IntPtr)actionPtr);
            Marshal.FreeCoTaskMem((IntPtr)jsonPtr);
        }
    }

    internal static bool Native_InputClearBindingOverride(string mapName, string actionName, int bindingIndex)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_clear_binding_override(mapPtr, actionPtr, bindingIndex) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static bool Native_InputBeginRebindSession(string mapName, string actionName, int bindingIndex)
    {
        var mapPtr = StrToPtr(mapName);
        var actionPtr = StrToPtr(actionName);
        try { return b->native_input_begin_rebind_session(mapPtr, actionPtr, bindingIndex) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)mapPtr); Marshal.FreeCoTaskMem((IntPtr)actionPtr); }
    }

    internal static void Native_InputCancelRebindSession() => b->native_input_cancel_rebind_session();

    internal static bool Native_InputIsRebindSessionActive() => b->native_input_is_rebind_session_active() != 0;

    internal static bool Native_InputLoadConfigOverrides(string projectPath, string userPath)
    {
        var projectPtr = StrToPtr(projectPath);
        var userPtr = StrToPtr(userPath);
        try { return b->native_input_load_config_overrides(projectPtr, userPtr) != 0; }
        finally
        {
            Marshal.FreeCoTaskMem((IntPtr)projectPtr);
            Marshal.FreeCoTaskMem((IntPtr)userPtr);
        }
    }

    internal static bool Native_InputSaveUserConfigOverrides(string userPath)
    {
        var ptr = StrToPtr(userPath);
        try { return b->native_input_save_user_config_overrides(ptr) != 0; }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static bool Native_InputIsGamepadConnected(uint deviceId) => b->native_input_is_gamepad_connected(deviceId) != 0;

    internal static bool Native_InputIsGamepadButtonDown(uint deviceId, int button) => b->native_input_is_gamepad_button_down(deviceId, button) != 0;

    internal static bool Native_InputIsGamepadButtonPressed(uint deviceId, int button) => b->native_input_is_gamepad_button_pressed(deviceId, button) != 0;

    internal static bool Native_InputIsGamepadButtonReleased(uint deviceId, int button) => b->native_input_is_gamepad_button_released(deviceId, button) != 0;

    internal static float Native_InputGetGamepadAxis(uint deviceId, int axis) => b->native_input_get_gamepad_axis(deviceId, axis);

    internal static bool Native_ComponentExists(ulong entityId, Type componentType)
    {
        var name = ComponentManager.GetNativeTypeName(componentType);
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

    internal static void Native_EntityEnqueueDestroy(ulong entityId) => b->native_entity_enqueue_destroy(entityId);

    internal static void Native_EntityEnqueueAddComponent(ulong entityId, string typeName)
    {
        var ptr = StrToPtr(typeName);
        try { b->native_entity_enqueue_add_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityEnqueueRemoveComponent(ulong entityId, string typeName)
    {
        var ptr = StrToPtr(typeName);
        try { b->native_entity_enqueue_remove_component(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityEnqueueAddManaged(ulong entityId, string typeName)
    {
        var ptr = StrToPtr(typeName);
        try { b->native_entity_enqueue_add_managed(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_EntityEnqueueRemoveManaged(ulong entityId, string typeName)
    {
        var ptr = StrToPtr(typeName);
        try { b->native_entity_enqueue_remove_managed(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_Rigidbody2D_SetVelocity(ulong entityId, float x, float y) => b->native_Rigidbody2dComponent_SetVelocity(entityId, x, y);

    internal static void Native_Rigidbody2D_ApplyForce(ulong entityId, float x, float y) => b->native_Rigidbody2dComponent_ApplyForce(entityId, x, y);

    internal static void Native_Rigidbody2D_ApplyImpulse(ulong entityId, float x, float y) => b->native_Rigidbody2dComponent_ApplyImpulse(entityId, x, y);

    internal static void Native_Rigidbody_SetVelocity(ulong entityId, float x, float y, float z) => b->native_RigidbodyComponent_SetVelocity(entityId, x, y, z);

    internal static void Native_Rigidbody_ApplyForce(ulong entityId, float x, float y, float z) => b->native_RigidbodyComponent_ApplyForce(entityId, x, y, z);

    internal static void Native_Rigidbody_ApplyImpulse(ulong entityId, float x, float y, float z) => b->native_RigidbodyComponent_ApplyImpulse(entityId, x, y, z);

    internal static void Native_Rigidbody_Teleport(ulong entityId, float px, float py, float pz, float qx, float qy, float qz, float qw) => b->native_RigidbodyComponent_Teleport(entityId, px, py, pz, qx, qy, qz, qw);

    internal static void Native_Animator_Play(ulong entityId, string name)
    {
        var ptr = StrToPtr(name);
        try { b->native_AnimatorComponent_Play(entityId, ptr); }
        finally { Marshal.FreeCoTaskMem((IntPtr)ptr); }
    }

    internal static void Native_Animator_Stop(ulong entityId) => b->native_AnimatorComponent_Stop(entityId);

    internal static void Native_Animator_Resume(ulong entityId) => b->native_AnimatorComponent_Resume(entityId);

    internal static void Native_AudioSource_Play(ulong entityId)
    {
        if (b->native_AudioSourceComponent_Play != null) b->native_AudioSourceComponent_Play(entityId);
    }

    internal static void Native_AudioSource_Stop(ulong entityId)
    {
        if (b->native_AudioSourceComponent_Stop != null) b->native_AudioSourceComponent_Stop(entityId);
    }

    internal static void Native_AudioSource_Pause(ulong entityId)
    {
        if (b->native_AudioSourceComponent_Pause != null) b->native_AudioSourceComponent_Pause(entityId);
    }

    internal static void Native_AudioSource_UnPause(ulong entityId)
    {
        if (b->native_AudioSourceComponent_UnPause != null) b->native_AudioSourceComponent_UnPause(entityId);
    }

    internal static bool Native_AudioSource_IsPlaying(ulong entityId)
    {
        return b->native_AudioSourceComponent_IsPlaying != null &&
               b->native_AudioSourceComponent_IsPlaying(entityId) != 0;
    }

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

    internal static int Rigidbody2dComponent_Type_Get(ulong entityId)
    {
        var f = b->native_Rigidbody2dComponent_type_get;
        return f != null ? f(entityId) : 0;
    }
    internal static void Rigidbody2dComponent_Type_Set(ulong entityId, int value)
    { if (b->native_Rigidbody2dComponent_type_set != null) b->native_Rigidbody2dComponent_type_set(entityId, value); }

    internal static Vector2f Rigidbody2dComponent_Velocity_Get(ulong entityId)
    {
        float x = 0, y = 0;
        var f = b->native_Rigidbody2dComponent_velocity_get;
        if (f != null) f(entityId, &x, &y);
        return new Vector2f(x, y);
    }
    internal static void Rigidbody2dComponent_Velocity_Set(ulong entityId, Vector2f value)
    {
        Rigidbody2dComponent_SetVelocity(entityId, value.x, value.y);
    }
    internal static void Rigidbody2dComponent_SetVelocity(ulong entityId, float vx, float vy)
    {
        var f = b->native_Rigidbody2dComponent_SetVelocity;
        if (f != null) f(entityId, vx, vy);
    }
    internal static void Rigidbody2dComponent_ApplyForce(ulong entityId, float fx, float fy)
    {
        var f = b->native_Rigidbody2dComponent_ApplyForce;
        if (f != null) f(entityId, fx, fy);
    }
    internal static void Rigidbody2dComponent_ApplyImpulse(ulong entityId, float fx, float fy)
    {
        var f = b->native_Rigidbody2dComponent_ApplyImpulse;
        if (f != null) f(entityId, fx, fy);
    }

    internal static void Rigidbody2dComponent_MovePosition(ulong entityId, float x, float y)
    {
        var f = b->native_Rigidbody2dComponent_move_position;
        if (f != null) { f(entityId, x, y); return; }
        var t = b->native_TransformComponent_position_set;
        if (t != null) t(entityId, x, y, 0f);
    }

    internal static float Native_Time_GetFixedDeltaTime()
    {
        var f = b->native_time_get_fixed_delta_time;
        return f != null ? f() : 1f / 60f;
    }

    internal static int Physics2d_PollEventCount()
    {
        var f = b->native_physics2d_poll_event_count;
        return f != null ? f() : 0;
    }
    internal static unsafe int Physics2d_GetEvent(int idx, float* out9)
    {
        var f = b->native_physics2d_get_event;
        return f != null ? f(idx, (IntPtr)out9) : 0;
    }
    internal static unsafe int Physics2d_Raycast(float ox, float oy, float dx, float dy, float md,
                                                  uint layer, uint mask, float minFrac, float* outHits8, int cap)
    {
        var f = b->native_physics2d_raycast;
        return f != null ? f(ox, oy, dx, dy, md, layer, mask, minFrac, (IntPtr)outHits8, cap) : 0;
    }
    internal static unsafe int Physics2d_BoxCast(float cx, float cy, float hx, float hy, float ang,
                                                  float dx, float dy, float md, uint layer, uint mask,
                                                  float* outHits8, int cap)
    {
        var f = b->native_physics2d_boxcast;
        return f != null ? f(cx, cy, hx, hy, ang, dx, dy, md, layer, mask, (IntPtr)outHits8, cap) : 0;
    }
    internal static unsafe int Physics2d_OverlapAABB(float cx, float cy, float hx, float hy,
                                                      uint layer, uint mask, uint* outIds, int cap, int strideWords)
    {
        var f = b->native_physics2d_overlap_aabb;
        return f != null ? f(cx, cy, hx, hy, layer, mask, (IntPtr)outIds, cap, strideWords) : 0;
    }
    internal static void Physics2d_IgnoreCollision(ulong a, ulong entityB, bool ignore)
    {
        var f = b->native_physics2d_ignore_collision;
        if (f != null) f(a, entityB, ignore);
    }
    internal static unsafe int Physics2d_GetColliderContacts(ulong e, uint* outIds, int cap, int strideWords)
    {
        var f = b->native_physics2d_get_collider_contacts;
        return f != null ? f(e, (IntPtr)outIds, cap, strideWords) : 0;
    }
    internal static unsafe int Physics2d_ColliderDistance(ulong a, ulong entityB, float* outDist)
    {
        var f = b->native_physics2d_collider_distance;
        return f != null ? f(a, entityB, (IntPtr)outDist) : 0;
    }
    internal static bool SpriteRendererComponent_Visible_Get(ulong entityId)
    {
        var f = b->native_spriterenderercomponent_visible_get;
        return f != null && f(entityId);
    }
    internal static void SpriteRendererComponent_Visible_Set(ulong entityId, bool value)
    {
        var f = b->native_spriterenderercomponent_visible_set;
        if (f != null) f(entityId, value);
    }
}
