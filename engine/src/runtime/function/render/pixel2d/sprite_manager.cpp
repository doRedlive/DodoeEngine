// do@Redlive

#include "sprite_manager.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/math/math.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/importer/import_settings_io.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    namespace {

        Scope<Sprite> s_fallback{};
        UnorderedMap<InstanceID, Scope<Sprite>> s_sprite_manager{};

        void CreateFallback() {
            auto sprite_scope = create_scope<Sprite>(ObjectID{UUID(0), 3});
            Sprite* sprite = sprite_scope.get();
            sprite->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
            sprite->setPixelsPerUnit(kDefaultPixelsPerUnit);
            s_fallback = std::move(sprite_scope);
        }
    }

    void SpriteManager::Initialize() {
        (void)GetFallback();
    }

    Sprite* SpriteManager::Create(const ObjectID& ref, const String& path) {
        Float ppu = kDefaultPixelsPerUnit;
        Float u0 = 0.0f;
        Float v0 = 0.0f;
        Float u1 = 1.0f;
        Float v1 = 1.0f;

        Texture2D* texture = ResourceManager::Self().loadObject<Texture2D>(ref.asset_id, 0);

        if (auto* asset_manager = ResourceManager::Self().getAssetManager()) {
            const FsPath absolute = asset_manager->getAssetDir() / FsPath(path.c_str());
            ImportSettings settings;
            if (ImportSettingsIO::Load(absolute, settings)) {
                const SpriteMeta* found = nullptr;
                for (const auto& sprite : settings.sprites) {
                    if (sprite.local_id == ref.local_id) {
                        found = &sprite;
                        break;
                    }
                }
                if (found) {
                    ppu = found->ppu;
                    if (texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
                        u0 = found->slice_left / static_cast<Float>(texture->getWidth());
                        u1 = found->slice_right / static_cast<Float>(texture->getWidth());
                        v0 = found->slice_bottom / static_cast<Float>(texture->getHeight());
                        v1 = found->slice_top / static_cast<Float>(texture->getHeight());
                    }
                }
            }
        }

        auto sprite = create_scope<Sprite>(ref);
        Sprite* sprite_raw = sprite.get();
        sprite_raw->setTexture(PPtr<Texture2D>(texture));
        sprite_raw->setUVRect(u0, v0, u1, v1);
        sprite_raw->setPixelsPerUnit(ppu);
        const InstanceID instance_id = sprite_raw->getInstanceID();
        s_sprite_manager.emplace(instance_id, std::move(sprite));
        return sprite_raw;
    }

    Sprite* SpriteManager::GetFallback() {
        if (!s_fallback) {
            CreateFallback();
        }
        return s_fallback.get();
    }

    void SpriteManager::Shutdown() {
        s_sprite_manager.clear();
        s_fallback = {};
    }

} // dodoe
