// do@Redlive

#pragma once

#include "dopch.h"
#include "object.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    template<typename T>
    class PPtr {
        FileID m_file_id{};
        UUID m_uuid{};
        InstanceID m_instance_id{0};

    public:
        PPtr() = default;
        PPtr(const FileID& file_id, const UUID& uuid)
            : m_file_id(file_id), m_uuid(uuid) {}
        PPtr(const FileID& file_id, const UUID& uuid, const InstanceID instance_id)
            : m_file_id(file_id), m_uuid(uuid), m_instance_id(instance_id) {}

        [[nodiscard]] T* get() const {
            if (m_instance_id != 0) {
                return static_cast<T*>(Object::FindObjectFromInstanceID(m_instance_id));
            }
            if (m_file_id.isValid()) {
                const InstanceID id = Object::FindInstanceID(m_file_id);
                if (id != 0) {
                    return static_cast<T*>(Object::FindObjectFromInstanceID(id));
                }
            }
            return nullptr;
        }
        T* operator->() const { return get(); }
        [[nodiscard]] explicit operator Bool() const { return m_uuid.isValid(); }
        [[nodiscard]] Bool isValid() const { return m_uuid.isValid(); }

        [[nodiscard]] const FileID& getFileID() const { return m_file_id; }
        [[nodiscard]] const UUID& getUUID() const { return m_uuid; }
        [[nodiscard]] InstanceID getInstanceID() const { return m_instance_id; }
    };

} // dodoe
