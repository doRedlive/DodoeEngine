// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/object/object_id.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/world/entity.h"

namespace dodoe {

    class Prefab;

    class DODOE_API SceneImporter {
    public:
        static void ImportModel(const String& path);
        static void ImportSprite(const String& path);
        static Entity ImportPrefab(const String& path);
        static Entity InstantiatePrefab(const PPtr<Prefab>& prefab_ref);
        static ObjectID ExportPrefab(const String& path, Entity root);
        static void ImportAsset(const String& path);
    };

} // dodoe
