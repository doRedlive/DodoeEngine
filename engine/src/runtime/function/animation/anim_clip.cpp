// do@Redlive

#include "anim_clip.h"

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<AnimClip>> s_anim_clip_cache{};

    } // namespace

    AnimClip* AnimClip::Create(const ObjectID& id) {
        if (!id.isValid()) {
            return nullptr;
        }
        if (const InstanceID existing = Object::FindInstanceID(id); existing != 0) {
            if (auto* obj = Object::FindObjectFromInstanceID(existing)) {
                return static_cast<AnimClip*>(obj);
            }
        }
        auto clip = create_scope<AnimClip>(id);
        AnimClip* raw = clip.get();
        s_anim_clip_cache.emplace(raw->getInstanceID(), std::move(clip));
        return raw;
    }

    void AnimClip::Shutdown() {
        s_anim_clip_cache.clear();
    }

} // dodoe
