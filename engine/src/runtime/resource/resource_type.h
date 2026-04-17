//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_RESOURCE_TYPE_H
#define DODOE_RESOURCE_TYPE_H

#include "dopch.h"

#include "runtime/function/animation/animation.h"

namespace dodoe {

    struct TextureRes {
        identifier id{0};
        std::string path{};
        float ppu{10.0f};
    };


    struct Material {
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        Vector3f emissive{0.0f, 0.0f, 0.0f};
        float metallic{0.0f};
        float roughness{1.0f};
        identifier base_color_texture{};
        identifier normal_texture{};
        identifier metallic_roughness_texture{};
        identifier emissive_texture{};
    };

	struct MeshVertex {
		Vector3f position;
		Vector3f normal;
		Vector2f tex_coords;
		Vector3f tangent;
		Vector3f bitangent;
	};

    struct MeshData {
        std::vector<MeshVertex> vertices;
        std::vector<ui32> indices;
        std::vector<identifier> textures;

        ~MeshData() {
            vertices.clear();
            indices.clear();
            textures.clear();
        }
    };

    struct MeshRes {
        identifier id{0};
        Ref<MeshData> data{nullptr};
    };

    struct ModelData {
        std::vector<identifier> meshes;
        std::string directory;
    };

    struct ModelRes {
        identifier id{0};
        Ref<ModelData> data{nullptr};
    };

    struct AnimClip2dRes {
        Ref<AnimClip2d> clip;
        std::string name;
        identifier id;
    };

} // dodoe

#endif//DODOE_RESOURCE_TYPE_H