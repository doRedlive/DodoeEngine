// do@Redlive
// C API Facade — parsed by SWIG to generate C# P/Invoke bindings.
// Rule: use only C-compatible types. No C++ implementation details.

#pragma once

#include <cstdint>

// ============================================================
// Platform export macro (skipped by SWIG)
// ============================================================
#ifndef SWIG
    #ifdef _WIN32
        #ifdef DODOE_BUILD_DLL
            #define DODOE_API __declspec(dllexport)
        #else
            #define DODOE_API __declspec(dllimport)
        #endif
    #else
        #define DODOE_API __attribute__((visibility("default")))
    #endif
#endif

// ============================================================
// C-compatible types (mirroring engine C++ types)
// ============================================================

typedef uint64_t DodoeHandle;       // ECS entity handle
typedef uint64_t DodoeTextureId;    // texture instance ID
typedef uint64_t DodoeAssetId;      // asset file ID

typedef struct { float x, y; }      DodoeVec2;
typedef struct { float x, y, z; }   DodoeVec3;
typedef struct { float r, g, b, a; } DodoeColor;

typedef struct {
    DodoeAssetId   id;
    DodoeTextureId path_id;
    const char*    path;
    const char*    type;             // "texture", "model", "scene", ...
} DodoeAssetRef;

typedef struct {
    DodoeTextureId id;
    const char*    path;
    int            width;
    int            height;
} DodoeTextureInfo;

// ============================================================
// API
// ============================================================
#ifdef __cplusplus
extern "C" {
#endif

// --- Context lifecycle ---
DODOE_API void*       dodoe_context_create(void);
DODOE_API void        dodoe_context_destroy(void* ctx);
DODOE_API void        dodoe_context_tick(void* ctx);
DODOE_API const char* dodoe_get_version(void);
DODOE_API void*       dodoe_get_native_window(void* ctx);
DODOE_API void        dodoe_show_window(void* ctx, bool show);

// --- ECS Entity ---
DODOE_API DodoeHandle dodoe_entity_create(void* ctx);
DODOE_API void        dodoe_entity_destroy(void* ctx, DodoeHandle entity);

DODOE_API void        dodoe_entity_set_name(void* ctx, DodoeHandle entity, const char* name);
DODOE_API const char* dodoe_entity_get_name(void* ctx, DodoeHandle entity);

DODOE_API void        dodoe_entity_set_position(void* ctx, DodoeHandle entity, DodoeVec2 pos);
DODOE_API DodoeVec2   dodoe_entity_get_position(void* ctx, DodoeHandle entity);
DODOE_API void        dodoe_entity_set_scale(void* ctx, DodoeHandle entity, DodoeVec2 scale);
DODOE_API DodoeVec2   dodoe_entity_get_scale(void* ctx, DodoeHandle entity);
DODOE_API void        dodoe_entity_set_rotation(void* ctx, DodoeHandle entity, float radians);
DODOE_API float       dodoe_entity_get_rotation(void* ctx, DodoeHandle entity);

DODOE_API int         dodoe_entity_get_child_count(void* ctx, DodoeHandle parent);
DODOE_API DodoeHandle dodoe_entity_get_child_at(void* ctx, DodoeHandle parent, int index);
DODOE_API void        dodoe_entity_set_parent(void* ctx, DodoeHandle child, DodoeHandle parent);

DODOE_API int         dodoe_entity_has_component(void* ctx, DodoeHandle entity, const char* component_type);
DODOE_API void        dodoe_entity_add_component(void* ctx, DodoeHandle entity, const char* component_type);
DODOE_API void        dodoe_entity_remove_component(void* ctx, DodoeHandle entity, const char* component_type);
DODOE_API int         dodoe_entity_get_component_count(void* ctx, DodoeHandle entity);
DODOE_API const char* dodoe_entity_get_component_type(void* ctx, DodoeHandle entity, int index);

// --- Textures ---
DODOE_API DodoeTextureId   dodoe_texture_load(void* ctx, const char* path);
DODOE_API DodoeTextureInfo dodoe_texture_get_info(void* ctx, DodoeTextureId id);
DODOE_API int              dodoe_texture_get_loaded_count(void* ctx);

// --- Assets ---
DODOE_API int           dodoe_asset_get_count(void* ctx, const char* asset_type);
DODOE_API DodoeAssetRef dodoe_asset_get_at(void* ctx, const char* asset_type, int index);

// --- Editor selection ---
DODOE_API void        dodoe_editor_select_entity(void* ctx, DodoeHandle entity);
DODOE_API DodoeHandle dodoe_editor_get_selected_entity(void* ctx);

// --- World state (Play/Pause/Stop) ---
DODOE_API void dodoe_world_set_state(void* ctx, int state);  // 0=Simulation, 1=Runtime, 2=Pause

// --- Viewport (Avalonia NativeControlHost) ---
DODOE_API void dodoe_viewport_attach(void* ctx, void* native_handle, int width, int height);
DODOE_API void dodoe_viewport_resize(void* ctx, int width, int height);
DODOE_API void dodoe_viewport_detach(void* ctx);

#ifdef __cplusplus
}
#endif
