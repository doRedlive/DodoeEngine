// do@Redlive

#include "object.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    UnorderedMap<InstanceID, Object*> Object::s_instance_map{};
    UnorderedMap<UInt64, InstanceID> Object::s_ref_to_instance{};
    UnorderedMap<InstanceID, UInt32> Object::s_generations{};
    InstanceID Object::s_next_instance_id{1};

    Object::Object(const ObjectID& id)
        : m_id(id) {
        AllocateInstanceID(this);
    }

    UInt64 Object::makeKey(const ObjectID& id) {
        return static_cast<UInt64>(id.asset_id) ^ (static_cast<UInt64>(id.local_id) << 32);
    }

    Object* Object::FindObjectFromInstanceID(const InstanceID id) {
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            return it->second;
        }
        return nullptr;
    }

    InstanceID Object::FindInstanceID(const ObjectID& id) {
        const UInt64 key = makeKey(id);
        const auto it = s_ref_to_instance.find(key);
        if (it != s_ref_to_instance.end()) {
            return it->second;
        }
        return 0;
    }

    InstanceID Object::AllocateInstanceID(Object* obj) {
        const UInt64 key = makeKey(obj->m_id);
        const auto it = s_ref_to_instance.find(key);
        if (it != s_ref_to_instance.end()) {
            obj->m_instance_id = it->second;
            const auto gen_it = s_generations.find(it->second);
            obj->m_generation = gen_it != s_generations.end() ? gen_it->second : 1;
            return it->second;
        }

        const InstanceID id = s_next_instance_id++;
        obj->m_instance_id = id;
        obj->m_generation = 1;
        obj->m_registered = true;
        s_instance_map[id] = obj;
        s_generations[id] = 1;
        s_ref_to_instance[key] = id;
        return id;
    }

    void Object::ReleaseInstanceID(const InstanceID id) {
        const auto it = s_instance_map.find(id);
        if (it != s_instance_map.end()) {
            s_ref_to_instance.erase(makeKey(it->second->m_id));
            auto gen_it = s_generations.find(id);
            if (gen_it != s_generations.end()) {
                gen_it->second += 1;
            }
            s_instance_map.erase(id);
        }
    }

    Bool Object::isAlive(const InstanceID id, const UInt32 generation) {
        const auto it = s_instance_map.find(id);
        if (it == s_instance_map.end()) {
            return false;
        }
        const auto gen_it = s_generations.find(id);
        return gen_it != s_generations.end() && gen_it->second == generation;
    }

    Object::~Object() {
        if (m_registered) {
            ReleaseInstanceID(m_instance_id);
            m_registered = false;
            m_instance_id = 0;
        }
    }

} // namespace dodoe
