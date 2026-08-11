// do@Redlive

#include "file_id.h"
#include "file_system.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    UInt64 FileID::computeID(const String& path, const UUID& uuid) {
        const UInt64 uuid_val = static_cast<UInt64>(uuid);
        if (uuid_val != 0) {
            return uuid_val;
        }
        return static_cast<UInt64>(string2hash(path));
    }

    FileID::FileID(const String& path)
        : m_path(FileSystem::NormalizePath(path))
        , m_uuid(UUID(static_cast<UInt64>(string2hash(m_path))))
        , m_id(computeID(m_path, m_uuid)) {}

    FileID::FileID(const String& path, const UUID& uuid)
        : m_path(FileSystem::NormalizePath(path))
        , m_uuid(uuid)
        , m_id(computeID(m_path, m_uuid)) {}

} // dodoe
