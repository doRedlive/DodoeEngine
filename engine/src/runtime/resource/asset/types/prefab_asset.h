// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/res_type/scene_res.h"

namespace dodoe {

    class PrefabAsset : public Asset {
        SceneRes m_scene_res{};

    public:
        static constexpr AssetType kStaticType = AssetType::Prefab;

        PrefabAsset() { m_meta.type = AssetType::Prefab; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const SceneRes& getSceneRes() const { return m_scene_res; }
        SceneRes& getSceneRes() { return m_scene_res; }
    };

} // dodoe
