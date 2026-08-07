// do@Redlive

#include "object.h"
#include "runtime/core/asserts.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    UnorderedMap<InstanceID, Object*> Object::s_instance_map{};
    UnorderedMap<UInt64, InstanceID> Object::s_id_to_instance{};
    InstanceID Object::s_next_instance_id{1};

    Object::Object(const FileID& file_id)
        : m_file_id(file_id), m_uuid() {
        AllocateInstanceID(this);
    }

    Object::Object(const FileID& file_id, const UUID& uuid)
        : m_file_id(file_id), m_uuid(uuid) {
        AllocateInstanceID(this);
    }

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
            DO_ASSERT(false, "AllocateInstanceID: duplicate FileID constructed as a new object");
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

    Object::~Object() {
        if (m_instance_id) {
            ReleaseInstanceID(m_instance_id);
            m_instance_id = 0;
        }
    }

} // namespace dodoe
