#pragma once

#include "dopch.h"

namespace dodoe {
    class Scene;

    class SceneSerializer {
    public:
        explicit SceneSerializer(Scene* scene);

        bool serialize(const std::filesystem::path& file_path) const;
        bool deserialize(const std::filesystem::path& file_path);

    private:
        Scene* scene_{ nullptr };
    };
} // dodoe
