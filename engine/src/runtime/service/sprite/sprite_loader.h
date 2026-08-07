// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/texture/sprite.h"

namespace dodoe {

    class DODOE_API SpriteLoader {
    public:
        static void Initialize();
        [[nodiscard]] static Sprite* Load(const String& path);
        [[nodiscard]] static Sprite* Find(InstanceID id);
        [[nodiscard]] static Sprite* GetFallback();
        static void Shutdown();
    };

} // dodoe
