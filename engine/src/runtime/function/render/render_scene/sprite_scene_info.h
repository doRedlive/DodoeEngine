// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/pixel2d/sprite.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    class RenderObject;

    struct alignas(16) SpriteInstance {
        Float position_x{0.0f};
        Float position_y{0.0f};
        Float scale_x{1.0f};
        Float scale_y{1.0f};
        Float rotation{0.0f};
        Float _pad0{0.0f};
        UInt32 atlas_index{0};
        UInt32 _pad1{0};
        Float uv_min_x{0.0f};
        Float uv_min_y{0.0f};
        Float uv_max_x{1.0f};
        Float uv_max_y{1.0f};
        UInt32 color{0xFFFFFFFF};
        UInt32 sorting_key{0};
        UInt32 material_id{0};
        UInt32 flags{0};
    };

    static_assert(sizeof(SpriteInstance) == 64, "SpriteInstance must be 64 bytes");
    static_assert(alignof(SpriteInstance) == 16, "SpriteInstance must be 16-byte aligned");

    constexpr UInt32 kSpriteFlagFlipX        = 1 << 0;
    constexpr UInt32 kSpriteFlagFlipY        = 1 << 1;
    constexpr UInt32 kSpriteFlagHasNormalMap = 1 << 2;
    constexpr UInt32 kSpriteFlagCastShadow   = 1 << 3;
    constexpr UInt32 kSpriteFlagIsOpaque     = 1 << 4;

    struct QuadVertex {
        Float px, py, pz;
        Float u, v;
        UInt32 color;
        UInt32 tex_index;
    };

    inline constexpr QuadVertex kQuadVertices[] = {
        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0xFFFFFFFF, 0},
        { 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0xFFFFFFFF, 0},
        { 0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0xFFFFFFFF, 0},
        {-0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0xFFFFFFFF, 0},
    };

    constexpr UInt32 kQuadVertexStride = sizeof(QuadVertex);
    constexpr UInt32 kQuadVertexCount = 4;

    inline constexpr UInt16 kQuadIndices[] = {0, 1, 2, 2, 3, 0};

    class SpriteSceneInfo {
    public:
        SpriteSceneInfo() = default;
        explicit SpriteSceneInfo(Identifier id);

        void setRenderObject(const RenderObject* render_object) { m_render_object = render_object; }
        void setWorldTransform(const Matrix4f& world_transform) { m_world_transform = world_transform; }
        void setPosition(const Vector2f& position) { m_position = position; }
        void setScale(const Vector2f& scale) { m_scale = scale; }
        void setRotation(Float rotation) { m_rotation = rotation; }
        void setColor(UInt32 color) { m_color = color; }
        void setSortingKey(UInt32 sorting_key) { m_sorting_key = sorting_key; }
        void setMaterialId(UInt32 material_id) { m_material_id = material_id; }
        void setFlags(UInt32 flags) { m_flags = flags; }
        void setVisible(Bool visible) { m_visible = visible; }

        void setBatchInstances(DynamicArray<SpriteInstance> instances) {
            m_instances = create_scope<DynamicArray<SpriteInstance>>(std::move(instances));
        }
        void setBounds(const Vector3f& center, const Vector3f& extents) {
            m_bounds_center = center;
            m_bounds_extents = extents;
        }

        void setSprite(const PPtr<Sprite>& sprite);
        void setAtlasIndex(UInt32 index) { m_atlas_index = index; }
        void setUVRect(Float min_x, Float min_y, Float max_x, Float max_y);

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const RenderObject* getRenderObject() const { return m_render_object; }
        [[nodiscard]] const Matrix4f& getWorldTransform() const { return m_world_transform; }
        [[nodiscard]] const Vector2f& getPosition() const { return m_position; }
        [[nodiscard]] const Vector2f& getScale() const { return m_scale; }
        [[nodiscard]] Float getRotation() const { return m_rotation; }
        [[nodiscard]] UInt32 getColor() const { return m_color; }
        [[nodiscard]] UInt32 getSortingKey() const { return m_sorting_key; }
        [[nodiscard]] UInt32 getMaterialId() const { return m_material_id; }
        [[nodiscard]] UInt32 getFlags() const { return m_flags; }
        [[nodiscard]] const PPtr<Sprite>& getSprite() const { return m_sprite; }
        [[nodiscard]] UInt32 getAtlasIndex() const { return m_atlas_index; }
        [[nodiscard]] Float getUVMinX() const { return m_uv_min_x; }
        [[nodiscard]] Float getUVMinY() const { return m_uv_min_y; }
        [[nodiscard]] Float getUVMaxX() const { return m_uv_max_x; }
        [[nodiscard]] Float getUVMaxY() const { return m_uv_max_y; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }

        [[nodiscard]] Bool hasInstances() const { return m_instances != nullptr; }
        [[nodiscard]] const DynamicArray<SpriteInstance>& getInstances() const { return *m_instances; }
        [[nodiscard]] const Vector3f& getBoundsCenter() const { return m_bounds_center; }
        [[nodiscard]] const Vector3f& getBoundsExtents() const { return m_bounds_extents; }

        [[nodiscard]] SpriteInstance toInstance() const;

    private:
        Identifier m_id{};
        const RenderObject* m_render_object{nullptr};
        Matrix4f m_world_transform{1.0f};
        Vector2f m_position{0.0f};
        Vector2f m_scale{1.0f};
        Float m_rotation{0.0f};
        UInt32 m_color{0xFFFFFFFF};
        UInt32 m_sorting_key{0};
        UInt32 m_material_id{0};
        UInt32 m_flags{0};
        PPtr<Sprite> m_sprite{};
        Float m_uv_min_x{0.0f};
        Float m_uv_min_y{0.0f};
        Float m_uv_max_x{1.0f};
        Float m_uv_max_y{1.0f};
        UInt32 m_atlas_index{0};
        Bool m_visible{true};

        // 批量模式数据（Scope 间接持有，单 sprite 场景零开销）
        Scope<DynamicArray<SpriteInstance>> m_instances{};
        Vector3f m_bounds_center{0.0f};
        Vector3f m_bounds_extents{0.0f};
    };

} // namespace dodoe
