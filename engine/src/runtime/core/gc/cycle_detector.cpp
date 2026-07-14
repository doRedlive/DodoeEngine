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
                root->Trace(visitor);
                visitor.mark(root->getInstanceID());
            }
        }

        const auto& acquired = native_bridge::GetAcquiredSet();
        for (InstanceID id : acquired) {
            auto* obj = Object::FindObjectFromInstanceID(id);
            if (obj && obj->isAlive()) {
                obj->Trace(visitor);
                visitor.mark(id);
            }
        }

        DynamicArray<Object*> candidates{};
        for (auto& [id, obj] : Object::s_instance_map) {
            if (obj && obj->isAlive() && !visitor.isMarked(id)) {
                if (obj->m_strong_refs.load(std::memory_order_relaxed) > 0) {
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
                candidate->m_alive.store(0, std::memory_order_release);
                candidate->onDestroy();
                native_bridge::NotifyDestroyed(candidate->getInstanceID());
                Object::ReleaseInstanceID(candidate->getInstanceID());
                delete candidate;
            }
        }
    }

} // namespace dodoe
