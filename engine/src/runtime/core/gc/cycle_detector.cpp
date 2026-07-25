// do@Redlive

#include "cycle_detector.h"
#include "gc_root.h"
#include "runtime/core/object/native_bridge.h"

namespace dodoe {

    CycleDetector& CycleDetector::instance() {
        static CycleDetector s_instance;
        return s_instance;
    }

    void CycleDetector::tick() {
        ++m_frame_count;
        if (m_frame_count % m_interval != 0) {
            return;
        }

        TraceVisitor visitor;
        DynamicArray<Object*> roots{};
        GCRootRegistry::instance().collectRoots(roots);

        for (auto* root : roots) {
            if (root && root->isAlive()) {
                root->trace(visitor);
                visitor.mark(root->getInstanceID());
            }
        }

        const auto& acquired = native_bridge::GetAcquiredSet();
        for (InstanceID id : acquired) {
            auto* obj = Object::FindObjectFromInstanceID(id);
            if (obj && obj->isAlive()) {
                obj->trace(visitor);
                visitor.mark(id);
            }
        }

        DynamicArray<Object*> candidates{};
        for (auto& [id, obj] : Object::GetInstanceMap()) {
            if (obj && obj->isAlive() && !visitor.isMarked(id)) {
                if (obj->getStrongRefCount() > 0) {
                    candidates.push_back(obj);
                }
            }
        }

        for (auto* candidate : candidates) {
            TraceVisitor rescan_visitor;
            candidate->trace(rescan_visitor);
            const auto& children = rescan_visitor.getMarkedSet();

            Bool all_unreachable = true;
            for (InstanceID child_id : children) {
                if (visitor.isMarked(child_id)) {
                    all_unreachable = false;
                    break;
                }
            }

            if (all_unreachable) {
                candidate->releaseRef();
            }
        }
    }

} // namespace dodoe
