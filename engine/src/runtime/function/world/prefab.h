// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object.h"
#include "runtime/resource/res_type/scene_res.h"

namespace dodoe {

    class DODOE_API Prefab : public Object {
        SceneRes m_scene_res{};
        String m_path{};

    public:
        Prefab() = default;
        explicit Prefab(const ObjectID& id)
            : Object(id) {}

        [[nodiscard]] const char* getObjectTypeName() const override { return "Prefab"; }

        [[nodiscard]] const SceneRes& getSceneRes() const { return m_scene_res; }
        void setSceneRes(const SceneRes& res) { m_scene_res = res; }
        [[nodiscard]] const String& getPath() const { return m_path; }
        void setPath(const String& path) { m_path = path; }

        [[nodiscard]] static Prefab* Create(const ObjectID& ref, const String& path);
        static void Shutdown();
    };

} // dodoe
