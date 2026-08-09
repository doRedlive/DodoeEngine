// do@Redlive

#include "sprite_loader.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/texture/texture_manager.h"

namespace dodoe {

    namespace {

        constexpr UInt64 kSpriteUuidSalt = 0x9E3779B97F4A7C15ULL;

        Scope<Sprite> s_fallback{};
        UnorderedMap<InstanceID, Scope<Sprite>> s_sprite_cache{};

        FileID MakeSpriteFileID(const String& path) {
            const FileID texture_id(path);
            return FileID(path, UUID(static_cast<UInt64>(texture_id.getUUID()) ^ kSpriteUuidSalt));
        }

        Sprite* CreateSprite(const String& path) {
            Texture2D* texture = Texture2D::Load(path);
            const FileID file_id = MakeSpriteFileID(path);
            auto sprite = create_scope<Sprite>(file_id);
            Sprite* sprite_raw = sprite.get();
            sprite_raw->setTexture(PPtr<Texture2D>(texture));
            sprite_raw->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
            sprite_raw->setPixelsPerUnit(kDefaultPixelsPerUnit);
            const InstanceID id = sprite_raw->getInstanceID();
            s_sprite_cache.emplace(id, std::move(sprite));
            return sprite_raw;
        }

        void CreateFallback() {
            auto sprite_scope = create_scope<Sprite>(FileID("<sprite-fallback>"), UUID(0));
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
        const FileID file_id = MakeSpriteFileID(path);
        const InstanceID existing = Object::FindInstanceID(file_id);
        if (existing != 0) {
            return static_cast<Sprite*>(Object::FindObjectFromInstanceID(existing));
        }
        return CreateSprite(path);
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
