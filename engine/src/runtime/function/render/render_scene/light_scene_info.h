// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/framework/texture.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    enum class LightType : UInt8 {
        Directional,
        Point,
        Spot,
        Sky
    };

    struct DirectionalLightData {
        Vector3f direction{0.3f, -0.8f, -0.5f};
        Vector3f color{1.0f};
        Float irradiance{1.0f};
    };

    struct PointLightData {
        Float radius{1.0f};
        Float range{10.0f};
        Vector3f color{1.0f};
        Float intensity{1.0f};
    };

    struct SpotLightData {
        Float radius{1.0f};
        Float range{10.0f};
        Vector3f color{1.0f};
        Float intensity{1.0f};
        Float inner_angle{30.0f};
        Float outer_angle{45.0f};
    };

    struct SkyLightData {
        Ref<Texture> cubemap{};
        Float intensity{1.0f};
    };

    class LightSceneInfo {
    public:
        LightSceneInfo() = default;
        explicit LightSceneInfo(const Identifier id) : m_id(id) { }

        void setId(const Identifier id) { m_id = id; }
        void setLightType(const LightType light_type) { m_light_type = light_type; }
        void setWorldTransform(const Matrix4f& world_transform) { m_world_transform = world_transform; }
        void setEnabled(const Bool enabled) { m_enabled = enabled; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }

        void setDirectionalLightData(const DirectionalLightData& data) { m_directional_data = data; }
        void setPointLightData(const PointLightData& data) { m_point_data = data; }
        void setSpotLightData(const SpotLightData& data) { m_spot_data = data; }
        void setSkyLightData(const SkyLightData& data) { m_sky_data = data; }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] LightType getLightType() const { return m_light_type; }
        [[nodiscard]] const Matrix4f& getWorldTransform() const { return m_world_transform; }
        [[nodiscard]] Bool isEnabled() const { return m_enabled; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }

        [[nodiscard]] const DirectionalLightData& getDirectionalLightData() const { return m_directional_data; }
        [[nodiscard]] const PointLightData& getPointLightData() const { return m_point_data; }
        [[nodiscard]] const SpotLightData& getSpotLightData() const { return m_spot_data; }
        [[nodiscard]] const SkyLightData& getSkyLightData() const { return m_sky_data; }

    private:
        Identifier m_id{};
        LightType m_light_type{LightType::Point};
        Matrix4f m_world_transform{1.0f};
        Bool m_enabled{true};
        Bool m_cast_shadow{true};

        DirectionalLightData m_directional_data{};
        PointLightData m_point_data{};
        SpotLightData m_spot_data{};
        SkyLightData m_sky_data{};
    };

} // dodoe
