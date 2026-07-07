// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class DODOE_API SceneImporter {
    public:
        static void ImportModel(const String& path);
        static void ImportSprite(const String& path);
        static void ImportAsset(const String& path);
    };

} // dodoe
