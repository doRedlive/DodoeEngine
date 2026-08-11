// do@Redlive

#include "file_id.h"
#include "file_system.h"

#include <mutex>

namespace dodoe {

    namespace {

        class FileIDInternTable {
        public:
            UInt32 Make(const String& path) {
                std::lock_guard<std::mutex> lock(m_mutex);
                const auto it = m_path_to_id.find(path);
                if (it != m_path_to_id.end()) {
                    return it->second;
                }
                const UInt32 id = m_next_id++;
                m_path_to_id.emplace(path, id);
                m_id_to_path.emplace(id, path);
                return id;
            }

            [[nodiscard]] const String& PathOf(const UInt32 intern_id) const {
                std::lock_guard<std::mutex> lock(m_mutex);
                const auto it = m_id_to_path.find(intern_id);
                if (it != m_id_to_path.end()) {
                    return it->second;
                }
                return m_empty;
            }

        private:
            UnorderedMap<String, UInt32> m_path_to_id{};
            OrderedMap<UInt32, String> m_id_to_path{};
            String m_empty{};
            UInt32 m_next_id{1};
            mutable std::mutex m_mutex{};
        };

        FileIDInternTable& GetInternTable() {
            static FileIDInternTable table{};
            return table;
        }
    }

    FileID FileID::Make(const String& path) {
        FileID result;
        result.m_intern_id = GetInternTable().Make(FileSystem::NormalizePath(path));
        return result;
    }

    const String& FileID::getPath() const {
        return GetInternTable().PathOf(m_intern_id);
    }

} // dodoe
