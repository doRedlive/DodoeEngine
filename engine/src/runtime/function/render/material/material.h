// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/core/object/pptr.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    class Texture2D;

    struct MaterialProperties {
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        Vector3f emissive{0.0f, 0.0f, 0.0f};
        Float metallic{0.0f};
        Float roughness{1.0f};
        FileID base_color_texture{};
        FileID normal_texture{};
        FileID metallic_roughness_texture{};
        FileID emissive_texture{};
        Bool operator==(const MaterialProperties& other) const = default;
    };

    class DODOE_API Material : public Object {
        Vector4f m_color{1.0f, 1.0f, 1.0f, 1.0f};
        Vector3f m_emissive{0.0f, 0.0f, 0.0f};
        Float m_metallic{0.0f};
        Float m_roughness{1.0f};
        PPtr<Texture2D> m_base_color_texture{};
        PPtr<Texture2D> m_normal_texture{};
        PPtr<Texture2D> m_metallic_roughness_texture{};
        PPtr<Texture2D> m_emissive_texture{};
        String m_path{};

    public:
        Material() = default;
        explicit Material(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Material"; }

        void setColor(const Vector4f& color) { m_color = color; }
        void setEmissive(const Vector3f& emissive) { m_emissive = emissive; }
        void setMetallic(const Float metallic) { m_metallic = metallic; }
        void setRoughness(const Float roughness) { m_roughness = roughness; }
        void setBaseColorTexture(const PPtr<Texture2D>& texture) { m_base_color_texture = texture; }
        void setNormalTexture(const PPtr<Texture2D>& texture) { m_normal_texture = texture; }
        void setMetallicRoughnessTexture(const PPtr<Texture2D>& texture) { m_metallic_roughness_texture = texture; }
        void setEmissiveTexture(const PPtr<Texture2D>& texture) { m_emissive_texture = texture; }
        void setPath(const String& path) { m_path = path; }

        [[nodiscard]] const Vector4f& getColor() const { return m_color; }
        [[nodiscard]] const Vector3f& getEmissive() const { return m_emissive; }
        [[nodiscard]] Float getMetallic() const { return m_metallic; }
        [[nodiscard]] Float getRoughness() const { return m_roughness; }
        [[nodiscard]] const PPtr<Texture2D>& getBaseColorTexture() const { return m_base_color_texture; }
        [[nodiscard]] const PPtr<Texture2D>& getNormalTexture() const { return m_normal_texture; }
        [[nodiscard]] const PPtr<Texture2D>& getMetallicRoughnessTexture() const { return m_metallic_roughness_texture; }
        [[nodiscard]] const PPtr<Texture2D>& getEmissiveTexture() const { return m_emissive_texture; }
        [[nodiscard]] const String& getPath() const { return m_path; }

        [[nodiscard]] Bool loadFromJson(const String& absolute_path);
        [[nodiscard]] Bool saveToJson(const String& absolute_path) const;

        [[nodiscard]] static Material* Create(const ObjectID& ref, const String& path);
        static void Shutdown();
    };

} // dodoe
