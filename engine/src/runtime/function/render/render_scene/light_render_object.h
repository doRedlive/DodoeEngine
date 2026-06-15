// do@Redlive

#pragma once

#include "dopch.h"

#include "render_object.h"
#include "runtime/core/utils/util.h"
#include "runtime/function/render/framework/texture.h"

namespace dodoe {

    class LightRenderObject : public RenderObject {
    protected:
        Color m_color{Color::white()};
        Float m_intensity{1.0f};

    public:
        void setColor(const Color& color) { m_color = color; }
        void setIntensity(const Float intensity) { m_intensity = intensity; }

        [[nodiscard]] const Color& getColor() const { return m_color; }
        [[nodiscard]] Float getIntensity() const { return m_intensity; }

        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
    };

    class PointLightRenderObject final : public LightRenderObject {
    private:
        Float m_radius{0.0f};
        Float m_range{10.0f};

    public:
        void setRadius(const Float radius) { m_radius = radius; }
        void setRange(const Float range) { m_range = range; }

        [[nodiscard]] Float getRadius() const { return m_radius; }
        [[nodiscard]] Float getRange() const { return m_range; }

        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::PointLight; }
        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
    };

    class SpotLightRenderObject final : public LightRenderObject {
    private:
        Float m_radius{0.0f};
        Float m_range{10.0f};
        Float m_inner_angle{30.0f};
        Float m_outer_angle{45.0f};

    public:
        void setRadius(const Float radius) { m_radius = radius; }
        void setRange(const Float range) { m_range = range; }
        void setInnerAngle(const Float angle) { m_inner_angle = angle; }
        void setOuterAngle(const Float angle) { m_outer_angle = angle; }

        [[nodiscard]] Float getRadius() const { return m_radius; }
        [[nodiscard]] Float getRange() const { return m_range; }
        [[nodiscard]] Float getInnerAngle() const { return m_inner_angle; }
        [[nodiscard]] Float getOuterAngle() const { return m_outer_angle; }

        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::SpotLight; }
        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
    };

    class SkyLightRenderObject final : public LightRenderObject {
    private:
        Ref<Texture> m_cubemap{};

    public:
        void setCubemap(const Ref<Texture>& cubemap) { m_cubemap = cubemap; }
        [[nodiscard]] const Ref<Texture>& getCubemap() const { return m_cubemap; }

        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::SkyLight; }
        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
    };

} // dodoe
