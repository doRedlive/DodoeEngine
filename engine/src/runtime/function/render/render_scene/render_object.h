#pragma once

#include "dopch.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    enum class RenderObjectType : UInt8 {
        StaticMesh,
        Foliage,
        PointLight,
        SpotLight,
        SkyLight,
        Sprite
    };

    enum class RenderObjectDirtyFlags : UInt32 {
        None = 0,
        Mesh = 1 << 0,
        Materials = 1 << 1,
        State = 1 << 2,
        ProxyData = 1 << 3,
        All = Mesh | Materials | State | ProxyData
    };

    inline RenderObjectDirtyFlags operator|(const RenderObjectDirtyFlags lhs, const RenderObjectDirtyFlags rhs) {
        return static_cast<RenderObjectDirtyFlags>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline RenderObjectDirtyFlags& operator|=(RenderObjectDirtyFlags& lhs, const RenderObjectDirtyFlags rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const RenderObjectDirtyFlags lhs, const RenderObjectDirtyFlags rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

    class RenderObject {
    protected:
        Identifier m_id{0};
        UUID m_uuid{};
        Matrix4f m_world_transform{1.0f};

    public:
        virtual ~RenderObject() = default;

        void setId(const Identifier id) { m_id = id; }
        [[nodiscard]] Identifier getId() const { return m_id; }
        void setUUID(const UUID& uuid) { m_uuid = uuid; }
        [[nodiscard]] UUID getUUID() const { return m_uuid; }
        void setWorldTransform(const Matrix4f& transform) { m_world_transform = transform; }
        [[nodiscard]] const Matrix4f& getWorldTransform() const { return m_world_transform; }

        [[nodiscard]] virtual RenderObjectType getRenderObjectType() const = 0;
        [[nodiscard]] virtual RenderObjectDirtyFlags diff(const RenderObject& previous) const;
    };

} // dodoe
