// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/texture/sprite.h"

namespace dodoe {

    class DODOE_API SpriteManager {
    public:
        static void Initialize();
        [[nodiscard]] static Sprite* Create(const ObjectID& ref, const String& path);
        [[nodiscard]] static Sprite* GetFallback();
        static void Shutdown();
    };

} // dodoe
