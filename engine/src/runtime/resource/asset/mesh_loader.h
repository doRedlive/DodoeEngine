// do-redlive

#pragma once

#include "dopch.h"

#include "../resource_type.h"

namespace dodoe {

    class MeshLoader {
        std::unordered_map<identifier, ModelRes> model_umap_{};
        std::unordered_map<identifier, MeshRes> mesh_umap_{};
        identifier next_mesh_id_{1};

    public:
        static Scope<MeshLoader> create();
        static void destroy(Scope<MeshLoader>& loader);

        ModelRes loadModel(identifier id, const std::string& path);
        ModelRes loadModel(const std::string& id, const std::string& path);

        [[nodiscard]] MeshRes getMesh(identifier id) const;
        identifier addMesh(const Ref<MeshData>& mesh_data);
        
        [[nodiscard]] ModelRes getModel(identifier id);
        [[nodiscard]] ModelRes getModel(identifier id, const std::string& path);
        [[nodiscard]] ModelRes getModel(const std::string& id);
        [[nodiscard]] ModelRes getModel(const std::string& id, const std::string& path);

    private:
            void initialize();
            void shutdown();
            identifier allocateMeshId();
    };

} // dodoe