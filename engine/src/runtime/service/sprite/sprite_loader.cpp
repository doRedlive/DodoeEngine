// do@Redlive

#include "sprite_loader.h"

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

        constexpr UInt64 kSpriteUuidSalt = 0x9E3779B97F4A7C15ULL;

        Scope<Sprite> s_fallback{};
        UnorderedMap<InstanceID, Scope<Sprite>> s_sprite_cache{};

        ObjectID MakeDefaultSpriteObjectID(const String& path) {
            return ObjectID{UUID(static_cast<UInt64>(string2hash(path)) ^ kSpriteUuidSalt), 0};
        }

        Bool ResolveSpriteFromMeta(const String& path,
                                   ObjectID& out_id,
                                   Float& out_ppu,
                                   Float& out_u0,
                                   Float& out_v0,
                                   Float& out_u1,
                                   Float& out_v1) {
            auto* asset_manager = ResourceManager::Self().getAssetManager();
            if (!asset_manager) {
                return false;
            }
            const ObjectID ref = asset_manager->resolvePathToRef(FileID(path));
            if (!ref.isValid()) {
                return false;
            }

            const FsPath absolute = asset_manager->getAssetDir() / FsPath(path.c_str());
            ImportSettings settings;
            if (!ImportSettingsIO::Load(absolute, settings) || settings.sprites.empty()) {
                return false;
            }

            const auto& sprite = settings.sprites.front();
            if (sprite.local_id == 0) {
                return false;
            }

            Float u0 = 0.0f;
            Float v0 = 0.0f;
            Float u1 = 1.0f;
            Float v1 = 1.0f;
            if (auto* texture = Texture2D::Load(path);
                texture && texture->getWidth() > 0 && texture->getHeight() > 0) {
                u0 = sprite.slice_left / static_cast<Float>(texture->getWidth());
                u1 = sprite.slice_right / static_cast<Float>(texture->getWidth());
                v0 = sprite.slice_bottom / static_cast<Float>(texture->getHeight());
                v1 = sprite.slice_top / static_cast<Float>(texture->getHeight());
            }

            out_id = ObjectID{ref.asset_id, sprite.local_id};
            out_ppu = sprite.ppu;
            out_u0 = u0;
            out_v0 = v0;
            out_u1 = u1;
            out_v1 = v1;
            return true;
        }

        void CreateFallback() {
            auto sprite_scope = create_scope<Sprite>(ObjectID{UUID(0), 3});
            Sprite* sprite = sprite_scope.get();
            sprite->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
            sprite->setPixelsPerUnit(kDefaultPixelsPerUnit);
            s_fallback = std::move(sprite_scope);
        }
    }

    void SpriteLoader::Initialize() {
        (void)GetFallback();
    }

    Sprite* SpriteLoader::Load(const String& path) {
        if (path.empty()) {
            return GetFallback();
        }

        ObjectID id;
        Float ppu = kDefaultPixelsPerUnit;
        Float u0 = 0.0f;
        Float v0 = 0.0f;
        Float u1 = 1.0f;
        Float v1 = 1.0f;
        if (!ResolveSpriteFromMeta(path, id, ppu, u0, v0, u1, v1)) {
            id = MakeDefaultSpriteObjectID(path);
        }

        const InstanceID existing = Object::FindInstanceID(id);
        if (existing != 0) {
            return static_cast<Sprite*>(Object::FindObjectFromInstanceID(existing));
        }

        Texture2D* texture = Texture2D::Load(path);
        auto sprite = create_scope<Sprite>(id);
        Sprite* sprite_raw = sprite.get();
        sprite_raw->setTexture(PPtr<Texture2D>(texture));
        sprite_raw->setUVRect(u0, v0, u1, v1);
        sprite_raw->setPixelsPerUnit(ppu);
        const InstanceID instance_id = sprite_raw->getInstanceID();
        s_sprite_cache.emplace(instance_id, std::move(sprite));
        return sprite_raw;
    }

    Sprite* SpriteLoader::Find(InstanceID id) {
        const auto it = s_sprite_cache.find(id);
        if (it != s_sprite_cache.end()) {
            return it->second.get();
        }
        return GetFallback();
    }

    Sprite* SpriteLoader::GetFallback() {
        if (!s_fallback) {
            CreateFallback();
        }
        return s_fallback.get();
    }

    void SpriteLoader::Shutdown() {
        s_sprite_cache.clear();
        s_fallback = {};
    }

} // dodoe
