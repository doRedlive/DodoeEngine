// do@Redlive

#include "skeleton.h"

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Skeleton>> s_skeleton_cache{};

    } // namespace

    Skeleton* Skeleton::Create(const ObjectID& id) {
        if (!id.isValid()) {
            return nullptr;
        }
        if (const InstanceID existing = Object::FindInstanceID(id); existing != 0) {
            if (auto* obj = Object::FindObjectFromInstanceID(existing)) {
                return static_cast<Skeleton*>(obj);
            }
        }
        auto skeleton = create_scope<Skeleton>(id);
        Skeleton* raw = skeleton.get();
        s_skeleton_cache.emplace(raw->getInstanceID(), std::move(skeleton));
        return raw;
    }

    void Skeleton::Shutdown() {
        s_skeleton_cache.clear();
    }

} // dodoe
