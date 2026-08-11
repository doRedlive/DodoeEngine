// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"
#include "object_id.h"
#include "runtime/core/utils/uuid.h"

namespace dodoe {

    template<typename T>
    class PPtr {
        ObjectID m_id{};
        InstanceID m_instance_id{0};
        String m_legacy_path{};

    public:
        PPtr() = default;
        explicit PPtr(const ObjectID& id)
            : m_id(id) {}
        PPtr(const ObjectID& id, const InstanceID instance_id)
            : m_id(id), m_instance_id(instance_id) {}
        explicit PPtr(T* obj) {
            if (obj) {
                m_id = obj->getID();
                m_instance_id = obj->getInstanceID();
            }
        }

        [[nodiscard]] T* get() const {
            if (m_instance_id != 0) {
                return static_cast<T*>(Object::FindObjectFromInstanceID(m_instance_id));
            }
            if (m_id.isValid()) {
                const InstanceID id = Object::FindInstanceID(m_id);
                if (id != 0) {
                    return static_cast<T*>(Object::FindObjectFromInstanceID(id));
                }
            }
            return nullptr;
        }
        T* operator->() const { return get(); }
        [[nodiscard]] explicit operator Bool() const { return m_instance_id != 0 || m_id.isValid(); }
        [[nodiscard]] Bool isValid() const { return m_instance_id != 0 || m_id.isValid(); }
        [[nodiscard]] Bool isAssigned() const { return m_id.isValid(); }

        [[nodiscard]] const ObjectID& getObjectID() const { return m_id; }
        [[nodiscard]] InstanceID getInstanceID() const { return m_instance_id; }
        [[nodiscard]] const String& getLegacyPath() const { return m_legacy_path; }
        void setLegacyPath(const String& path) { m_legacy_path = path; }
        void setIdentity(const ObjectID& id, const InstanceID instance_id = 0) {
            m_id = id;
            m_instance_id = instance_id;
        }
    };

} // dodoe
