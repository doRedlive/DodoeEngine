// do@Redlive

#include "gc_root.h"

namespace dodoe {

    GCRootRegistry& GCRootRegistry::instance() {
        static GCRootRegistry s_instance;
        return s_instance;
    }

    void GCRootRegistry::registerRoot(Object* obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roots.push_back(obj);
    }

    void GCRootRegistry::unregisterRoot(InstanceID id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (Size_t i = 0; i < m_roots.size(); ++i) {
            if (m_roots[i] && m_roots[i]->getInstanceID() == id) {
                m_roots[i] = m_roots.back();
                m_roots.pop_back();
                return;
            }
        }
    }

    void GCRootRegistry::collectRoots(DynamicArray<Object*>& out_roots) {
        std::lock_guard<std::mutex> lock(m_mutex);
        out_roots.clear();
        for (auto* root : m_roots) {
            if (root && root->isAlive()) {
                out_roots.push_back(root);
            }
        }
    }

} // namespace dodoe
