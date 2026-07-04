// do@Redlive

#pragma once

#include "dopch.h"

#include "view_extension.h"

namespace dodoe {

    class SpriteSceneInfo;

    class SpriteViewExtension : public IViewExtension {
    public:
        DynamicArray<const SpriteSceneInfo*> visible_sprites{};

        void reset() override {
            visible_sprites.clear();
        }
    };

} // dodoe
