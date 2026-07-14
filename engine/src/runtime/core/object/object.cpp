// do@Redlive

#include "object.h"
#include "runtime/core/utils/uuid.h"
#include "runtime/core/object/native_bridge.h"

namespace dodoe {

    UnorderedMap<InstanceID, Object*> Object::s_instance_map{};
    UnorderedMap<UInt64, InstanceID> Object::s_id_to_instance{};
    InstanceID Object::s_next_instance_id{1};

    UInt64 Object::makeKey(const FileID& file_id) {
        return file_id.getID();
    }

    Object* Object::FindObjectFromInstanceID(const InstanceID id) {
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    InstanceID Object::FindInstanceID(const FileID& file_id) {
        const UInt64 key = makeKey(file_id);
        const auto it = s_id_to_instance.find(key);
        if (it != s_id_to_instance.end()) {
            return it->second;
        }
        return 0;
    }

    InstanceID Object::AllocateInstanceID(Object* obj) {
        const UInt64 key = makeKey(obj->m_file_id);
        const auto it = s_id_to_instance.find(key);
        if (it != s_id_to_instance.end()) {
            obj->m_instance_id = it->second;
            return it->second;
        }

        const InstanceID id = s_next_instance_id++;
        obj->m_instance_id = id;
        s_instance_map[id] = obj;
        s_id_to_instance[key] = id;
        return id;
    }

    void Object::ReleaseInstanceID(const InstanceID id) {
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            s_id_to_instance.erase(makeKey(it->second->m_file_id));
        }
        s_instance_map.erase(id);
    }

    void Object::releaseRef() {
        if (m_strong_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            m_alive.store(0, std::memory_order_release);
            onDestroy();
            native_bridge::NotifyDestroyed(m_instance_id);
            ReleaseInstanceID(m_instance_id);
            m_strong_refs.store(0, std::memory_order_relaxed);
            delete this;
        }
    }

    void Object::releaseWeakRef() {
        if (m_weak_refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            m_weak_refs.store(0, std::memory_order_relaxed);
        }
    }

    Object::~Object() {
        if (m_instance_id) {
            onDestroy();
            native_bridge::NotifyDestroyed(m_instance_id);
            ReleaseInstanceID(m_instance_id);
        }
    }

} // dodoe
