// do@Redlive

#pragma once

#include <cstdint>

#include "runtime/core/utils/uuid.h"
#include "object_id.h"

namespace dodoe {

    class Object {
        ObjectID m_id{};
        InstanceID m_instance_id{0};
        UInt32 m_generation{0};
        Bool m_registered{false};
        String m_name{};

        static UnorderedMap<InstanceID, Object*> s_instance_map;
        static UnorderedMap<UInt64, InstanceID> s_ref_to_instance;
        static UnorderedMap<InstanceID, UInt32> s_generations;
        static InstanceID s_next_instance_id;

    protected:
        Object() = default;
        explicit Object(const ObjectID& id);

        static UInt64 makeKey(const ObjectID& id);

    public:
        virtual ~Object();

        [[nodiscard]] InstanceID getInstanceID() const { return m_instance_id; }
        [[nodiscard]] UInt32 getGeneration() const { return m_generation; }
        [[nodiscard]] const ObjectID& getID() const { return m_id; }
        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] ObjectHandle getHandle() const { return ObjectHandle{m_instance_id, m_generation}; }

        void setName(const String& n) { m_name = n; }

        [[nodiscard]] static Object* FindObjectFromInstanceID(InstanceID id);
        [[nodiscard]] static InstanceID FindInstanceID(const ObjectID& id);
        [[nodiscard]] static Bool isAlive(InstanceID id, UInt32 generation);

        static InstanceID AllocateInstanceID(Object* obj);
        static void ReleaseInstanceID(InstanceID id);

        [[nodiscard]] static const auto& GetInstanceMap() { return s_instance_map; }

        [[nodiscard]] virtual const char* getObjectTypeName() const = 0;
    };

} // namespace dodoe
