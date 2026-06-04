// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    class Object {
        FileID m_file_id{};
        UUID m_uuid{};
        InstanceID m_instance_id{0};
        String m_name{};

        static UnorderedMap<InstanceID, Object*> s_instance_map;
        static UnorderedMap<UInt64, InstanceID> s_id_to_instance;
        static InstanceID s_next_instance_id;

    protected:
        Object() = default;

        static UInt64 makeKey(const FileID& file_id);

    public:
        virtual ~Object() = default;

        [[nodiscard]] InstanceID getInstanceID() const { return m_instance_id; }
        [[nodiscard]] const FileID& getFileID() const { return m_file_id; }
        [[nodiscard]] const UUID& getUUID() const { return m_uuid; }
        [[nodiscard]] const String& getName() const { return m_name; }

        void setName(const String& n) { m_name = n; }
        void setFileIdentity(const FileID& file_id, const UUID& uuid) { m_file_id = file_id; m_uuid = uuid; }

        [[nodiscard]] static Object* FindObjectFromInstanceID(InstanceID id);
        [[nodiscard]] static InstanceID FindInstanceID(const FileID& file_id);
        [[nodiscard]] static InstanceID AllocateInstanceID(Object* obj);
        static void ReleaseInstanceID(InstanceID id);

        [[nodiscard]] virtual const char* getObjectTypeName() const = 0;
    };

} // dodoe
