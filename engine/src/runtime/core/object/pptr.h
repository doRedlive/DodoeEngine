// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    template<typename T>
    class PPtr {
        UUID m_uuid{};
        InstanceID m_instance_id{0};

    public:
        PPtr() = default;
        explicit PPtr(const UUID& uuid)
            : m_uuid(uuid) {}
        PPtr(const UUID& uuid, const InstanceID instance_id)
            : m_uuid(uuid), m_instance_id(instance_id) {}
        explicit PPtr(T* obj)
            : m_uuid(obj->getUUID()), m_instance_id(obj->getInstanceID()) {}

        [[nodiscard]] T* get() const {
            if (m_instance_id != 0) {
                T* obj = static_cast<T*>(Object::FindObjectFromInstanceID(m_instance_id));
                if (obj && (!m_uuid.isValid() || obj->getUUID() == m_uuid)) {
                    return obj;
                }
            }
            if (m_uuid.isValid()) {
                const InstanceID id = Object::FindInstanceID(m_uuid);
                if (id != 0) {
                    return static_cast<T*>(Object::FindObjectFromInstanceID(id));
                }
            }
            return nullptr;
        }
        T* operator->() const { return get(); }
        [[nodiscard]] explicit operator Bool() const { return m_instance_id != 0 || m_uuid.isValid(); }
        [[nodiscard]] Bool isValid() const { return m_instance_id != 0 || m_uuid.isValid(); }

        [[nodiscard]] const UUID& getUUID() const { return m_uuid; }
        [[nodiscard]] InstanceID getInstanceID() const { return m_instance_id; }
    };

} // dodoe
