// do@Redlive

#include "native_bridge.h"
#include "runtime/core/container/spsc_queue.h"
#include "runtime/core/gc/gc_root.h"

namespace dodoe {
    namespace native_bridge {

        static UnorderedSet<InstanceID> s_acquired{};
        static SpscQueue<InstanceID, 256> s_release_queue{};

        void NotifyDestroyed(InstanceID id) {
            if (id == 0) return;
            s_acquired.erase(id);
            UnregisterRoot(id);
        }

        void AcquireRef(InstanceID id) {
            if (id == 0) return;
            s_acquired.insert(id);
            RegisterRoot(id);
            auto* obj = Object::FindObjectFromInstanceID(id);
            if (obj) obj->addRef();
        }

        void ReleaseRef(InstanceID id) {
            if (id == 0) return;
            s_release_queue.tryPush(id);
        }

        void DrainReleases() {
            InstanceID id;
            while (s_release_queue.tryPop(id)) {
                s_acquired.erase(id);
                UnregisterRoot(id);
                auto* obj = Object::FindObjectFromInstanceID(id);
                if (obj) obj->releaseRef();
            }
        }

        void RegisterRoot(InstanceID id) {
            auto* obj = Object::FindObjectFromInstanceID(id);
            if (obj) GCRootRegistry::instance().registerRoot(obj);
        }

        void UnregisterRoot(InstanceID id) {
            auto* obj = Object::FindObjectFromInstanceID(id);
            if (obj) GCRootRegistry::instance().unregisterRoot(obj->getInstanceID());
        }

        const UnorderedSet<InstanceID>& GetAcquiredSet() {
            return s_acquired;
        }

    } // namespace native_bridge
} // namespace dodoe
