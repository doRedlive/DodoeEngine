// do@Redlive

#include "pso_disk_cache.h"

#include "runtime/resource/file/file_system.h"

#include <fstream>

namespace dodoe {

    Bool PsoDiskCache::beginLoad(const String& cache_path, UInt64 expected_shader_hash) {
        m_cache_path = cache_path;
        m_shader_hash = expected_shader_hash;
        m_loaded = false;
        m_cache_data.clear();
        m_pending_writes.clear();

        auto full_path = FileSystem::GetEngineResPath() / cache_path;
        std::ifstream in(full_path, std::ios::binary);
        if (!in.is_open()) {
            DO_INFO("PsoDiskCache::beginLoad no existing cache at {}", full_path.string());
            m_loaded = true;
            return true;
        }

        UInt32 magic = 0;
        UInt32 version = 0;
        UInt64 file_shader_hash = 0;
        UInt64 entry_count = 0;

        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        in.read(reinterpret_cast<char*>(&file_shader_hash), sizeof(file_shader_hash));
        in.read(reinterpret_cast<char*>(&entry_count), sizeof(entry_count));

        if (magic != kMagic || version != kVersion) {
            DO_INFO("PsoDiskCache::beginLoad invalid header, discarding cache");
            m_loaded = true;
            return true;
        }

        if (file_shader_hash != expected_shader_hash) {
            DO_INFO("PsoDiskCache::beginLoad shader hash mismatch, discarding cache");
            m_loaded = true;
            return true;
        }

        m_cache_data.reserve(static_cast<Size_t>(entry_count));

        for (UInt64 i = 0; i < entry_count; ++i) {
            EntryHeader header{};
            in.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (!in) {
                break;
            }

            DynamicArray<UInt8> data(static_cast<Size_t>(header.data_size));
            in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(header.data_size));
            if (!in) {
                break;
            }

            m_cache_data[header.key_hash] = std::move(data);
        }

        m_loaded = true;
        DO_INFO("PsoDiskCache::beginLoad loaded {} entries from {}", m_cache_data.size(), full_path.string());
        return true;
    }

    DynamicArray<UInt8>* PsoDiskCache::find(UInt64 key_hash) {
        auto it = m_cache_data.find(key_hash);
        if (it != m_cache_data.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void PsoDiskCache::insert(UInt64 key_hash, const DynamicArray<UInt8>& pso_blob) {
        m_cache_data[key_hash] = pso_blob;
        m_pending_writes.push_back({key_hash, pso_blob});
    }

    void PsoDiskCache::flushToDisk() {
        if (m_pending_writes.empty() || m_cache_path.empty()) {
            return;
        }

        auto full_path = FileSystem::GetEngineResPath() / m_cache_path;
        std::ofstream out(full_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            DO_ERROR("PsoDiskCache::flushToDisk failed to open {}", full_path.string());
            return;
        }

        UInt32 magic = kMagic;
        UInt32 version = kVersion;
        UInt64 entry_count = static_cast<UInt64>(m_cache_data.size());

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&m_shader_hash), sizeof(m_shader_hash));
        out.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));

        for (const auto& [key_hash, data] : m_cache_data) {
            EntryHeader header{};
            header.key_hash = key_hash;
            header.data_size = static_cast<UInt64>(data.size());
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }

        m_pending_writes.clear();
        DO_INFO("PsoDiskCache::flushToDisk wrote {} entries to {}", m_cache_data.size(), full_path.string());
    }

    void PsoDiskCache::invalidateAll() {
        m_cache_data.clear();
        m_pending_writes.clear();
    }

} // namespace dodoe
