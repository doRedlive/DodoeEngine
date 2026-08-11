// do@Redlive

#include "object.h"
#include "runtime/core/asserts.h"
#include "runtime/core/utils/uuid.h"

#include <shared_mutex>

namespace dodoe {

    UnorderedMap<InstanceID, Object*> Object::s_instance_map{};
    UnorderedMap<UUID, InstanceID> Object::s_uuid_to_instance{};
    InstanceID Object::s_next_instance_id{1};
    DynamicArray<InstanceID> Object::s_free_instance_ids{};
    std::shared_mutex Object::s_instance_mutex{};

    Object::Object(const FileID& file_id)
        : m_file_id(file_id), m_uuid() {
        AllocateInstanceID(this);
    }

    Object::Object(const FileID& file_id, const UUID& uuid)
        : m_file_id(file_id), m_uuid(uuid) {
        AllocateInstanceID(this);
    }

    Object* Object::FindObjectFromInstanceID(const InstanceID id) {
        std::shared_lock lock(s_instance_mutex);
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    InstanceID Object::FindInstanceID(const UUID& uuid) {
        std::shared_lock lock(s_instance_mutex);
        const auto it = s_uuid_to_instance.find(uuid);
        if (it != s_uuid_to_instance.end()) {
            return it->second;
        }
        return 0;
    }

    InstanceID Object::AllocateInstanceID(Object* obj) {
        std::unique_lock lock(s_instance_mutex);

        if (obj->m_uuid.isValid()) {
            const auto it = s_uuid_to_instance.find(obj->m_uuid);
            if (it != s_uuid_to_instance.end()) {
                DO_ASSERT(false, "AllocateInstanceID: duplicate uuid constructed as a new object");
                obj->m_instance_id = it->second;
                return it->second;
            }
        }

        InstanceID id = 0;
        if (!s_free_instance_ids.empty()) {
            id = s_free_instance_ids.back();
            s_free_instance_ids.pop_back();
        } else {
            id = s_next_instance_id++;
        }

        obj->m_instance_id = id;
        s_instance_map[id] = obj;
        if (obj->m_uuid.isValid()) {
            s_uuid_to_instance[obj->m_uuid] = id;
        }
        return id;
    }

    void Object::ReleaseInstanceID(const InstanceID id) {
        std::unique_lock lock(s_instance_mutex);
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            Object* obj = it->second;
            if (obj->m_uuid.isValid()) {
                const auto uit = s_uuid_to_instance.find(obj->m_uuid);
                if (uit != s_uuid_to_instance.end() && uit->second == id) {
                    s_uuid_to_instance.erase(uit);
                }
            }
            s_instance_map.erase(it);
            s_free_instance_ids.push_back(id);
        }
    }

    void Object::setFileIdentity(const FileID& file_id, const UUID& uuid) {
        std::unique_lock lock(s_instance_mutex);
        if (m_uuid.isValid()) {
            const auto it = s_uuid_to_instance.find(m_uuid);
            if (it != s_uuid_to_instance.end() && it->second == m_instance_id) {
                s_uuid_to_instance.erase(it);
            }
        }
        m_file_id = file_id;
        m_uuid = uuid;
        if (m_uuid.isValid() && m_instance_id != 0) {
            s_uuid_to_instance[m_uuid] = m_instance_id;
        }
    }

    Object::~Object() {
        if (m_instance_id) {
            ReleaseInstanceID(m_instance_id);
            m_instance_id = 0;
        }
    }

} // namespace dodoe
