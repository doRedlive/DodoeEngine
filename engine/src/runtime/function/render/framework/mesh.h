// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/file/file_id.h"

namespace dodoe {

	struct MeshVertex {
		Vector3f position;
		Vector3f normal;
		Vector2f tex_coords;
		Vector3f tangent;
		Vector3f bitangent;
	};

    class MeshData {
    public:
        DynamicArray<MeshVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<FileID> textures;

        ~MeshData() {
            vertices.clear();
            indices.clear();
            textures.clear();
        }
    };
} // dodoe